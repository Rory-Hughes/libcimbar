# CI Sanitizer Alignment Repair

## Scope

- Date: 2026-07-21
- Failed workflow: security-sanitizers, run 29612777659
- Failed jobs: ASan and UBSan; Metadata fuzzer smoke test
- Local sanitizer platform: Ubuntu 24.04 under WSL
- Toolchain: Clang 18.1.3, CMake 3.28.3, Ninja 1.11.1

## Root Causes

The ASan and UBSan job reached the test phase and failed on three UBSan
alignment reports in the vendored Wirehair implementation:

1. `WirehairCodec.cpp` placed `PeelRefs` immediately after a byte-sized
   recovery allocation without rounding the offset to its required alignment.
2. `gf256.cpp` performed scalar 32-bit and 64-bit loads and stores through
   typed pointers even when callers supplied byte-aligned buffers.
3. `WirehairCodec.cpp::Triangle` stored a 32-bit value through a potentially
   unaligned pointer into row storage.

The metadata fuzzer job failed earlier at compile time because
`FountainMetadata.h` used fixed-width integer types without including
`<cstdint>` directly.

## Remediation

- Added the missing direct `<cstdint>` include and value-initialized metadata
  storage.
- Backported the unaligned scalar access strategy from Wirehair upstream commit
  `ae59083b5a6d` by using `memcpy`-based load and store helpers in `gf256.cpp`.
- Backported the workspace padding and triangle-word access fixes from Wirehair
  upstream commit `fb6001382cb3`.
- Preserved `-fsanitize=address,undefined` and did not suppress alignment checks.
- Added the vendored third-party include root to the standalone state-fuzzer
  target so its zstd dependency resolves in the CI build.
- Regenerated the SPDX SBOM after the Wirehair source-tree digest changed.

Upstream references:

- https://github.com/catid/wirehair/commit/ae59083b5a6d
- https://github.com/catid/wirehair/commit/fb6001382cb3

## Verification

Linux sanitizer build and tests:

```text
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan

100% tests passed, 0 tests failed out of 9
```

The test command ran with leak detection, strict string checks, UBSan stack
traces, and `halt_on_error=1`. This includes the previously failing
`cimbar_js_test`, `encoder_test`, and `fountain_test` binaries.

Fuzzer smoke campaigns:

```text
fuzz_fountain_metadata: 25,000 runs completed
fuzz_fountain_state:    10,000 runs completed, 133 MB peak reported RSS
```

Both fuzzers ran with ASan and UBSan enabled. Neither produced a crash artifact
or sanitizer report. The state fuzzer stayed below its 256 MB limit.

Windows MinGW regression:

```text
100% tests passed, 0 tests failed out of 9
```

SBOM verification:

```text
scripts/generate-sbom.ps1 -Check
SBOM is current
```

## CI Status

The failing GitHub jobs reference the previous pull-request commit. A new CI
run is required after these working-tree changes are committed and pushed.
