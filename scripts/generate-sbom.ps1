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
        downloadLocation      = if ($Definition.downloadLocation) { $Definition.downloadLocation } else { "NOASSERTION" }
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
        downloadLocation      = "git+https://github.com/r-lyeh-archived/base@9c50c57b46b9be1b028134a65e3f12d40516e9b1"
        licenseDeclared       = "Zlib"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/base91"
        sourceInfo            = "Vendored source at src/third_party_lib/base91; base.hpp matches upstream revision 9c50c57b46b9be1b028134a65e3f12d40516e9b1."
        comment               = "Version is declared by BASE_VERSION in base.hpp; upstream repository is archived."
    },
    @{
        SPDXID                = "SPDXRef-cxxopts"
        name                  = "cxxopts"
        versionInfo           = "2.2.1"
        downloadLocation      = "git+https://github.com/jarro2783/cxxopts@302302b30839505703d37fb82f536c53cf9172fa"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/cxxopts"
        sourceInfo            = "Vendored source at src/third_party_lib/cxxopts; cxxopts.hpp matches upstream tag v2.2.1 at revision 302302b30839505703d37fb82f536c53cf9172fa."
        comment               = "The upstream v2.2.1 tag retains 2.2.0 in CXXOPTS__VERSION_*; source matching and repository history establish the tag."
    },
    @{
        SPDXID                = "SPDXRef-intx"
        name                  = "intx"
        versionInfo           = "0.9.3"
        downloadLocation      = "git+https://github.com/chfast/intx@4c1ca55d78777ffea7ede46e70cbd46a5beef008"
        licenseDeclared       = "Apache-2.0"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/intx"
        sourceInfo            = "Vendored source at src/third_party_lib/intx; intx.hpp matches upstream tag v0.9.3 at revision 4c1ca55d78777ffea7ede46e70cbd46a5beef008."
        comment               = "The importing libcimbar commit records the v0.9.3 release URL."
    },
    @{
        SPDXID                = "SPDXRef-libcorrect"
        name                  = "libcorrect"
        versionInfo           = "git-f5a28c74fba7"
        downloadLocation      = "git+https://github.com/quiet/libcorrect@f5a28c74fba7a99736fe49d3a5243eca29517ae9"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/libcorrect"
        sourceInfo            = "Vendored source at src/third_party_lib/libcorrect from upstream revision f5a28c74fba7a99736fe49d3a5243eca29517ae9."
        comment               = "The git-subtree metadata in import commit a291e6ba23e8f5d7f8c6f40cfede2b5a2b5cea20 records this upstream split."
    },
    @{
        SPDXID                = "SPDXRef-libpopcnt"
        name                  = "libpopcnt"
        versionInfo           = "3.1"
        downloadLocation      = "git+https://github.com/kimwalisch/libpopcnt@5214d3fba1dcebd7ea36f0aed2731549d16d7df9"
        licenseDeclared       = "BSD-2-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/libpopcnt"
        sourceInfo            = "Vendored source at src/third_party_lib/libpopcnt; libpopcnt.h matches upstream tag v3.1 at revision 5214d3fba1dcebd7ea36f0aed2731549d16d7df9."
        comment               = "The importing libcimbar commit records the v3.1 release URL."
    },
    @{
        SPDXID                = "SPDXRef-Wirehair"
        name                  = "Wirehair"
        versionInfo           = "git-0d8b51da63c4+libcimbar (ABI 2)"
        downloadLocation      = "git+https://github.com/catid/wirehair@0d8b51da63c4f146112b46f225ffa34ac1183f16"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/wirehair"
        sourceInfo            = "Vendored source at src/third_party_lib/wirehair based on upstream revision 0d8b51da63c4f146112b46f225ffa34ac1183f16 with reviewed libcimbar build and security patches."
        comment               = "All upstream source files in libcimbar import commit 2440853356506dd65b2e7849ee69fd89a764eec9 match the recorded revision; the current tree digest captures local divergence."
    },
    @{
        SPDXID                = "SPDXRef-zstd"
        name                  = "zstd"
        versionInfo           = "1.5.5"
        downloadLocation      = "git+https://github.com/facebook/zstd@63779c798237346c2b245c546c40b72a5a5913fe"
        licenseDeclared       = "BSD-3-Clause"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/zstd"
        sourceInfo            = "Vendored source at src/third_party_lib/zstd; zstd.h matches upstream tag v1.5.5 at revision 63779c798237346c2b245c546c40b72a5a5913fe."
        comment               = "Version is declared by ZSTD_VERSION_* in zstd.h. The source also contains GPL-2.0 text; the project build uses the BSD-3-Clause option declared in LICENSE."
    },
    @{
        SPDXID                = "SPDXRef-PicoSHA2"
        name                  = "PicoSHA2"
        versionInfo           = "git-7bfa26156981"
        downloadLocation      = "git+https://github.com/okdshin/PicoSHA2@7bfa26156981f7181f240906495a2c33c7fa48be"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/PicoSHA2"
        sourceInfo            = "Vendored source at src/third_party_lib/PicoSHA2; picosha2.h matches upstream revision 7bfa26156981f7181f240906495a2c33c7fa48be."
        comment               = "Used by tests only; upstream does not publish numbered releases for this snapshot."
    },
    @{
        SPDXID                = "SPDXRef-fmt"
        name                  = "fmt"
        versionInfo           = "12.1.0"
        downloadLocation      = "git+https://github.com/fmtlib/fmt@407c905e45ad75fc29bf0f9bb7c5c2fd3475976f"
        licenseDeclared       = "MIT"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/fmt"
        sourceInfo            = "Vendored source at src/third_party_lib/fmt; base.h matches upstream tag 12.1.0 at revision 407c905e45ad75fc29bf0f9bb7c5c2fd3475976f."
        comment               = "Version is declared by FMT_VERSION in base.h."
    },
    @{
        SPDXID                = "SPDXRef-concurrentqueue"
        name                  = "concurrentqueue"
        versionInfo           = "1.0.4"
        downloadLocation      = "git+https://github.com/cameron314/concurrentqueue@6dd38b8a1dbaa7863aa907045f32308a56a6ff5d"
        licenseDeclared       = "BSD-2-Clause OR BSL-1.0"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/concurrentqueue"
        sourceInfo            = "Vendored source at src/third_party_lib/concurrentqueue; concurrentqueue.h matches upstream tag v1.0.4 at revision 6dd38b8a1dbaa7863aa907045f32308a56a6ff5d."
        comment               = "The importing libcimbar commit records version 1.0.4."
    },
    @{
        SPDXID                = "SPDXRef-stb_image"
        name                  = "stb_image"
        versionInfo           = "2.30"
        downloadLocation      = "git+https://github.com/nothings/stb@31c1ad37456438565541f4919958214b6e762fb4"
        licenseDeclared       = "MIT OR Unlicense"
        primaryPackagePurpose = "LIBRARY"
        relativePath          = "src/third_party_lib/stb"
        sourceInfo            = "Vendored source at src/third_party_lib/stb; stb_image.h exactly matches upstream revision 31c1ad37456438565541f4919958214b6e762fb4."
        comment               = "Production-reachable embedded-image decoder upgraded from v2.27 after OSV identified OSV-2020-1372 and OSV-2021-1239; v2.30 is declared in the header banner."
    },
    @{
        SPDXID                = "SPDXRef-Catch2"
        name                  = "Catch2"
        versionInfo           = "2.13.8"
        downloadLocation      = "git+https://github.com/catchorg/Catch2@216713a4066b79d9803d374f261ccb30c0fb451f"
        licenseDeclared       = "BSL-1.0"
        primaryPackagePurpose = "TEST"
        relativePath          = "test/catch.hpp"
        sourceInfo            = "Vendored single-header test framework at test/catch.hpp; catch.hpp matches upstream tag v2.13.8 at revision 216713a4066b79d9803d374f261ccb30c0fb451f."
        comment               = "Test-only dependency. Version is declared by CATCH_VERSION_* in the checked-in single header."
    }
)

