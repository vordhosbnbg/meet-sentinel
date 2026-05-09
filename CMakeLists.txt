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

add_library(meet_sentinel_core
    src/core/reminder_engine.cpp
)
target_include_directories(meet_sentinel_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(meet_sentinel_core PUBLIC cxx_std_20)

if(MEET_SENTINEL_BUILD_APP)
    set(_meet_sentinel_qt_found FALSE)

    if(MEET_SENTINEL_QT_PREFIX AND EXISTS "${MEET_SENTINEL_QT_PREFIX}")
        list(PREPEND CMAKE_PREFIX_PATH "${MEET_SENTINEL_QT_PREFIX}")
        find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network)
        set(_meet_sentinel_qt_found "${Qt6_FOUND}")
    endif()

    if(NOT _meet_sentinel_qt_found AND MEET_SENTINEL_ALLOW_SYSTEM_QT)
        find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network)
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
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
                Qt6::Network
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
endif()
