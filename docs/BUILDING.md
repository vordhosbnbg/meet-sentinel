# Building

The repository is intended to build in stages.

The Qt-free core library and core tests should configure without Qt:

```sh
cmake -S . -B build/dev
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

## Vendored Qt

Qt should be added as a pinned submodule under `third_party/qt`.

Example, using a Qt 6 release tag:

```sh
git submodule add --branch v6.11.0 https://code.qt.io/qt/qt5.git third_party/qt
cd third_party/qt
./init-repository --module-subset=qtbase
```

The upstream repository is named `qt5.git` even for Qt 6.

## Static Qt Build

Build Qt out of source and install it into an ignored prefix under `third_party/qt-install`.

Linux development example:

```sh
cmake -E make_directory build/qt-linux-static
cd build/qt-linux-static
../../third_party/qt/configure -release -static -prefix ../../third_party/qt-install -nomake examples -nomake tests
cmake --build . --parallel
cmake --install .
```

Windows release direction from a suitable MSVC developer shell:

```bat
cmake -E make_directory build\qt-windows-msvc-static
cd build\qt-windows-msvc-static
..\..\third_party\qt\configure.bat -release -static -static-runtime -prefix ..\..\third_party\qt-install -nomake examples -nomake tests
cmake --build . --parallel
cmake --install .
```

Then configure the app against the vendored Qt prefix:

```sh
cmake -S . -B build/app -DMEET_SENTINEL_QT_PREFIX="$PWD/third_party/qt-install"
cmake --build build/app
```

`MEET_SENTINEL_ALLOW_SYSTEM_QT=ON` exists only for local experiments before the vendored Qt build is ready.
