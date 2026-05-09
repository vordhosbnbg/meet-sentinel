param(
    [string[]] $ExtraConfigureArgs = @()
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$QtSource = if ($env:QT_SOURCE) { $env:QT_SOURCE } else { Join-Path $RepoRoot "third_party\qt" }
$QtBuild = if ($env:QT_BUILD_DIR) { $env:QT_BUILD_DIR } else { Join-Path $RepoRoot "build\qt-windows-msvc-static" }
$QtInstall = if ($env:QT_INSTALL_PREFIX) { $env:QT_INSTALL_PREFIX } else { Join-Path $RepoRoot "third_party\qt-install" }
$Configure = Join-Path $QtSource "configure.bat"

if (!(Test-Path $Configure)) {
    Write-Error "Qt source tree is not initialized at $QtSource. Run scripts/bootstrap_qt_submodule.sh or the equivalent git submodule/init-repository commands first."
}

cmake -E make_directory $QtBuild

Push-Location $QtBuild
try {
    & $Configure `
        -opensource `
        -confirm-license `
        -release `
        -static `
        -static-runtime `
        -no-pch `
        -no-opengl `
        -no-openssl `
        -schannel `
        -no-feature-androiddeployqt `
        -no-feature-printsupport `
        -no-feature-sql `
        -no-feature-testlib `
        -no-feature-wasmdeployqt `
        -prefix $QtInstall `
        -nomake examples `
        -nomake tests `
        @ExtraConfigureArgs
    cmake --build . --parallel
    cmake --install .
}
finally {
    Pop-Location
}

Write-Host "Installed static Qt to $QtInstall."
