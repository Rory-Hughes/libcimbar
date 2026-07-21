# Fuzzing Workspace

This directory contains security-focused fuzzing scaffolding for the hardened libcimbar fork. It is intentionally separate from the upstream build until harnesses and dependency assumptions are validated.

## Current targets

`fuzz_fountain_metadata` exercises the six-byte fountain metadata parser and its field accessors under libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.

`fuzz_fountain_state` drives bounded start, frame, recover, and reset operations through `fountain_decoder_sink`. It uses a 64 KiB object limit, one active stream, 128 unique blocks, and two retained completion records, so arbitrary inputs cannot trigger large decoder allocations.

Build locally with a recent Clang and CMake:

```bash
cmake -S fuzz -B out/fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build out/fuzz
./out/fuzz/fuzz_fountain_metadata -runs=100000
./out/fuzz/fuzz_fountain_state -runs=10000 -max_len=512
```

## Planned targets

1. `fuzz_fountain_metadata`
   - Raw metadata parsing and canonical field round trip.
2. `fuzz_corrected_payload`
   - Fast fuzzing after symbol and ECC extraction but before fountain reconstruction.
3. `fuzz_raw_frame`
   - Fixed-format raw image input through geometry and symbol extraction.
4. `fuzz_frame_sequence`
   - Reordered, duplicated, dropped, delayed, and mutated camera frames.
5. `fuzz_zstd_filename_upstream`
   - Audit-only coverage for generic upstream decompression and filename paths; excluded from the product profile.

## Harness rules

- One narrow concern per target.
- No network or filesystem side effects.
- Deterministic initial state.
- Explicit size caps before calling expensive dependencies.
- No logging on ordinary rejection paths.
- Assertions must express product invariants, not assumptions about hostile input validity.
- Every fixed crash becomes a permanent regression corpus entry.

## Corpus layout

Planned directories:

```text
fuzz/corpus/fountain_metadata/
fuzz/corpus/fountain_state/
fuzz/corpus/corrected_payload/
fuzz/corpus/raw_frame/
fuzz/corpus/frame_sequence/
```

Do not publicly commit a crash input that exposes an undisclosed exploitable vulnerability. Store private reproducers in the coordinated finding process described in `SECURITY.md`.

## Resource controls

Continuous runs should apply:

- Per-input timeout.
- RSS limit.
- Maximum input length.
- Sanitizer halt-on-error.
- Corpus minimization.
- Artifact-prefix isolation.

The stateful fountain harness must additionally assert that active decoder count, seen-block state, output size, and transfer lifetime remain within the selected policy.
