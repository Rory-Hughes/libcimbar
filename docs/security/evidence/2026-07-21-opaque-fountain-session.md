# Restricted Opaque Fountain Session Evidence

## Scope

- Date: 2026-07-21
- Components: fountain sink headers, `fountain_decoder_session`, generic CLI compatibility callers, fountain and encoder tests
- Toolchain: Clang 18.1.3 with AddressSanitizer and UndefinedBehaviorSanitizer under WSL

## Finding

The only reconstruction API was the generic `fountain_decoder_sink`. Its
header defined filename-based output callbacks, decompression-to-file helpers,
and filesystem path overloads alongside the decoder state machine. Even callers
that did not use those features transitively included zstd, filename, file, and
formatting code. The callback API also made it possible to emit an object more
than once or perform a partial external side effect outside decoder ownership.

## Remediation

- Added `FountainObjectClass` and `FountainTransferPolicy`; an unknown class,
  invalid limits, multiple active streams, mismatched active-byte limit, or any
  retained completed-detail record makes the restricted session invalid.
- Added `fountain_decoder_session` with no default policy and no callback.
- Recover into one exact-length session-owned vector only after Wirehair reports
  completion and all optical metadata limits have passed.
- Expose the vector through `take_completed_object()` exactly once using an
  ownership move. No partial vector is observable.
- Fail the session closed and clear decoder/output state after any negative
  submission result; require explicit reset before new input.
- Clear an untaken object on cancellation and require explicit reset for reuse.
- Moved `write_on_store` and `decompress_on_store` into
  `fountain_decoder_file_sink.h`, marked as generic compatibility helpers.
- Removed the state fuzzer's unnecessary zstd build and link dependency; the
  fountain core and restricted session now compile against Wirehair alone.
- Replaced hidden transitive includes in browser and generic CLI sources with
  direct dependencies.

## Focused Verification

```text
fountain_test: 157223 assertions in 43 test cases passed
encoder_test:  96 assertions in 25 test cases passed
cimbar and cimbar_recv generic compatibility targets built successfully
full ASan/UBSan CTest: 9/9 targets passed
fuzz_fountain_state: 10,000 runs, no crash or sanitizer finding
restricted header dependency check: no zstd, compression, File.h, or filesystem dependency
```

The final state-fuzzer campaign, after removing its zstd dependency, reached
3,566 coverage points and 8,203 features with 165 MB peak RSS under the
256 MB process limit.

Regression coverage verifies exact recovered bytes, explicit object class,
single take, refusal of later submissions, invalid-policy rejection, fail-closed
oversized metadata, cancellation clearing untaken output, reset, and successful
reuse in a new session.

## Remaining Risk

- Allocation failure is caught, but deterministic failure injection and output
  refusal tests are still required before WP-07 is complete.
- The session covers corrected fountain packets, not the raw-frame/OpenCV stages.
  The full product `DecoderSession` must compose the fixed camera boundary with
  this reconstruction stage.
- A dedicated product build target must prove that zstd, filename parsing, and
  filesystem helpers are absent from the final artifact.
- Wirehair internal overhead remains outside the explicit byte budget.
