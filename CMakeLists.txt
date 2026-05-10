cmake_minimum_required(VERSION 3.25)

project(
    meet_sentinel
    VERSION 0.1.0
    DESCRIPTION "A local tray app for reliable Google Calendar meeting reminders"
    LANGUAGES CXX
)

option(MEET_SENTINEL_BUILD_APP "Build the Qt tray application when Qt is available" ON)
option(MEET_SENTINEL_ALLOW_SYSTEM_QT "Allow using a system Qt instead of the vendored Qt prefix" OFF)
set(MEET_SENTINEL_QT_PREFIX
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qt-install"
    CACHE PATH
    "Installed prefix for the vendored static Qt build"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/nlohmann_json EXCLUDE_FROM_ALL)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_SRP ON CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(PICKY_COMPILER OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
endif()

set(_meet_sentinel_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(third_party/curl EXCLUDE_FROM_ALL)
set(BUILD_TESTING "${_meet_sentinel_build_testing}")

add_library(meet_sentinel_core
    src/core/reminder_engine.cpp
)
target_include_directories(meet_sentinel_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(meet_sentinel_core PUBLIC cxx_std_20)

add_library(meet_sentinel_persistence
    src/persistence/credential_store.cpp
    src/persistence/file_logger.cpp
    src/persistence/json_persistence.cpp
)
target_include_directories(meet_sentinel_persistence
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(meet_sentinel_persistence PUBLIC cxx_std_20)
target_link_libraries(meet_sentinel_persistence
    PUBLIC
        meet_sentinel_core
        nlohmann_json::nlohmann_json
)

add_library(meet_sentinel_google
    src/google/calendar_client.cpp
    src/google/curl_http_client.cpp
    src/google/http_client.cpp
    src/google/loopback_authorization_server.cpp
    src/google/oauth_client.cpp
    src/google/pkce.cpp
    src/google/url_encoding.cpp
)
target_include_directories(meet_sentinel_google
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(meet_sentinel_google PUBLIC cxx_std_20)
target_link_libraries(meet_sentinel_google
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
)
if(WIN32)
    target_link_libraries(meet_sentinel_google PRIVATE ws2_32)
endif()

if(MEET_SENTINEL_BUILD_APP)
    set(_meet_sentinel_qt_found FALSE)

    if(MEET_SENTINEL_QT_PREFIX AND EXISTS "${MEET_SENTINEL_QT_PREFIX}")
        list(PREPEND CMAKE_PREFIX_PATH "${MEET_SENTINEL_QT_PREFIX}")
        find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
        set(_meet_sentinel_qt_found "${Qt6_FOUND}")
    endif()

    if(NOT _meet_sentinel_qt_found AND MEET_SENTINEL_ALLOW_SYSTEM_QT)
        find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
        set(_meet_sentinel_qt_found "${Qt6_FOUND}")
    endif()

    if(_meet_sentinel_qt_found)
        qt_add_executable(meet-sentinel
            WIN32
            src/app/main.cpp
        )
        target_link_libraries(meet-sentinel
            PRIVATE
                meet_sentinel_core
                meet_sentinel_persistence
                meet_sentinel_google
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
        )

        if(COMMAND qt_import_plugins)
            qt_import_plugins(meet-sentinel
                EXCLUDE_BY_TYPE imageformats
                EXCLUDE_BY_TYPE sqldrivers
            )
        endif()
    else()
        message(WARNING
            "Qt6 was not found from MEET_SENTINEL_QT_PREFIX='${MEET_SENTINEL_QT_PREFIX}'. "
            "Skipping the Qt app target. Build/install vendored Qt or set "
            "MEET_SENTINEL_ALLOW_SYSTEM_QT=ON for local experiments."
        )
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(meet_sentinel_core_tests
        tests/core/reminder_engine_test.cpp
    )
    target_link_libraries(meet_sentinel_core_tests PRIVATE meet_sentinel_core)
    add_test(NAME meet_sentinel_core_tests COMMAND meet_sentinel_core_tests)

    add_executable(meet_sentinel_persistence_tests
        tests/persistence/persistence_test.cpp
    )
    target_link_libraries(meet_sentinel_persistence_tests PRIVATE meet_sentinel_persistence)
    add_test(NAME meet_sentinel_persistence_tests COMMAND meet_sentinel_persistence_tests)

    add_executable(meet_sentinel_google_tests
        tests/google/google_client_test.cpp
    )
    target_link_libraries(meet_sentinel_google_tests PRIVATE meet_sentinel_google)
    add_test(NAME meet_sentinel_google_tests COMMAND meet_sentinel_google_tests)
endif()
