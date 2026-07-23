# MemorySanitizer: Hardened Transport Profile

Date: 2026-07-21  
Scope: byte-only `cimbar_hardened_transport` production closure and its profile test

## Method

Clang 18.1.3 on WSL was first checked with two minimal PIE executables. A fully initialized program exited successfully; a deliberate uninitialized read was detected and exited with the configured status 86. This established that the local MemorySanitizer runtime and shadow mapping worked before interpreting project results.

The project probe used:

```text
-fsanitize=memory
-fsanitize-memory-track-origins=2
-fno-omit-frame-pointer
-fPIE
-pie
MSAN_OPTIONS=halt_on_error=1:exit_code=86:poison_in_dtor=1
```

The maintained preset disables GUI tools and builds only `hardened_transport_profile_test`. This deliberately excludes the OpenCV image stack and other broad targets whose distribution C++ dependencies are not MemorySanitizer-instrumented. It is a focused product-closure result, not a claim that every external library in a full build is clean.

## Findings and remediation

Initial execution crossed uninstrumented libstdc++ storage in test-only `std::stringstream` and `std::string` staging. The profile test now uses value-initialized fixed arrays and the same direct `FountainEncoder` API as the production closure.

The next report reached `std::set<unsigned>` duplicate tracking in `FountainDecoder`. Independently of the sanitizer's standard-library boundary, review showed a resource issue: each attacker-selected unique block caused a tree-node allocation outside the Wirehair codec-memory budget. Duplicate tracking is now a policy-sized `std::vector<std::uint64_t>` bitmap indexed only after the maximum block identifier is checked. At the 16-bit wire maximum it occupies 8 KiB; the hardened test policy's maximum identifier of 64 requires two 64-bit words.

The final report was caused by unnecessary completion-filename construction in the no-history product path. The sink now has a filename-free completion path when `maximum_completed_transfers` is zero, preserving the compatibility history behavior when enabled.

## Result

The rebuilt focused executable completed with exit status 0 under MemorySanitizer and origin tracking. The same changes and a new block-boundary regression passed the focused `fountain_test` and `hardened_transport_profile_test` selection under ASan/UBSan.

Reproduction:

```sh
cmake --preset memory-sanitizer
cmake --build --preset memory-sanitizer
ctest --preset memory-sanitizer
```

The sanitizer workflow now repeats this check on Ubuntu 24.04. A future full-project MemorySanitizer claim would require an instrumented libc++, OpenCV, and remaining dependency closure; that work is not represented by this result.
