# Fuzzing Workspace

This directory contains security-focused fuzzing scaffolding for the hardened libcimbar fork. It is intentionally separate from the upstream build until harnesses and dependency assumptions are validated.

## Current targets

`fuzz_fountain_metadata` exercises the six-byte fountain metadata parser and its field accessors under libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.

`fuzz_fountain_state` drives raw and structured frames, mixed batched metadata, duplicate and conflicting blocks, recovery, no-progress cancellation, timeout, completion, replay, and reset through `fountain_decoder_sink`. It uses a 4 KiB object limit, one active stream, a Wirehair heap budget derived from the worst-case 4 KiB decoder estimate, 64 unique blocks, a maximum block ID of 96, four packets per frame, bounded transfer budgets, two retained completion details, and fixed terminal encode-ID tracking, so arbitrary inputs cannot trigger large decoder allocations, bypass packet policy inside a batch, or duplicate successful output within a session.

`fuzz_frame_sequence` starts from a bounded source object, encodes canonical fountain packets, and then submits adversarial frame sequences through the restricted `fountain_decoder_session`. The schedule supports in-order completion, reordering, dropping, duplication, batching, transfer-delay expiry, reset/replay, and rejected packet-envelope mutation. Any completed object must match the original source bytes exactly, completion may happen at most once before reset, and reset must return the session to a clean receiving state.

`fuzz_raw_frame` exercises the checked receive ABI, raw RGB/RGBA/NV12/I420 size validation, OpenCV conversion, scanner edge probing, midpoint geometry, and perspective deskew rejection. It caps generated fuzz frames at 96 x 96 pixels, while still sending malformed dimensions, unsupported formats, mismatched byte lengths, null input, degenerate corners, NaN, infinity, and extreme line coordinates through the same validation boundaries.

Build locally with a recent Clang and CMake:

```bash
cmake -S fuzz -B out/fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build out/fuzz
python3 fuzz/corpus/generate.py --check
mkdir -p out/fuzz-corpus/fountain_metadata out/fuzz-corpus/fountain_state out/fuzz-corpus/frame_sequence
cp fuzz/corpus/fountain_metadata/*.bin out/fuzz-corpus/fountain_metadata/
cp fuzz/corpus/fountain_state/*.bin out/fuzz-corpus/fountain_state/
cp fuzz/corpus/frame_sequence/*.bin out/fuzz-corpus/frame_sequence/
./out/fuzz/fuzz_fountain_metadata out/fuzz-corpus/fountain_metadata -runs=100000
./out/fuzz/fuzz_fountain_state out/fuzz-corpus/fountain_state -runs=10000 -max_len=512
./out/fuzz/fuzz_frame_sequence out/fuzz-corpus/frame_sequence -runs=5000 -max_len=1024 -rss_limit_mb=256 -timeout=5
mkdir -p out/fuzz-corpus/raw_frame
cp fuzz/corpus/raw_frame/*.bin out/fuzz-corpus/raw_frame/
./out/fuzz/fuzz_raw_frame out/fuzz-corpus/raw_frame -runs=1000 -max_len=4096 -timeout=2
```

The 96 x 96 raw-frame fuzzer is intentionally smaller than the public frame
limit. Measure the checked ABI's full-size CPU budget from an optimized normal
build instead:

```bash
cmake -S . -B out/build/raw-frame-cpu -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIBCIMBAR_BUILD_GUI_TOOLS=OFF
cmake --build out/build/raw-frame-cpu --target cimbar_raw_frame_cpu_budget
./out/build/raw-frame-cpu/build/src/lib/cimbar_js/test/cimbar_raw_frame_cpu_budget --iterations 3
```

The command disables OpenCL, restricts OpenCV to one thread, and returns a
nonzero status when one of five full-size hostile RGB patterns exceeds the
default 250 ms budget. The budget is hardware-specific; record a new baseline
when the CPU class, compiler, OpenCV version, or frame policy changes.

## Planned targets

1. `fuzz_fountain_metadata`
   - Raw metadata parsing and canonical field round trip.
2. `fuzz_corrected_payload`
   - Fast fuzzing after symbol and ECC extraction but before fountain reconstruction.
3. `fuzz_zstd_filename_upstream`
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

The committed fountain corpora are generated deterministically by
`fuzz/corpus/generate.py`. `fuzz/corpus/manifest.json` records each seed's
target, size, SHA-256 digest, and security purpose. Regenerate after an
intentional corpus change and use `--check` in CI or review to detect missing,
modified, or unexpected binary entries.

```text
fuzz/corpus/fountain_metadata/
fuzz/corpus/fountain_state/
fuzz/corpus/raw_frame/
fuzz/corpus/corrected_payload/
fuzz/corpus/frame_sequence/
```

The metadata corpus covers empty and truncated headers, ordinary values,
high-bit encoding, and all maximum-width fields. The state corpus preserves
regressions for short/misaligned frames, invalid one-block configurations,
block-policy rejection, full-ID stream-slot conflicts, duplicate/no-progress
cancellation, timeout/reset cleanup, mixed batched metadata, completion replay
after detail eviction, and allowed reuse only after explicit reset. The
raw-frame corpus covers valid RGB/RGBA/NV12/I420 envelopes, cropped and rotated
pattern frames, noisy and overexposed frames, damaged byte lengths, odd
subsampled dimensions, null-input selection, unsupported formats, and
non-finite or degenerate geometry seeds. The frame-sequence corpus covers
canonical completion, reordering, duplicate replay, dropping with delay expiry,
mutated packet envelopes, batched frames, and explicit reset/replay.

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
