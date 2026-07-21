param(
    [string]$VcpkgRoot = "$env:USERPROFILE\vcpkg",
    [string]$Triplet = "x64-mingw-dynamic",
    [string]$BuildDir = "out/build/windows-mingw-vcpkg"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
$toolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"

if (-not (Test-Path -LiteralPath $vcpkg)) {
    throw "vcpkg.exe not found at $vcpkg"
}
if (-not (Test-Path -LiteralPath $toolchain)) {
    throw "vcpkg CMake toolchain not found at $toolchain"
}

Push-Location $repoRoot
try {
    git config --local core.autocrlf false
    git config --local core.eol lf
    git submodule update --init --recursive

    $license = Join-Path $repoRoot "LICENSE"
    $licenseText = [System.IO.File]::ReadAllText($license)
    $normalized = $licenseText -replace "`r`n", "`n"
    if ($normalized -ne $licenseText) {
        [System.IO.File]::WriteAllText($license, $normalized, (New-Object System.Text.UTF8Encoding($false)))
    }

    & $vcpkg install opencv4 glfw3 opengl-registry --triplet $Triplet

    cmake --fresh -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=RelWithDebInfo `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DVCPKG_TARGET_TRIPLET="$Triplet" `
        -DLIBCIMBAR_BUILD_GUI_TOOLS=OFF

    cmake --build $BuildDir --parallel 4
    ctest --test-dir $BuildDir --output-on-failure
}
finally {
    Pop-Location
}
