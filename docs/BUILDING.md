# Building

The repository is intended to build in stages.

The Qt-free core library and core tests should configure without Qt:

```sh
cmake --preset core-dev
cmake --build --preset core-dev
ctest --preset core-dev
```

## Vendored Qt

Qt should be added as a pinned submodule under `third_party/qt`.

The current project pin is Qt `v6.11.0`.

Preferred setup:

```sh
scripts/bootstrap_qt_submodule.sh
```

Manual equivalent:

```sh
git submodule add https://code.qt.io/qt/qt5.git third_party/qt
git -C third_party/qt fetch --tags origin
git -C third_party/qt switch --detach v6.11.0
third_party/qt/init-repository --module-subset=qtbase,qtwayland
```

The upstream repository is named `qt5.git` even for Qt 6.

## Static Qt Build

Build Qt out of source and install it into an ignored prefix under `third_party/qt-install`.

Linux development example:

```sh
scripts/build_qt_linux_static.sh
```

The Linux script intentionally disables PCH and several unused Qt features to keep the static build smaller:
SQL, PrintSupport, QtTest, GTK/GLib integration, ICU, Vulkan, embedded Linux platform plugins, input-device plugins, and Android/WASM deployment tools.
It also asks QtBase to use bundled copies for small dependencies such as zlib, PCRE, HarfBuzz, libpng, libjpeg, and double-conversion.
The default Linux build includes both XCB and Wayland QPA backends, with XCB kept as the default backend.
Extra Qt configure arguments can be passed after the script name if this host needs local feature flags.
Set `QT_BUILD_PARALLELISM` to limit build parallelism on machines with tight disk or memory headroom.
On Linux this is a development toolchain, not the final dependency-minimized release target: Qt still links against host desktop integration libraries such as XCB, Wayland, EGL/OpenGL, xkbcommon, and Freetype.
On Arch-like systems, the Wayland-enabled build expects host packages such as `wayland`, `wayland-protocols`, `libxkbcommon`, `mesa`, and `libglvnd` to be installed.

To rebuild an existing checkout after changing the Qt module subset or QPA backends:

```sh
scripts/bootstrap_qt_submodule.sh
rm -rf build/qt-linux-static third_party/qt-install
QT_BUILD_PARALLELISM=4 scripts/build_qt_linux_static.sh
cmake --preset app-vendored-qt --fresh
cmake --build --preset app-vendored-qt
ctest --preset app-vendored-qt
```

To smoke-test the Wayland backend on a Wayland session:

```sh
QT_QPA_PLATFORM=wayland build/app-vendored-qt/meet-sentinel
```

Windows release direction from a suitable MSVC developer shell:

```powershell
scripts\build_qt_windows_static.ps1
```

The Windows script uses static runtime linking, disables PCH and unused Qt features, and prefers Schannel over OpenSSL.

Then configure the app against the vendored Qt prefix:

```sh
cmake --preset app-vendored-qt
cmake --build --preset app-vendored-qt
ctest --preset app-vendored-qt
```

`MEET_SENTINEL_ALLOW_SYSTEM_QT=ON` exists only for local experiments before the vendored Qt build is ready.
