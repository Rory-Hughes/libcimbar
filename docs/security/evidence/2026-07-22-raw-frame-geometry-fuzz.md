# Raw-Frame Geometry Fuzzing Evidence - 2026-07-22

## Scope

- Work package: WP-09 raw-frame and geometry fuzzing
- Components: `fuzz_raw_frame`, checked receive ABI, `Scanner`, `Geometry`,
  `Deskewer`, deterministic fuzz corpus
- Boundary: tightly packed hostile raw frame bytes and derived geometry into
  OpenCV conversion, scanner probing, midpoint calculation, and perspective warp

## Remediation

- Added `fuzz/fuzz_raw_frame.cpp`.
- Builds the target only when OpenCV is available, leaving fountain-only fuzzing
  available in minimal environments.
- The harness sends RGB, RGBA, NV12, and I420 envelopes through
  `cimbard_scan_extract_decode_checked`, with generated fuzz frames capped at
  96 x 96 pixels.
- The harness mutates unsupported formats, zero and oversized dimensions, odd
  subsampled dimensions, null input, short buffers, long buffers, and zero output
  buffers.
- The same input also exercises scanner edge probing, midpoint line
  intersections, deskew transforms, degenerate corners, NaN, infinity, and
  bounded extreme coordinates.
- Scanner pixel probes outside the current matrix now fail closed.
- Deskew now returns an empty image for empty input, invalid output dimensions,
  non-finite perspective transforms, and singular perspective transforms.
- Added deterministic raw-frame corpus seeds for valid RGB/RGBA/NV12/I420,
  cropped, rotated, noisy, overexposed, damaged, unsupported-format,
  null-input, non-finite-geometry, and degenerate-corner cases.

## Supported Raw Frame Contract

The checked public compatibility API accepts only tightly packed:

- RGB, three bytes per pixel
- RGBA, four bytes per pixel
- NV12, 12 bits per pixel with even width and height
- I420, 12 bits per pixel with even width and height

The public API rejects zero dimensions, unsupported formats, mismatched byte
lengths, and frames above `CIMBARD_MAX_FRAME_PIXELS` before OpenCV conversion.
The fuzz generator uses a smaller 96 x 96 ceiling so pull-request smoke runs
remain resource-bounded.

## Validation

Environment:

- WSL Ubuntu
- Clang 18.1.3
- OpenCV 4.6.0 from the WSL system packages
- ASan/UBSan selected by the fuzz target and focused CTest environment

Commands:

```text
py fuzz/corpus/generate.py --check
cmake -S fuzz -B out/fuzz-wp09-wsl -G Ninja -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build out/fuzz-wp09-wsl -j 4
rm -rf out/fuzz-corpus/raw_frame && mkdir -p out/fuzz-corpus/raw_frame out/fuzz-artifacts/raw_frame
cp fuzz/corpus/raw_frame/*.bin out/fuzz-corpus/raw_frame/
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 out/fuzz-wp09-wsl/fuzz_raw_frame out/fuzz-corpus/raw_frame -runs=1000 -max_len=4096 -timeout=2 -artifact_prefix=out/fuzz-artifacts/raw_frame/
cmake --build out/build/asan-ubsan --target extractor_test -j 4
cmake --build out/build/asan-ubsan --target cimbar_js_test -j 4
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ctest --test-dir out/build/asan-ubsan -R '^extractor_test$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ctest --test-dir out/build/asan-ubsan -R '^cimbar_js_test$' --output-on-failure
```

Results:

- Corpus check passed: 34 seeds.
- `fuzz_raw_frame` built successfully.
- Full standalone fuzz workspace built successfully.
- Raw-frame fuzz smoke passed 1,000 runs over 15 committed seeds and mutations.
- `extractor_test` passed under ASan/UBSan.
- `cimbar_js_test` passed under ASan/UBSan.

## Remaining Risk

- WP-09 still needs a reproducible worst-case per-frame CPU measurement and an
  accepted per-frame budget.
- The public compatibility API still allows four raw formats for legacy callers;
  a hardened product profile should select one camera format, fixed dimensions,
  and a stride contract.
