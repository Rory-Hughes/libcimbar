[CmdletBinding()]
param(
    [string]$OutputPath = "docs/security/sbom/libcimbar.spdx.json",
    [switch]$Check
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$outputFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputPath))

function Get-TreeDigest {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $targetPath = Join-Path $repoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $targetPath)) {
        throw "SBOM package path does not exist: $RelativePath"
    }

    $target = Get-Item -LiteralPath $targetPath
    if ($target.PSIsContainer) {
        $files = @(Get-ChildItem -LiteralPath $target.FullName -File -Recurse |
            Sort-Object -Property FullName)
    }
    else {
        $files = @($target)
    }

    $records = New-Object System.Text.StringBuilder
    foreach ($file in $files) {
        if ($target.PSIsContainer) {
            $relativeFile = $file.FullName.Substring($target.FullName.Length).TrimStart('\', '/') -replace '\\', '/'
        }
        else {
            $relativeFile = $file.Name
        }

        $fileDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        [void]$records.Append($relativeFile).Append([char]0).Append($fileDigest).Append([System.Environment]::NewLine)
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($records.ToString())
        return (($sha256.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $sha256.Dispose()
    }
}

function New-Package {
    param([Parameter(Mandatory = $true)][hashtable]$Definition)

    $package = [ordered]@{
        SPDXID                = $Definition.SPDXID
        name                  = $Definition.name
        versionInfo           = $Definition.versionInfo
        downloadLocation      = "NOASSERTION"
        filesAnalyzed         = $false
        licenseConcluded      = "NOASSERTION"
        licenseDeclared       = $Definition.licenseDeclared
        copyrightText         = "NOASSERTION"
        primaryPackagePurpose = $Definition.primaryPackagePurpose
        sourceInfo            = $Definition.sourceInfo
        comment               = $Definition.comment
    }

    if ($Definition.ContainsKey("relativePath")) {
        $package.checksums = @(
            [ordered]@{
                algorithm     = "SHA256"
                checksumValue = Get-TreeDigest $Definition.relativePath
            }
        )
    }

    return $package
}

$head = (& git -C $repoRoot rev-parse --verify HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($head)) {
    throw "Could not resolve the repository HEAD."
}

$worktreeStatus = & git -C $repoRoot status --porcelain 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Could not determine the repository worktree state."
}
$worktreeState = if ($worktreeStatus) { "dirty" } else { "clean" }

$vendoredPackages = @(
    @{
        SPDXID                = "SPDXRef-base91"
        name                  = "base91"
        versionInfo           = "1.0.2"
        licenseDeclared       = "Zlib"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/base91"
        sourceInfo            = "Vendored source at src/third_party_lib/base91."
        comment               = "Version is declared by BASE_VERSION in base.hpp."
    },
    @{
        SPDXID                = "SPDXRef-cxxopts"
        name                  = "cxxopts"
        versionInfo           = "2.2.0"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/cxxopts"
        sourceInfo            = "Vendored source at src/third_party_lib/cxxopts."
        comment               = "Version is declared by CXXOPTS__VERSION_* in cxxopts.hpp."
    },
    @{
        SPDXID                = "SPDXRef-intx"
        name                  = "intx"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "Apache-2.0"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/intx"
        sourceInfo            = "Vendored source at src/third_party_lib/intx."
        comment               = "No upstream revision or release tag is recorded in this repository."
    },
    @{
        SPDXID                = "SPDXRef-libcorrect"
        name                  = "libcorrect"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/libcorrect"
        sourceInfo            = "Vendored source at src/third_party_lib/libcorrect."
        comment               = "No upstream revision or release tag is recorded in this repository."
    },
    @{
        SPDXID                = "SPDXRef-libpopcnt"
        name                  = "libpopcnt"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "BSD-2-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/libpopcnt"
        sourceInfo            = "Vendored source at src/third_party_lib/libpopcnt."
        comment               = "No upstream revision or release tag is recorded in this repository."
    },
    @{
        SPDXID                = "SPDXRef-Wirehair"
        name                  = "Wirehair"
        versionInfo           = "UNKNOWN (library ABI 2)"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/wirehair"
        sourceInfo            = "Vendored source at src/third_party_lib/wirehair."
        comment               = "The CMake target declares library ABI/SOVERSION 2; no upstream source revision is recorded."
    },
    @{
        SPDXID                = "SPDXRef-zstd"
        name                  = "zstd"
        versionInfo           = "1.5.5"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/zstd"
        sourceInfo            = "Vendored source at src/third_party_lib/zstd."
        comment               = "Version is declared by ZSTD_VERSION_* in zstd.h. The source also contains GPL-2.0 text; the project build uses the BSD-3-Clause option declared in LICENSE."
    },
    @{
        SPDXID                = "SPDXRef-PicoSHA2"
        name                  = "PicoSHA2"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/PicoSHA2"
        sourceInfo            = "Vendored source at src/third_party_lib/PicoSHA2."
        comment               = "Used by tests only. No upstream revision or release tag is recorded in this repository."
    },
    @{
        SPDXID                = "SPDXRef-fmt"
        name                  = "fmt"
        versionInfo           = "12.1.0"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/fmt"
        sourceInfo            = "Vendored source at src/third_party_lib/fmt."
        comment               = "Version is declared by FMT_VERSION in base.h."
    },
    @{
        SPDXID                = "SPDXRef-concurrentqueue"
        name                  = "concurrentqueue"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "BSD-2-Clause OR BSL-1.0"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/concurrentqueue"
        sourceInfo            = "Vendored source at src/third_party_lib/concurrentqueue."
        comment               = "No upstream revision or release tag is recorded in this repository."
    },
    @{
        SPDXID                = "SPDXRef-stb_image"
        name                  = "stb_image"
        versionInfo           = "2.27"
        licenseDeclared       = "MIT OR Unlicense"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/stb"
        sourceInfo            = "Vendored source at src/third_party_lib/stb."
        comment               = "Version is declared in the stb_image.h banner. Present in the repository but not included by the CMake targets reviewed for this inventory. No upstream revision is recorded."
    },
    @{
        SPDXID                = "SPDXRef-Catch2"
        name                  = "Catch2"
        versionInfo           = "2.13.8"
        licenseDeclared       = "BSL-1.0"
        primaryPackagePurpose = "TEST"
        relativePath          = "test/catch.hpp"
        sourceInfo            = "Vendored single-header test framework at test/catch.hpp."
        comment               = "Test-only dependency. Version is declared by CATCH_VERSION_* in the checked-in single header."
    }
)

$externalPackages = @(
    @{
        SPDXID                = "SPDXRef-OpenCV"
        name                  = "OpenCV"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "NOASSERTION"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Resolved by find_package(OpenCV REQUIRED) in the root CMakeLists.txt."
        comment               = "Not vendored or version-pinned by this repository. The Windows reference environment observed vcpkg opencv4 4.11.0#4."
    },
    @{
        SPDXID                = "SPDXRef-glfw3"
        name                  = "glfw3"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "NOASSERTION"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Resolved by find_package(glfw3 CONFIG REQUIRED) for Windows GUI tools."
        comment               = "Not vendored or version-pinned by this repository. The Windows reference environment observed vcpkg glfw3 3.4#1."
    },
    @{
        SPDXID                = "SPDXRef-OpenGL-Registry"
        name                  = "OpenGL Registry"
        versionInfo           = "UNKNOWN"
        licenseDeclared       = "NOASSERTION"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Resolved by find_path(GLES3/gl3.h) for Windows GUI tools."
        comment               = "Not vendored or version-pinned by this repository. The Windows reference environment observed vcpkg opengl-registry 2024-02-10#1."
    }
)

$packages = @(
    [ordered]@{
        SPDXID                = "SPDXRef-libcimbar"
        name                  = "libcimbar"
        versionInfo           = $head
        downloadLocation      = "git+https://github.com/Rory-Hughes/libcimbar@$head"
        filesAnalyzed         = $false
        licenseConcluded      = "MPL-2.0"
        licenseDeclared       = "MPL-2.0"
        copyrightText         = "NOASSERTION"
        primaryPackagePurpose = "APPLICATION"
        sourceInfo            = "Repository HEAD $head with a $worktreeState worktree at SBOM generation time."
        comment               = "This SBOM covers the direct source and build dependencies documented in docs/security/DEPENDENCY_INVENTORY.md."
    }
)

foreach ($definition in $vendoredPackages) {
    $packages += New-Package $definition
}
foreach ($definition in $externalPackages) {
    $packages += New-Package $definition
}

$relationships = @()
foreach ($definition in $vendoredPackages) {
    $relationships += [ordered]@{
        spdxElementId      = "SPDXRef-libcimbar"
        relationshipType   = "CONTAINS"
        relatedSpdxElement = $definition.SPDXID
        comment            = $definition.comment
    }
}
foreach ($definition in $externalPackages) {
    $relationships += [ordered]@{
        spdxElementId      = "SPDXRef-libcimbar"
        relationshipType   = "DEPENDS_ON"
        relatedSpdxElement = $definition.SPDXID
        comment            = $definition.comment
    }
}

$document = [ordered]@{
    spdxVersion       = "SPDX-2.3"
    dataLicense       = "CC0-1.0"
    SPDXID            = "SPDXRef-DOCUMENT"
    name              = "libcimbar direct dependency inventory"
    documentNamespace = "https://github.com/Rory-Hughes/libcimbar/spdx/$head"
    creationInfo      = [ordered]@{
        created  = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
        creators = @("Tool: scripts/generate-sbom.ps1")
        comment  = "The direct vendored dependency checksums are deterministic source-tree digests: SHA256 of sorted relative-path, NUL, file-SHA256 records."
    }
    documentDescribes = @("SPDXRef-libcimbar")
    packages          = $packages
    relationships     = $relationships
}

function Test-Sbom {
    param(
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)]$Expected
    )

    if ($Actual.spdxVersion -ne "SPDX-2.3" -or $Actual.dataLicense -ne "CC0-1.0") {
        throw "SBOM does not identify itself as SPDX 2.3 with CC0-1.0 data licensing."
    }
    if ($Actual.documentDescribes.Count -ne 1 -or $Actual.documentDescribes[0] -ne "SPDXRef-libcimbar") {
        throw "SBOM does not describe libcimbar."
    }
    if ($Actual.packages.Count -ne $Expected.packages.Count) {
        throw "SBOM package count does not match the generator catalog."
    }
    if ($Actual.relationships.Count -ne $Expected.relationships.Count) {
        throw "SBOM relationship count does not match the generator catalog."
    }

    $actualPackages = @{}
    foreach ($package in $Actual.packages) {
        if ($actualPackages.ContainsKey($package.SPDXID)) {
            throw "SBOM has a duplicate package ID: $($package.SPDXID)"
        }
        $actualPackages[$package.SPDXID] = $package
    }

    foreach ($expectedPackage in $Expected.packages) {
        if (-not $actualPackages.ContainsKey($expectedPackage.SPDXID)) {
            throw "SBOM is missing package $($expectedPackage.SPDXID)."
        }

        $actualPackage = $actualPackages[$expectedPackage.SPDXID]
        foreach ($property in @("name", "versionInfo", "licenseDeclared", "sourceInfo")) {
            if ($actualPackage.$property -ne $expectedPackage.$property) {
                throw "SBOM package $($expectedPackage.SPDXID) has stale $property."
            }
        }

        if ($expectedPackage.checksums) {
            if ($actualPackage.checksums.Count -ne 1 -or
                $actualPackage.checksums[0].algorithm -ne "SHA256" -or
                $actualPackage.checksums[0].checksumValue -ne $expectedPackage.checksums[0].checksumValue) {
                throw "SBOM package $($expectedPackage.SPDXID) has a stale source-tree digest."
            }
        }
    }

    $actualRelationships = @{}
    foreach ($relationship in $Actual.relationships) {
        $key = "$($relationship.spdxElementId)|$($relationship.relationshipType)|$($relationship.relatedSpdxElement)"
        $actualRelationships[$key] = $true
    }
    foreach ($expectedRelationship in $Expected.relationships) {
        $key = "$($expectedRelationship.spdxElementId)|$($expectedRelationship.relationshipType)|$($expectedRelationship.relatedSpdxElement)"
        if (-not $actualRelationships.ContainsKey($key)) {
            throw "SBOM is missing relationship $key."
        }
    }
}

if ($Check) {
    if (-not (Test-Path -LiteralPath $outputFullPath)) {
        throw "SBOM does not exist: $OutputPath"
    }

    $actual = Get-Content -LiteralPath $outputFullPath -Raw | ConvertFrom-Json
    Test-Sbom -Actual $actual -Expected $document
    Write-Host "SBOM check passed: $OutputPath"
    exit 0
}

$outputDirectory = Split-Path -Parent $outputFullPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$json = $document | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    $outputFullPath,
    "$json$([System.Environment]::NewLine)",
    (New-Object System.Text.UTF8Encoding($false))
)
Write-Host "Generated SPDX SBOM: $OutputPath"
