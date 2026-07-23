# Dependency Inventory

This is the direct-dependency inventory for libcimbar. It is deliberately
conservative: every copied dependency is tied to an immutable upstream revision,
and external versions are named only where the pinned reference build resolves
them.

The machine-readable companion is sbom/libcimbar.spdx.json. Regenerate it with:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/generate-sbom.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/generate-sbom.ps1 -Check
python scripts/check-dependency-lock.py
~~~

-Check validates the SPDX envelope, catalog, relationships, and a deterministic
SHA-256 fingerprint of every vendored dependency tree. The dependency-lock check
also requires every OSV Git coordinate to match an SPDX download location and
validates the direct vcpkg dependency set.

## Scope

- First-party source: libcimbar, licensed MPL-2.0.
- Vendored libraries and the single-header test framework in this checkout.
- Direct CMake dependencies and the transitive packages selected by the pinned
  Windows vcpkg baseline. The transitive packages are not yet represented as
  individual SPDX packages.
- The samples git submodule is development data, not a libcimbar runtime
  dependency. The checked-out reference is
  9bbafc5ea0ad27e2a43ec9eee387dd60714a6c7f (heads/v0.6).

This is not a release SBOM. It should be regenerated from a clean,
release-candidate commit, alongside the final binary inventory.

## Vendored Source

| Component | Locked upstream revision | Version / evidence | License | Reachability / local divergence |
| --- | --- | --- | --- | --- |
| base91 | `r-lyeh-archived/base@9c50c57b46b9be1b028134a65e3f12d40516e9b1` | 1.0.2 (`BASE_VERSION`) | Zlib | base91 codec; header matches archived upstream |
| cxxopts | `jarro2783/cxxopts@302302b30839505703d37fb82f536c53cf9172fa` | tag v2.2.1; the upstream header still declares 2.2.0 | MIT | command-line tools; header matches upstream |
| intx | `chfast/intx@4c1ca55d78777ffea7ede46e70cbd46a5beef008` | tag v0.9.3 and importing commit | Apache-2.0 | image hash; header matches upstream |
| libcorrect | `quiet/libcorrect@f5a28c74fba7a99736fe49d3a5243eca29517ae9` | git-subtree split recorded by the import | BSD-3-Clause | encoder/ECC; local CMake hardening only |
| libpopcnt | `kimwalisch/libpopcnt@5214d3fba1dcebd7ea36f0aed2731549d16d7df9` | tag v3.1 and importing commit | BSD-2-Clause | image hash; header matches upstream |
| Wirehair | `catid/wirehair@0d8b51da63c4f146112b46f225ffa34ac1183f16` | immutable source match; library ABI 2 | BSD-3-Clause | fountain encode/decode; reviewed alignment, allocation, and range-validation patches are recorded by the current tree digest |
| zstd | `facebook/zstd@63779c798237346c2b245c546c40b72a5a5913fe` | tag v1.5.5 and `ZSTD_VERSION_*` | BSD-3-Clause option | compatibility compression; local build subset and filename plumbing |
| PicoSHA2 | `okdshin/PicoSHA2@7bfa26156981f7181f240906495a2c33c7fa48be` | immutable header match | MIT | tests only |
| fmt | `fmtlib/fmt@407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` | tag 12.1.0 and `FMT_VERSION` | MIT | serialization formatting; headers match upstream |
| concurrentqueue | `cameron314/concurrentqueue@6dd38b8a1dbaa7863aa907045f32308a56a6ff5d` | tag v1.0.4 and importing commit | BSD-2-Clause OR BSL-1.0 | concurrent fountain sink; header matches upstream |
| stb_image | `nothings/stb@31c1ad37456438565541f4919958214b6e762fb4` | v2.30 header banner | MIT OR Unlicense | production embedded-image decoder; exact upstream header |
| Catch2 | `catchorg/Catch2@216713a4066b79d9803d374f261ccb30c0fb451f` | tag v2.13.8 and header macros | BSL-1.0 | tests only; header matches upstream |

The source trees themselves are fingerprinted in the SPDX SBOM. The immutable
revisions above are also queried by `osv-scanner-custom.json`; this is necessary
because automatic source scanning did not recognize this repository's copied
layout. Before accepting an update, the owner must update the upstream revision,
tree digest, release date, rationale, local-patch description, and OSV lock.

