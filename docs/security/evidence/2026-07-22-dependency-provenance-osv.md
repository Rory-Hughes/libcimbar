# Dependency Provenance and OSV Evidence — 2026-07-22

## Scope and Result

This work package identified immutable upstream revisions for every copied
dependency and the three direct external compatibility-application dependencies.
It binds those revisions across the SPDX catalog, an explicit OSV lock, and a
pinned vcpkg manifest.

The audit found and repaired two production-reachable stb_image advisories. A
corrected all-package scan then reported seven known OpenCV 4.11.0 advisories.
Five PNG-decoder findings were reachable through generic file-input tools. Every
production filename-to-image path was migrated to bounded stb_image v2.30, and
the seven remaining package-level findings now have reviewed exceptions expiring
2026-10-22.

The personal-project hardened transport profile was separately tightened so it
does not discover, install, configure, or link OpenCV, GLFW, GLES headers, or any
image codec.

## Scanner Integrity

The local scanner was the official OSV-Scanner v2.3.8 Linux amd64 release:

- OSV-Scanner 2.3.8, OSV-Scalibr 0.4.5, commit
  `408fcd6f8707999a29e7ba45e15809764cf24f67`.
- Release binary SHA-256:
  `bc98e15319ed0d515e3f9235287ba53cdc5535d576d24fd573978ecfe9ab92dc`.

An automatic recursive scan reported no packages under `src/third_party_lib`.
That result is not valid evidence of dependency safety: the repository copies
C and C++ source without recognized package-manager metadata. The audit therefore
created `osv-scanner-custom.json` with 15 explicit Git commits.

## Immutable Source Matching

The following copied sources were matched to exact upstream Git revisions using
header/blob comparison, import history, or recorded git-subtree metadata:

| Package | Upstream revision | Match evidence |
| --- | --- | --- |
| base91 | `9c50c57b46b9be1b028134a65e3f12d40516e9b1` | normalized `base.hpp` match |
| cxxopts | `302302b30839505703d37fb82f536c53cf9172fa` | exact v2.2.1 header match |
| intx | `4c1ca55d78777ffea7ede46e70cbd46a5beef008` | exact v0.9.3 header match |
| libcorrect | `f5a28c74fba7a99736fe49d3a5243eca29517ae9` | importing commit subtree split and representative source match |
| libpopcnt | `5214d3fba1dcebd7ea36f0aed2731549d16d7df9` | exact v3.1 header match |
| Wirehair | `0d8b51da63c4f146112b46f225ffa34ac1183f16` | all upstream source blobs match the original libcimbar import; current local security patches are source-digested |
| zstd | `63779c798237346c2b245c546c40b72a5a5913fe` | exact v1.5.5 public header match |
| PicoSHA2 | `7bfa26156981f7181f240906495a2c33c7fa48be` | exact header match |
| fmt | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` | exact 12.1.0 header match |
| concurrentqueue | `6dd38b8a1dbaa7863aa907045f32308a56a6ff5d` | exact v1.0.4 header match |
| Catch2 | `216713a4066b79d9803d374f261ccb30c0fb451f` | exact v2.13.8 single-header match |

The SPDX generator computes a deterministic SHA-256 digest over every copied
source tree. CI checks those digests before advisory scanning, and
`scripts/check-dependency-lock.py` requires the OSV and SPDX Git coordinates to
agree.

## stb_image Remediation

The initial explicit scan tied the old `stb_image` v2.27 snapshot at
`5ba0baaa269b3fd681828e0e3b3ac0f1472eaf40` to:

- `OSV-2020-1372`
- `OSV-2021-1239`

This path is production-reachable from `cimb_translator/Common.cpp`, which uses
`stbi_info_from_memory` and `stbi_load_from_memory` for embedded image assets.
The header was replaced byte-for-byte with upstream v2.30 at
`31c1ad37456438565541f4919958214b6e762fb4`. Its Git blob ID is
`9eedabedc45b3e6fd88fae6f14a160b4d53272ec` both locally and upstream. Neither
stb advisory appears after the replacement.

## OpenCV Findings

The corrected command used `--all-packages` so clean packages remained in JSON:

~~~text
osv-scanner scan --all-packages --format json --config=/dev/null \
  --lockfile=osv-scanner:./osv-scanner-custom.json
packages=15 vulnerabilities=7
~~~

All seven results are assigned to OpenCV 4.11.0 revision
`31b0eeea0b44b370fd0712312df4214d4ae1b158`:

- `OSV-2022-394`: reported `cv::split` function-pointer mismatch; no `cv::split`
  call exists in first-party source.
- `OSV-2023-444`: OpenJPEG palette handling; the pinned vcpkg feature set does
  not enable OpenJPEG.
- `OSV-2025-16`, `OSV-2025-17`, `OSV-2025-51`, `OSV-2025-68`, and
  `OSV-2025-190`: OpenCV PNG decoder memory-safety findings. Generic CLI and
  compatibility helper paths initially called `cv::imread`.

Those production calls now use `cimbar::load_img_file`, which caps encoded input
at 64 MiB and reuses the 4096-per-dimension, 16,777,216-pixel stb decoder. Missing,
empty, oversized, malformed, short-read, and expected allocation failures return
an empty image before output creation or image processing. The lock validator
rejects any production `cv::imread`, `cv::imdecode`, or `cv::split` occurrence.

`osv-scanner.toml` records the exact seven exceptions with reasons and a
2026-10-22 expiry. The unfiltered result remains part of this evidence. With the
reviewed configuration loaded, the result is:

~~~text
packages=15 actionable_vulnerabilities=0
Filtered 7 vulnerabilities from output
~~~

This is a reachability decision, not a claim that the OpenCV source revision is
patched. The validator fails on expiry or boundary drift, and the dependency
must still be upgraded before the exception date.

## vcpkg and Product-Profile Validation

`vcpkg.json` pins builtin baseline
`6ecbbbdf31cba47aafa7cf6189b1e73e10ac61f8`. The default
`compatibility-apps` feature resolves OpenCV 4.11.0#4 with only calib3d, core,
JPEG, PNG, and thread features, plus GLFW and the OpenGL Registry. Its direct
codec dependencies are limited to libjpeg-turbo, libpng, and zlib.

Dry-run results for `x64-mingw-dynamic`:

- Default features: resolves the compatibility graph successfully.
- `--x-no-default-features`: resolves no packages for the hardened transport.

A fresh Linux configuration with `LIBCIMBAR_HARDENED_TRANSPORT_ONLY=ON` built
only Wirehair and the hardened wrapper. The archive and linked test passed the
forbidden-symbol/dependency verifier, both CTest cases passed, and a scan of
`CMakeCache.txt` plus `build.ninja` found no OpenCV, GLFW, GLES, or image-codec
reference.

The full ASan/UBSan build completed after the decoder migration. All 12 CTest
targets passed, including translator, encoder, extractor, fountain, concurrency,
and hardened-profile tests. The JPEG decoder change altered one extraction
fixture by exactly one average-hash bit; the stb-derived value is now the pinned
regression result.

A fresh 80-unit CodeQL 2.26.1 production extraction and `security-extended`
analysis reported zero first-party findings. The ownership gate passed; all 42
results are vendored baseline findings (fmt 1, stb 8, Wirehair 9, zstd 24).

## Residual Work

1. Upgrade OpenCV and remove all seven exceptions before 2026-10-22.
2. Generate full transitive, target-specific release SBOMs rather than treating
   this direct catalog as a release SBOM.
3. Lock and inventory the Linux distribution packages used for any Linux release.
