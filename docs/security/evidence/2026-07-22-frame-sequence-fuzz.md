# Frame-Sequence Fuzzing Evidence - 2026-07-22

## Scope

- Work package: WP-10 end-to-end frame-sequence fuzzer
- Components: `fuzz_frame_sequence`, `fountain_decoder_session`,
  `fountain_decoder_sink`, `FountainEncoder`, deterministic fuzz corpus
- Boundary: encoded fountain packet sequences after corrected-payload admission
  and before opaque restricted-session object output

## Remediation

- Added `fuzz/fuzz_frame_sequence.cpp`.
- The harness creates a bounded source object, encodes real Wirehair fountain
  packets, and submits single-packet or batched frames through the restricted
  `fountain_decoder_session`.
- The schedule grammar covers canonical completion, selected reordering,
  dropped packets, duplicate replay, batching, transfer-delay expiry, explicit
  reset/replay, and hostile packet envelopes.
- Completion is asserted at most once before reset.
- Every completed object is taken through the session API and must byte-match
  the generated source exactly.
- Non-completion, failed, delayed, and reset paths must expose no completed
  object.
- Added deterministic corpus seeds for valid completion, reordered/duplicate
  completion, dropped/delayed no-output, mutated-envelope no-output, and
  batched reset/replay.
- Added the target to the standalone fuzz CMake build and the sanitizer CI
  smoke workflow.

## Boundary Note

During harness development, allowing arbitrary accepted payload-byte mutation
found a non-exact recovered object. That is expected for unauthenticated
Wirehair payload corruption: the fountain stage is an erasure/reconstruction
layer, not a content-authentication layer. WP-10 therefore keeps mutation
coverage to rejected packet envelopes at this boundary. Packet-content
integrity remains tied to the corrected-payload/ECC boundary and any future
content-authentication work.

## Validation

Environment:

- WSL Ubuntu
- Clang 18.1.3
- libFuzzer with AddressSanitizer and UndefinedBehaviorSanitizer
- Raw-frame fuzzing disabled for this focused fountain/Wirehair closure

Commands:

```text
py fuzz/corpus/generate.py --check
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && cmake -S fuzz -B out/fuzz-wp10-wsl -G Ninja -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DLIBCIMBAR_FUZZ_RAW_FRAME=OFF && cmake --build out/fuzz-wp10-wsl --target fuzz_frame_sequence -j 4"
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && rm -rf out/fuzz-corpus/frame_sequence out/fuzz-artifacts/frame_sequence && mkdir -p out/fuzz-corpus/frame_sequence out/fuzz-artifacts/frame_sequence && cp fuzz/corpus/frame_sequence/*.bin out/fuzz-corpus/frame_sequence/ && ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 out/fuzz-wp10-wsl/fuzz_frame_sequence out/fuzz-corpus/frame_sequence -runs=5000 -max_len=1024 -rss_limit_mb=256 -timeout=5 -artifact_prefix=out/fuzz-artifacts/frame_sequence/"
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && cmake --build out/fuzz-wp10-wsl -j 4"
py fuzz/corpus/generate.py --check
```

Results:

- Corpus check passed: 39 seeds.
- `fuzz_frame_sequence` built successfully.
- Full fountain-only standalone fuzz workspace built successfully.
- Frame-sequence fuzz smoke passed 5,000 runs over five committed seeds and
  mutations.
- Final frame-sequence campaign reached 4,389 coverage counters, 16,120
  features, 162 corpus entries / 3,624 bytes, and 134 MB peak reported RSS.
- Re-running the source corpus check after the writable-corpus campaign still
  passed with 39 seeds.

## Remaining Risk

- This target does not claim that the fountain layer detects arbitrary
  unauthenticated payload corruption. That must be handled before or above this
  boundary.
- Long-running campaigns should retain only reviewed non-sensitive coverage
  inputs. Private crash reproducers should stay in the coordinated finding
  process described in `SECURITY.md`.
