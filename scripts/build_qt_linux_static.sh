#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_source="${QT_SOURCE:-${repo_root}/third_party/qt}"
qt_build="${QT_BUILD_DIR:-${repo_root}/build/qt-linux-static}"
qt_install="${QT_INSTALL_PREFIX:-${repo_root}/third_party/qt-install}"
qt_build_parallelism="${QT_BUILD_PARALLELISM:-}"
qt_qpa_backends="${QT_QPA_BACKENDS:-xcb;wayland}"
qt_default_qpa="${QT_DEFAULT_QPA:-xcb}"

if [[ ! -x "${qt_source}/configure" ]]; then
    echo "Qt source tree is not initialized at ${qt_source}." >&2
    echo "Run scripts/bootstrap_qt_submodule.sh first." >&2
    exit 1
fi

cmake -E make_directory "${qt_build}"

(
    cd "${qt_build}"
    "${qt_source}/configure" \
        -opensource \
        -confirm-license \
        -release \
        -static \
        -no-pch \
        -dbus-runtime \
        -qpa "${qt_qpa_backends}" \
        -default-qpa "${qt_default_qpa}" \
        -qt-doubleconversion \
        -qt-harfbuzz \
        -qt-libjpeg \
        -qt-libpng \
        -qt-pcre \
        -qt-zlib \
        -no-evdev \
        -no-glib \
        -no-icu \
        -no-libinput \
        -no-gtk \
        -no-tslib \
        -no-feature-eglfs \
        -no-feature-kms \
        -no-feature-linuxfb \
        -no-feature-tuiotouch \
        -no-feature-vnc \
        -no-feature-vulkan \
        -no-feature-zstd \
        -no-feature-androiddeployqt \
        -no-feature-accessibility-atspi-bridge \
        -no-feature-printsupport \
        -no-feature-sql \
        -no-feature-testlib \
        -no-feature-wasmdeployqt \
        -prefix "${qt_install}" \
        -nomake examples \
        -nomake tests \
        "$@"
    if [[ -n "${qt_build_parallelism}" ]]; then
        cmake --build . --parallel "${qt_build_parallelism}"
    else
        cmake --build . --parallel
    fi
    cmake --install .
)

echo "Installed static Qt to ${qt_install}."
