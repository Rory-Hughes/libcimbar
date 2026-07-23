# Seeded Fountain Fuzz Corpus Evidence

## Scope

- Date: 2026-07-21
- Components: `fuzz_fountain_metadata`, `fuzz_fountain_state`, sanitizer CI,
  deterministic corpus generator and manifest
- Toolchain: Clang 18.1.3 libFuzzer with AddressSanitizer and
  UndefinedBehaviorSanitizer under WSL

## Finding

The fountain fuzzers were present and bounded but each campaign began from an
empty corpus. Reviewed boundary values and sequences for fixed defects were not
permanent inputs, so later random runs were not guaranteed to execute those
regressions directly. State operands were also decoded with host-endian
`memcpy`, making a committed sequence architecture-dependent.

## Remediation

- Added `fuzz/corpus/generate.py`, which deterministically emits 19 named binary
  seeds and a sorted JSON manifest containing descriptions, lengths, and
  SHA-256 digests.
- Added seven metadata cases for empty/truncated headers, canonical ordinary
  fields, the twenty-fifth size bit, and maximum-width values.
- Added twelve state cases for short and misaligned frames, one-block decoder
  rejection, excessive block IDs, full-ID stream-slot collision,
  duplicate/no-progress cancellation, timeout and reset release, unknown
  recovery, mixed batched metadata, terminal replay after completed-detail
  eviction, and encode-ID reuse only after explicit reset.
- Replaced native-endian state operand reads with explicit little-endian
  decoding.
- CI now checks corpus integrity, copies the reviewed seeds to a disposable
  writable corpus, and passes that copy to both sanitizer fuzz campaigns.

## Verification

```text
corpus generator check: 19/19 seeds matched
metadata campaign: 25,000 runs, no crash, 36 MB peak reported RSS
state campaign: 10,000 runs, no crash, 131 MB peak reported RSS
state campaign limit: 256 MB RSS, 5 seconds per input, maximum 512 bytes
metadata coverage merge: 2 coverage-minimal files
state coverage merge: all 12 regression files retained
post-campaign corpus check: 19/19 seeds unchanged
```

The metadata fuzzer ignores a zero-length file while loading a corpus but, as
part of normal libFuzzer initialization, still exercises the empty input. The
seven named metadata files are intentionally retained even though coverage
merge reduces them to two: their distinct truncation and field-width semantics
are useful audit evidence.

## Remaining Risk

- These are regression and boundary seeds, not a claim of exhaustive corpus
  quality. Long-running campaigns should periodically merge newly discovered
  non-sensitive coverage inputs after review.
- No undisclosed crash reproducer is included. Any future exploitable crash
  input must remain private until coordinated disclosure permits publication.
- Raw-frame, corrected-payload, and end-to-end optical sequence corpora remain
  separate P1 work.
