# Wirehair Decoder Memory Budget Evidence

## Scope

- Date: 2026-07-21
- Components: vendored Wirehair decoder allocation, `FountainDecoderLimits`,
  `fountain_decoder_sink`, restricted fountain session, hardened transport
- Toolchain: Clang 18.1.3 with AddressSanitizer and UndefinedBehaviorSanitizer
  under WSL

## Finding

Object-byte admission did not cover Wirehair-owned heap. Decoder creation
allocates input and peeling-workspace buffers, while the first solve later
allocates a Gaussian-elimination/compression matrix. The delayed allocation is
derived from hostile transfer dimensions and the number of deferred columns.
Wirehair had counters for its three payload buffers, but no public estimate or
current-usage API and no pre-construction budget check.

## Remediation

- Added `wirehair_decoder_memory_required`, calculated inside Wirehair from its
  private constants and structure sizes. It validates the supported block
  count and reserves the codec object, input buffer, workspace, a solve matrix
  with every input column conservatively deferred, pivots/heavy data, and the
  explicit `GF256_ALIGN_BYTES` overhead for each allocation.
- Added `wirehair_decoder_memory_allocated` for test and operational
  instrumentation of codec-owned heap currently allocated.
- Added per-stream and aggregate codec-memory limits. New streams reserve the
  worst-case estimate before `try_emplace`; completion, cancellation, expiry,
  failure, and reset release the reservation with the stream.
- Kept both limits disabled together by default for the generic compatibility
  sink. The restricted single-stream session requires equal, non-zero limits,
  making the product policy explicit and its aggregate bound unambiguous.
- Added distinct rejection results for a per-stream estimate and exhaustion of
  the aggregate reservation.

## Focused Verification

```text
hardened release target and artifact scans: passed
hardened_transport_profile_test: passed
hardened_transport_verifier_rejects_forbidden: passed
ASan/UBSan fountain_test: passed
ASan/UBSan hardened_transport_profile_test: passed
full ASan/UBSan CTest: 11/11 passed
ASan/UBSan fuzz_fountain_state: 10,000 runs, 165 MB peak reported RSS
ASan/UBSan fuzz_fountain_metadata: 25,000 runs, 36 MB peak reported RSS
git diff --check: passed
```

Regression coverage rejects a budget one byte below the estimate before state
mutation, admits the exact bound, prevents a second stream from exceeding the
aggregate budget, releases reservations on reset, rejects a restricted policy
with no codec budget, and checks live Wirehair allocation before and after its
solve matrix is created against the conservative estimate.

## Remaining Risk

- This is a bound for memory directly owned by a Wirehair codec. It does not
  include standard-library containers, the completed output vector, stacks,
  loaded libraries, or allocator bookkeeping beyond Wirehair's requested
  alignment padding. The isolated decoder still needs an OS-enforced process
  memory limit.
- The all-columns-deferred assumption is intentionally conservative. Final
  pairing, message, wallet, and firmware policies require hardware benchmarks
  to select budgets that both admit intended transfers and fit the OIP limit.
- The vendored Wirehair fork now has an additive local API. Any future upstream
  update must revalidate its allocation formulas and the regression tests.
