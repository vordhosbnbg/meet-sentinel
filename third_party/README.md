# Third-Party Dependencies

All third-party source dependencies belong here as pinned git submodules.

Planned submodules:

- `qt`: Qt supermodule, pinned to a Qt 6 release tag. Initialize with the `qtbase` module subset first.
- `nlohmann_json`: JSON library for settings, cache, and fired reminder state once persistence begins.
- `doctest` or `Catch2`: test framework if plain CTest executables stop being enough.

Do not commit dependency build directories, install prefixes, package caches, or generated binaries.
