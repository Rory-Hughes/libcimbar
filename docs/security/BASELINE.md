# Audit Baseline

## Target

| Field | Value |
|---|---|
| Fork | `Rory-Hughes/libcimbar` |
| Frozen branch | `audit-baseline-681e18e` |
| Frozen commit | `681e18eb61a059f4a796bc6ef097d24b45c430eb` |
| Upstream | `sz3/libcimbar` |
| Upstream default branch | `master` |
| Baseline date | 2026-07-17 |
| Language | C++17 with bundled C/C++ dependencies |
| Primary build system | CMake |

The frozen branch is a reference artifact. Do not commit fixes, formatting changes, dependency updates, or generated files to it.

## Upstream-stated status and functionality

The upstream README describes libcimbar as an experimental colour-icon matrix barcode format for air-gapped transfer. It reports animated optical transfer, Reed-Solomon error correction, Wirehair fountain reconstruction, zstd compression, and support for files up to approximately 33 MB after compression.

This audit does not assume that an optical channel is trusted. A compromised companion device can intentionally render hostile frames.

## Known build structure

The top-level build currently includes first-party areas for:

- Bit handling.
- Chromatic adaptation.
- Cimbar translation.
- Compression.
- Encoding and decoding.
- Extraction and geometry.
- Fountain reconstruction.
- GUI utilities.
- Image hashing.
- Serialization.
- General utilities.

It also includes or links third-party components including:

- OpenCV.
- GLFW and OpenGL ES headers.
- Wirehair.
- zstd.
- libcorrect.
- stb image code.
- concurrentqueue.
- fmt.
- cxxopts.
- intx.
- libpopcnt.
- Catch2.
- Base91 code.

The complete dependency inventory and exact versions still need to be generated from the frozen source and build environment.

## Existing automated checks observed

The upstream GitHub Actions configuration currently includes:

- GCC builds.
- Clang builds.
- CTest.
- Python usage tests.
- cppcheck.

Dedicated sanitizer and coverage-guided fuzzing jobs were not observed in the baseline workflow.

## Preliminary audit leads

These are review priorities, not confirmed vulnerabilities.

### Attacker-directed object sizing

Fountain metadata contains a claimed file size. The decoder path creates reconstruction state using metadata-derived size. The product profile must reject size values outside a Secure-Core-selected policy before any size-dependent decoder allocation.

### Persistent and potentially growing state

Fountain decoding tracks seen block identifiers, active streams, and completed objects. Each collection needs an explicit maximum, expiry rule, reset rule, and memory budget.

### Decompression expansion

The generic zstd path streams decompressed output without a product-level output ceiling visible at the wrapper boundary. The message and wallet transport profiles should remove decompression entirely.

### Filesystem output

The generic decoder can construct output paths and can derive an output filename from compressed metadata. The production transport component should have no filename or arbitrary filesystem-output feature.

### Image-container parsing

The command-line tool accepts arbitrary image files through OpenCV. The embedded production decoder should instead accept a fixed raw-frame format from the camera pipeline.

## Baseline evidence to collect on desktop

- Clean recursive clone of the frozen branch.
- Exact submodule state.
- Build-container digest.
- Compiler and linker versions.
- OpenCV and system dependency versions.
- CMake cache.
- CTest results.
- Python test results.
- Binary hashes and sizes.
- `ldd` or equivalent runtime dependency output.
- Initial SBOM.
- Initial benchmark on representative x86-64 and ARM64 systems.
