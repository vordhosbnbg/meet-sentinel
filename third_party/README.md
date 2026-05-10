# Third-Party Dependencies

All third-party source dependencies belong here as pinned git submodules.

Current submodules:

- `qt`: Qt supermodule, pinned to `v6.11.0`. Initialize with `qtbase,qtwayland` for Widgets, Network, XCB, and Wayland platform support.
- `nlohmann_json`: JSON library, pinned to `v3.12.0`, for settings, cache, fired reminder state, and credential development fallback schemas.

Potential future submodules:

- `doctest` or `Catch2`: test framework if plain CTest executables stop being enough.

Do not commit dependency build directories, install prefixes, package caches, or generated binaries.