$externalPackages = @(
    @{
        SPDXID                = "SPDXRef-OpenCV"
        name                  = "OpenCV"
        versionInfo           = "4.11.0#4 (vcpkg)"
        downloadLocation      = "git+https://github.com/opencv/opencv@31b0eeea0b44b370fd0712312df4214d4ae1b158"
        licenseDeclared       = "Apache-2.0"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Compatibility-apps dependency resolved by find_package(OpenCV REQUIRED); vcpkg baseline 6ecbbbdf31cba47aafa7cf6189b1e73e10ac61f8 selects opencv4 4.11.0#4 from upstream tag 4.11.0 with only calib3d, JPEG, PNG, and thread features."
        comment               = "Excluded from hardened-transport-only configuration. Seven known OpenCV findings have reviewed, expiring reachability exceptions after production file decoding was moved to bounded stb_image."
    },
    @{
        SPDXID                = "SPDXRef-glfw3"
        name                  = "glfw3"
        versionInfo           = "3.4#1 (vcpkg)"
        downloadLocation      = "git+https://github.com/glfw/glfw@7b6aead9fb88b3623e3b3725ebb42670cbe4c579"
        licenseDeclared       = "Zlib"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Compatibility-apps dependency resolved by find_package(glfw3 CONFIG REQUIRED) for Windows GUI tools; vcpkg baseline 6ecbbbdf31cba47aafa7cf6189b1e73e10ac61f8 selects glfw3 3.4#1 from upstream tag 3.4."
        comment               = "Excluded from hardened-transport-only configuration. Linux system packages remain distribution-selected."
    },
    @{
        SPDXID                = "SPDXRef-OpenGL-Registry"
        name                  = "OpenGL Registry"
        versionInfo           = "2024-02-10#1 (vcpkg)"
        downloadLocation      = "git+https://github.com/KhronosGroup/OpenGL-Registry@3530768138c5ba3dfbb2c43c830493f632f7ea33"
        licenseDeclared       = "NOASSERTION"
        primaryPackagePurpose = "LIBRARY"
        sourceInfo            = "Compatibility-apps dependency resolved by find_path(GLES3/gl3.h); vcpkg baseline 6ecbbbdf31cba47aafa7cf6189b1e73e10ac61f8 selects upstream revision 3530768138c5ba3dfbb2c43c830493f632f7ea33."
        comment               = "Excluded from hardened-transport-only configuration. Linux system packages remain distribution-selected."
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
        foreach ($property in @("name", "versionInfo", "downloadLocation", "licenseDeclared", "sourceInfo")) {
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