## Direct External Build Dependencies

`vcpkg.json` pins builtin baseline
`6ecbbbdf31cba47aafa7cf6189b1e73e10ac61f8`, the exact baseline used by the
Windows reference environment. A dry-run for `x64-mingw-dynamic` resolves the
versions below and their transitive graph reproducibly. These packages are in
the default `compatibility-apps` feature; hardened-transport-only builds disable
manifest default features and therefore install none of them. OpenCV's broad
defaults are disabled, retaining only calib3d, JPEG, PNG, and threading support.

| Component | CMake evidence | Pinned Windows resolution | Update owner |
| --- | --- | --- | --- |
| OpenCV | find_package(OpenCV REQUIRED) | opencv4 4.11.0#4; upstream `31b0eeea0b44b370fd0712312df4214d4ae1b158` | Repository security maintainer |
| GLFW | find_package(glfw3 CONFIG REQUIRED) on Windows | glfw3 3.4#1; upstream `7b6aead9fb88b3623e3b3725ebb42670cbe4c579` | Repository security maintainer |
| OpenGL Registry | find_path(GLES3/gl3.h) on Windows | opengl-registry 2024-02-10#1; upstream `3530768138c5ba3dfbb2c43c830493f632f7ea33` | Repository security maintainer |

The narrowed opencv4 feature set pulls libjpeg-turbo, libpng, and zlib, plus
vcpkg build helpers; the GUI headers also pull the EGL Registry. The baseline
fixes their resolution, but they must still be emitted as individual packages
in a compatibility-app release SBOM and reconciled with its binary inventory.

Linux CI currently resolves OpenCV, GLFW, and GLES headers from Ubuntu 24.04.
That distribution-selected graph is intentionally not claimed to be locked by
the Windows vcpkg manifest.

## Vulnerability Monitoring

OSV-Scanner 2.3.8 was run against the 15 explicit Git commits in
`osv-scanner-custom.json`. The initial copied-dependency scan found two
production-reachable stb_image findings: `OSV-2020-1372` and `OSV-2021-1239`.
The vendored header was upgraded from v2.27 to v2.30 at the locked revision,
which clears both findings.

The unfiltered all-package result is 15 packages and seven known vulnerabilities,
all assigned to the compatibility-apps OpenCV 4.11.0 source revision:
`OSV-2022-394`, `OSV-2023-444`, `OSV-2025-16`, `OSV-2025-17`,
`OSV-2025-51`, `OSV-2025-68`, and `OSV-2025-190`. Five are OpenCV PNG
decoder findings that were initially reachable through generic file-decoding
applications. All production filename-to-image paths now use bounded stb_image
v2.30 instead; `scripts/check-dependency-lock.py` fails if `cv::imread`,
`cv::imdecode`, or `cv::split` reappears outside tests. The pinned vcpkg feature
set does not enable OpenJPEG. None of OpenCV, image codecs, GLFW, or the OpenGL
Registry is configured or installed for the hardened transport profile.

`.github/workflows/security-dependencies.yml` runs the explicit scan on changes,
weekly, and on demand. It uploads SARIF and fails on vulnerabilities. Pull
requests compare the base and proposed locks. Dependabot separately monitors
pinned GitHub Actions. `osv-scanner.toml` contains seven reviewed reachability
exceptions expiring 2026-10-22. CI validates their exact set, rationale, expiry,
feature restrictions, and forbidden-call absence before the filtered scan can
pass. The current filtered result is 15 packages and zero actionable findings.
OSV commit coverage is incomplete by design, so that result does not replace
release-note review or fuzzing.

## Required Follow-up

1. Generate a full transitive SBOM for each supported release target, including
   the pinned vcpkg packages and the actual Ubuntu packages used by CI/releases.
2. Regenerate the SBOM from the clean release commit and compare its package
   inventory against the binary and package-manager evidence.
3. Re-source locally patched Wirehair and zstd from their locked upstream
   revisions during upgrades, then reapply each documented patch deliberately.
4. Upgrade OpenCV and remove the seven reachability exceptions before their
   2026-10-22 expiry even if the vulnerable APIs remain unreachable.
