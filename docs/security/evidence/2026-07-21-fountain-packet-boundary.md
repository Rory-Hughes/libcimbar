# Fountain Packet-Boundary Validation Evidence

## Scope

- Date: 2026-07-21
- Components: `FountainDecoderLimits`, `fountain_decoder_sink`, fountain unit tests, state-machine fuzzer
- Toolchain: Clang 18.1.3 with AddressSanitizer and UndefinedBehaviorSanitizer under WSL

## Finding

`fountain_decoder_sink::decode_frame()` permits multiple configured fountain
chunks in one call. It validated the first chunk's transfer metadata and the
overall byte ceiling, then passed the complete byte range to
`fountain_decoder_stream`. The stream discarded each chunk's first four header
bytes and used its block ID and payload, so later chunks could carry a different
encode ID or claimed object size without being rejected. Later block IDs also
had no per-policy numeric ceiling.

This was a policy-bypass and reconstruction-integrity defect. A hostile mixed
batch could feed payload from another transfer into the active Wirehair
context, even though separate calls with the same conflicting metadata were
rejected.

## Remediation

- Required `decode_frame()` input to be an exact multiple of the configured
  fountain chunk size.
- Parsed every packet header before allocating or mutating decoder state.
- Required every packet in a batch to match the first packet's full transfer
  ID, including encode ID and claimed object size.
- Added `maximum_block_id` to `FountainDecoderLimits` and enforced it for every
  packet before Wirehair.
- Added stable rejection results for misaligned frames, conflicting batched
  metadata, and excessive block IDs.
- Extended the state-machine fuzzer with an explicit mixed-batch operation and
  a product-sized block-ID limit.

## Verification

Focused fountain tests passed under ASan/UBSan:

```text
All tests passed (157076 assertions in 40 test cases)
```

The state-machine fuzzer completed a fresh smoke campaign under ASan/UBSan:

```text
10000 runs completed
3685 coverage points, 8621 features
162 MB peak reported RSS (256 MB limit)
no crash or sanitizer report
```

Regression cases cover rejection of a trailing byte, conflicting metadata in
the second packet of a batch, a block ID above policy, and zero decoder-state or
active-byte mutation after rejection. Existing multi-packet completion tests
continue to pass.

## Remaining Risk

- The generic default retains the full 16-bit block-ID range for compatibility;
  hardened callers must select a smaller limit as part of their transfer
  policy.
- Wirehair's internal allocation overhead is still not charged to the explicit
  decoder byte budget.
- The restricted single-take opaque-object API and product build profile remain
  to be implemented.
