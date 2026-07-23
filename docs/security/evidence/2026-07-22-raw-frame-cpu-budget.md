# Raw-Frame CPU Budget Evidence - 2026-07-22

## Scope

- Work package: WP-09 raw-frame and geometry fuzzing
- Boundary: `cimbard_scan_extract_decode_checked` through RGB conversion,
  extraction, scanner confirmation, and failed-frame return
- Goal: measure the public 4096 x 4096 accepted-frame ceiling without timing
  fuzzing, filesystem access, browser copying, or harness input generation

## Control

`cimbar_raw_frame_cpu_budget` is a benchmark executable in the checked receiver
test directory, measured from a Release configuration. It preallocates five
4096 x 4096 tightly packed RGB patterns:

- solid black
- solid white
- 4-pixel checkerboard
- dense 1:1:4:1:1 anchor-grid pattern
- deterministic xorshift noise

Each pattern has one warm-up and three timed scans. The timer begins immediately
before `cimbard_scan_extract_decode_checked` and ends on return; frame creation,
harness output allocation, and `cimbard_reset_decode` are outside the interval.
Allocations performed by the checked ABI, including its OpenCV conversion and
debug-frame ownership, remain inside the interval. The command disables OpenCL
and calls `cv::setNumThreads(1)` so elapsed time is a single-threaded CPU-bound
ceiling for this invocation. A non-
`CIMBARD_SCAN_EXTRACT_FAILED` result is a benchmark failure; a measured maximum
above the selected budget exits with status 2.

Scanner confirmation work is also capped at 64 candidates for both the primary
and bottom-right search stages. This bounds the nested scan and candidate
comparison work before it can grow with a hostile image pattern.

## Reference Environment

- WSL Ubuntu
- 13th Gen Intel Core i7-13700H
- Linux 6.6.87.2-microsoft-standard-WSL2
- Clang 18.1.3
- OpenCV 4.6.0
- Release CMake build, Ninja generator
- 4096 x 4096 RGB input, 50,331,648 input bytes per scan

The 250 ms limit is a release acceptance budget for this hardware and software
class, not a universal real-time guarantee. Re-run the command and explicitly
set a new product budget when the CPU class, compiler, OpenCV version, threading
policy, camera format, or frame-size policy changes.

## Commands

```text
cmake -S . -B out/build/raw-frame-cpu-wsl -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DLIBCIMBAR_BUILD_GUI_TOOLS=OFF
cmake --build out/build/raw-frame-cpu-wsl --target cimbar_raw_frame_cpu_budget -j 4
out/build/raw-frame-cpu-wsl/build/src/lib/cimbar_js/test/cimbar_raw_frame_cpu_budget --iterations 3
```

## Results

```text
raw-frame CPU budget
opencv=4.6.0 threads=1 opencl=disabled
frame=4096x4096 rgb-bytes=50331648 iterations=3 budget-ms=250.000
pattern=solid-black max-ms=103.142
pattern=solid-white max-ms=106.962
pattern=checkerboard max-ms=94.836
pattern=anchor-grid max-ms=98.348
pattern=noise max-ms=95.567
overall-max-ms=106.962
verdict=PASS
```

The highest observed scan was the solid-white full-size frame at 106.962 ms,
which passes the 250 ms budget with 143.038 ms of headroom.

## Follow-up Validation

The budget command is intentionally not part of generic CTest because a timing
limit must be tied to a declared hardware class. Sanitizer checks still cover
the same paths: build and run `cimbar_raw_frame_cpu_budget` at a reduced frame
size under ASan/UBSan, then run `extractor_test`, `cimbar_js_test`, and the
standalone raw-frame fuzzer smoke.

Completed sanitizer validation:

- The ASan/UBSan `cimbar_raw_frame_cpu_budget --dimension 128 --iterations 1
  --max-ms 5000` smoke passed, with a 9.631 ms maximum.
- `extractor_test` passed under ASan/UBSan in 6.28 seconds.
- `cimbar_js_test` passed under ASan/UBSan in 1.39 seconds.
- The regenerated corpus check passed with 34 seeds.
- The rebuilt `fuzz_raw_frame` smoke ran 1,000 inputs from a temporary writable
  copy of the 15 committed raw-frame seeds in 92 seconds without a sanitizer
  finding.
