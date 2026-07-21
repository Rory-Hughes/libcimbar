# Dependency Inventory

This is the initial, direct-dependency inventory for libcimbar. It is deliberately
conservative: a version is only named where the vendored source declares it or
the local reference build resolved it. Missing provenance is a tracked supply-chain
gap, not a guessed value.

The machine-readable companion is sbom/libcimbar.spdx.json. Regenerate it with:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/generate-sbom.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/generate-sbom.ps1 -Check
~~~

-Check validates the SPDX envelope, catalog, relationships, and a deterministic
SHA-256 fingerprint of every vendored dependency tree. It also detects a changed
repository revision or a changed declared package value.

## Scope

- First-party source: libcimbar, licensed MPL-2.0.
- Vendored libraries and the single-header test framework in this checkout.
- Direct CMake dependencies only. Transitive vcpkg dependencies are not yet locked
  or enumerated as shipped components.
- The samples git submodule is development data, not a libcimbar runtime
  dependency. The checked-out reference is
  9bbafc5ea0ad27e2a43ec9eee387dd60714a6c7f (heads/v0.6).

This is not a release SBOM. It should be regenerated from a clean,
release-candidate commit, alongside the final binary inventory.

## Vendored Source

| Component | Version evidence | License | Reachability / location | Provenance status |
| --- | --- | --- | --- | --- |
| base91 | 1.0.2 (BASE_VERSION) | Zlib | src/third_party_lib/base91; base91 codec | Needs upstream revision |
| cxxopts | 2.2.0 (CXXOPTS__VERSION_*) | MIT | src/third_party_lib/cxxopts; command-line tools | Needs upstream revision |
| intx | Unknown | Apache-2.0 | src/third_party_lib/intx; image hash | Needs upstream revision |
| libcorrect | Unknown | BSD-3-Clause | src/third_party_lib/libcorrect; encoder/ECC build target | Needs upstream revision |
| libpopcnt | Unknown | BSD-2-Clause | src/third_party_lib/libpopcnt; image hash | Needs upstream revision |
| Wirehair | Unknown (library ABI 2) | BSD-3-Clause | src/third_party_lib/wirehair; fountain encode/decode | Needs upstream revision |
| zstd | 1.5.5 (ZSTD_VERSION_*) | BSD-3-Clause option | src/third_party_lib/zstd; compression paths | Needs upstream revision |
| PicoSHA2 | Unknown | MIT | src/third_party_lib/PicoSHA2; tests only | Needs upstream revision |
| fmt | 12.1.0 (FMT_VERSION) | MIT | src/third_party_lib/fmt; serialization formatting | Needs upstream revision |
| concurrentqueue | Unknown | BSD-2-Clause OR BSL-1.0 | src/third_party_lib/concurrentqueue; concurrent fountain sink | Needs upstream revision |
| stb_image | 2.27 (header banner) | MIT OR Unlicense | src/third_party_lib/stb; no current CMake-target include found | Needs upstream revision |
| Catch2 | 2.13.8 (CATCH_VERSION_*) | BSL-1.0 | test/catch.hpp; tests only | Needs upstream revision |

The source trees themselves are fingerprinted in the SPDX SBOM. Before accepting
an update, the owner must record the upstream repository URL, immutable revision,
release date, update rationale, and any local patch.

## Direct External Build Dependencies

The repository has no vcpkg.json, vcpkg-configuration.json, or other lockfile.
Consequently, the values below are observations from the Windows reference
environment, not reproducible constraints.

| Component | CMake evidence | Reference environment observation | Update owner |
| --- | --- | --- | --- |
| OpenCV | find_package(OpenCV REQUIRED) | vcpkg opencv4 4.11.0#4, x64-mingw-dynamic | Repository security maintainer |
| GLFW | find_package(glfw3 CONFIG REQUIRED) on Windows | vcpkg glfw3 3.4#1, x64-mingw-dynamic | Repository security maintainer |
| OpenGL Registry | find_path(GLES3/gl3.h) on Windows | vcpkg opengl-registry 2024-02-10#1, x64-mingw-dynamic | Repository security maintainer |

opencv4 pulls additional packages in the reference environment, including
libjpeg-turbo, libpng, libtiff, libwebp, quirc, zlib, and protobuf.
They must be captured from a lockfile and the release binary linked-library
inventory before a release can claim complete dependency coverage.

## Required Follow-up

1. Replace each Unknown vendored revision with an immutable upstream reference
   and record any local divergence.
2. Add a vcpkg manifest/configuration with a pinned baseline, then generate a
   full transitive SBOM for each supported release target.
3. Add automated advisory monitoring against the resolved package coordinates;
   do not use this inventory's unversioned entries for vulnerability conclusions.
4. Regenerate the SBOM from the clean release commit and compare its package
   inventory against the binary and package-manager evidence.
