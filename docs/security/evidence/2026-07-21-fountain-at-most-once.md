# Fountain At-Most-Once Completion Evidence

## Scope

- Date: 2026-07-21
- Components: `fountain_decoder_sink`, fountain unit tests, state-machine fuzzer
- Toolchain: Clang 18.1.3 with AddressSanitizer and UndefinedBehaviorSanitizer under WSL

## Finding

The completed-transfer detail map has a configurable retention limit. After an
entry was evicted, the same hostile transfer could be reconstructed again and
could invoke the output callback more than once. Keeping the map bounded was
necessary for resource control, but eviction weakened replay handling.

The existing wire format does not carry a transfer generation or nonce. Its
seven-bit encode ID is therefore the only fixed-size value that can define an
exact terminal set without attacker-controlled growth.

## Remediation

- Added a 128-bit completed encode-ID set to each decoder sink.
- Marked an encode ID terminal only after recovery and the output callback
  complete successfully.
- Rejected later frames with that encode ID using the stable
  `transfer_already_completed` result.
- Kept the bounded full-ID map for recent filename and status details only.
- Made `reset()` clear the terminal set and documented it as the explicit
  decoder-session boundary.
- Added `cimbard_reset_decode()` to the C/WASM boundary. It clears the sink,
  recovered object, decompressor, reporting, and debug-frame state together;
  decode-mode changes now use the same complete reset instead of retaining old
  recovered output.
- Expanded the state-machine fuzzer to exercise structured valid completion,
  duplicate and conflicting frames, recovery, cancellation, timeout, replay,
  reset, and bounded-state invariants.
- Made `FountainEncoder.h` self-contained after the expanded fuzzer exposed an
  include-order dependency on `size_t` and `std::swap` declarations.

## Verification

The focused fountain test executable passed under ASan/UBSan:

```text
All tests passed (157066 assertions in 37 test cases)
```

The first complete-suite run exposed retained process-global state in the
browser C API: a later decode session reused an encode ID and received the new
replay rejection. After adding the explicit complete C-API reset and lifecycle
regression, the focused browser test and full suite passed:

```text
cimbar_js_test: passed
CTest: 100% tests passed, 0 failed out of 9
```

The state-machine fuzzer completed a fresh empty-corpus smoke campaign under
ASan/UBSan:

```text
10000 runs completed
3654 coverage points, 8322 features
173 MB peak reported RSS (256 MB limit)
no crash or sanitizer report
```

Regression coverage confirms that detail-cache eviction does not reopen a
completed encode ID, a different claimed size cannot reuse that encode ID, the
C API rejects same-session replay, reset clears recovered output, and explicit
reset permits a new session.

## Remaining Risk

- The generic callback API cannot guarantee exactly-once external side effects
  if a callback performs an action and then throws. Hardened integrations should
  use a non-throwing transactional handoff or the planned single-take opaque
  object API.
- Encode-ID reuse now requires an explicit reset. A future versioned transport
  should add a transfer generation if multiple generations must coexist.
- The current fuzzer smoke campaign starts from an empty corpus; permanent seed
  and regression corpora remain outstanding.
