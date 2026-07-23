# Attack-Surface Map

## End-to-end decode path

```text
Camera hardware
  -> camera driver
  -> raw frame buffer
  -> pixel conversion
  -> geometry and anchor detection
  -> deskew / distortion correction
  -> tile and colour classification
  -> cimbar symbol decoding
  -> Reed-Solomon correction
  -> frame metadata parsing
  -> transfer-context selection
  -> Wirehair fountain processing
  -> object reconstruction
  -> optional decompression
  -> optional filename processing
  -> output sink
```

Every stage receives data influenced by an attacker-controlled display.

## Review table

| Stage | Inputs to bound | Failure concerns | Product disposition |
|---|---|---|---|
| Camera/raw frame | width, height, stride, pixel format, buffer length | overflow, invalid stride, oversized allocation | retain fixed formats only |
| OpenCV conversion | channel count, empty matrices, continuity | exceptions, out-of-range ROI, dependency vulnerabilities | minimize operations |
| Geometry extraction | coordinates, contours, transforms | NaN, singular transform, worst-case CPU | retain and fuzz heavily |
| Tile classification | grid sizes, sample coordinates, colour values | out-of-bounds reads, unstable classification | retain one mode initially |
| ECC | codeword size, error count, corrected length | corruption, partial output, unchecked status | retain with strict failure handling |
| Metadata | object size, transfer ID, block ID | allocation abuse, truncation, state collision | replace implicit trust with policy |
| Fountain/Wirehair | block count, unique IDs, packet size | memory growth, CPU exhaustion, state confusion | wrap and bound |
| zstd | input size, output size, frame structure | decompression bomb, trailing data | remove from message/wallet profile |
| Filename handling | encoded filename, path syntax | traversal, overwrite, Unicode issues | remove from product profile |
| Filesystem sink | location, permissions, free space | arbitrary write, partial files, disk exhaustion | remove from product profile |
| IPC output | type, length, transfer generation | partial output, duplicate output, parser confusion | fixed typed protocol |

## Function-level hostile-input map

The browser receive path now uses the length-aware API. The older API remains
for compatibility but cannot prove the size of caller-owned memory and is not
eligible for the hardened product profile.

| Transition | Source symbols | Ownership, bounds, and allocation | State and failure behaviour | Review status |
|---|---|---|---|---|
| Compatibility file to owned RGB image | CLI / `DecoderPlus` / `DeskewerPlus` / `ExtractorPlus` -> `cimbar::load_img_file` -> `detail::decode_image` | Encoded files are capped at 64 MiB before allocation; stb_image v2.30 validates metadata; width and height are each capped at 4096, decoded images at 16,777,216 pixels, and the returned `cv::Mat` owns exactly three RGB channels. Production code contains no `cv::imread` or `cv::imdecode` call. | Missing, empty, oversized, malformed, short-read, and expected allocation failures return an empty image. Callers stop before output creation or image processing. No decode state persists between calls. | Valid/missing-file regression and compatibility decode/extract tests pass under ASan/UBSan. Seven package-level OpenCV findings have exact reachability exceptions expiring 2026-10-22; CI rejects reintroduction of affected production calls. |
| Browser frame to C boundary | `web/recv-worker.js:RecvWorker.on_frame` -> `cimbard_scan_extract_decode_checked` | JavaScript copies the camera frame into WASM memory and passes its exact byte length. RGB, RGBA, NV12, and I420 are accepted; subsampled dimensions must be even; tightly packed size must match exactly; total pixels are capped at 4096 x 4096. | Invalid calls return a named negative result before OpenCV. The release `cimbar_raw_frame_cpu_budget` command measures five full-size hostile RGB patterns with OpenCV restricted to one thread and enforces a 250 ms per-frame ceiling on the reference environment. | Reviewed, unit-tested, fuzzed, and CPU-budgeted. Fixed product dimensions and stride policy remain to be selected. |
| Raw bytes to owned RGB image | `expected_frame_size` -> `get_rgb` -> `cv::Mat::clone` / `cv::cvtColor` | Input is borrowed only until `clone`; OpenCV owns the resulting `UMat`. Checked 64-bit arithmetic precedes conversion to signed OpenCV dimensions. | `cv::Exception`, `std::exception`, and unknown exceptions are contained at the C ABI and return `CIMBARD_SCAN_PROCESSING_ERROR`. | Reviewed for the checked API. The compatibility wrapper cannot validate actual input length. |
| Frame to anchors | `Extractor::extract` -> `Scanner::Scanner` -> `Scanner::preprocess_image` -> `Scanner::scan_primary` / `add_bottom_right_corner` | Scanner creates a grayscale image and candidate vectors bounded indirectly by the checked frame limit. Pixel access fails closed outside the current matrix. Candidate confirmation vectors are capped at 64 entries per stage before nested scans and candidate comparison. | Fewer than four anchors returns `Extractor::FAILURE`. Scanner and OpenCV objects are per-call. Degenerate bottom-right recovery inputs return false. | Unit-tested, covered by `fuzz_raw_frame`, and exercised by the 250 ms full-size raw-frame CPU budget. |
| Anchors to normalized image | `Corners` -> `Deskewer::deskew` -> `cv::getPerspectiveTransform` -> `cv::warpPerspective` | Four selected anchor centers drive a transform into a configuration-sized output image. Empty images, invalid output geometry, non-finite transforms, and singular transforms return an empty matrix before warp. | The checked C entry contains OpenCV exceptions. Generic CLI entry points stop on empty decoded images, and deskew rejects invalid geometry before allocation or warp. | Unit-tested, covered by `fuzz_raw_frame`, and included in the 250 ms full-size raw-frame CPU budget. |
| Normalized image to cell symbols | `Decoder::decode_fountain` -> `CimbReader::CimbReader` / `preprocessSymbolGrid` -> `read` / `read_color` -> `Cell::mean_rgb` | Reader owns preprocessed symbol/color buffers and iterates a configuration-derived fixed cell count. `Cell` validates matrix depth, channels, positive dimensions, and complete ROI containment before row access; byte offsets use `std::size_t` and totals use 64-bit accumulators. | Invalid regions return a zero sample without reading. Continuous and strided matrices share the same row-wise traversal. Decode state is local to one frame. | Rectangular strided, large accumulator, grayscale subregion, and out-of-bounds regressions pass under ASan/UBSan. Direct malformed-grid fuzzing remains open. |
| Symbols to corrected payload | `Decoder::do_decode` -> `bitbuffer::flush` -> `reed_solomon_stream::write` -> `ReedSolomon::decode` | Symbol and color capacities, ECC block size, and interleave maps come from the selected configuration. Temporary vectors are sized from the fixed read count. | Failed ECC blocks become `BadChunk`; `aligned_stream::mark_bad_chunk` drops the affected aligned packet instead of forwarding partial bytes. | Reviewed and unit-tested; corrected-payload fuzz target remains open. |
| Corrected bytes to frame packets | `aligned_stream::flush` / `CimbReader::update_metadata` -> `escrow_buffer_writer::write` | Packets are aligned to configured fountain chunk size. Escrow output is caller-owned and limited by the configured chunk count checked at the C boundary. Metadata parsing is six bytes. | A bad or incomplete packet is not forwarded. Escrow refuses a wrong-sized packet or exhausted output slot. | Reviewed and covered by alignment, ECC, and end-to-end tests. |
| Packet metadata to transfer context | `cimbard_fountain_decode` -> `fountain_decoder_sink::decode_frame` -> `FountainMetadata` | Frame pointer, exact chunk alignment, object size, aggregate object bytes, active streams, packet count, transfer slot/full ID, and every packet's block ID are checked before decoder construction. All packets batched in one call must carry the same full transfer ID. | Conflicting or out-of-policy packets reject the whole call before state mutation. Completed encode IDs remain terminal until explicit reset; cancellation records are bounded. Frame, no-progress, and duration limits deterministically cancel and release state. | Reviewed, unit-tested, and state-fuzzed, including mixed batched metadata and at-most-once completion. |
| Packet payload to Wirehair | `FountainDecoder::decoder_memory_required` -> `wirehair_decoder_memory_required`; then `fountain_decoder_stream::write` / `decode` -> `FountainDecoder::decode` -> `wirehair_decode` | Before stream construction, object and packet sizes select a conservative Wirehair heap reservation covering the codec object, input, workspace, worst-case deferred solve matrix, pivots/heavy data, and allocator alignment. Per-stream and aggregate codec budgets are enforced in addition to object bytes. Wirehair calculates and validates its block count in 64 bits before narrowing, caps block size to the signed length accepted by gf256, and checks matrix offsets on 32-bit hosts. A fixed chunk buffer strips the six-byte header only after the sink validates every aligned packet. A policy-sized block bitmap rejects duplicates before Wirehair and uses at most 8 KiB for the wire format's 16-bit block identifier. | Rejection occurs before stream state or codec allocation. Reservations are released on completion, cancellation, expiry, failure, or reset. `wirehair_decoder_memory_allocated` reports live codec-owned bytes for regression verification. Completion without retained history performs no filename construction. | Unit-tested at below-bound, exact-bound, aggregate-limit, block-ID boundary, pre-narrowing block-count overflow, reset, and post-matrix-allocation paths under ASan/UBSan. The byte-only profile also passes focused MemorySanitizer execution. The codec bound deliberately assumes every input column is deferred. |
| Completed transfer to bytes | `HardenedFountainTransport` -> `fountain_decoder_session::submit_frame` / `take_completed_object` -> `fountain_decoder_sink::recover` -> `FountainDecoder::recover` -> `wirehair_recover` | The compiled profile façade requires a Secure-Core-selected object class and limits, allocates exactly the validated object length, and owns the completed vector until one move-only take. It exposes no callback, filename, decompression, filesystem, output-directory, or application-route operation. | Decoder-state or output allocation exceptions and output recovery refusal fail the session closed and clear partial state. Success becomes terminal until the object is taken or the session is explicitly cancelled/reset. A second take returns no object. | Message and wallet exact-output tests pass under the release profile and ASan/UBSan. `fuzz_frame_sequence` drives encoded reorder/drop/duplicate/delay/envelope-mutation/reset schedules through the restricted session. The archive and linked profile test are scanned after every build. |
| Sandboxed decoder child | Secure Core parent -> `hardened_transport_sandbox_probe` child process -> `HardenedFountainTransport` | The Linux prototype forks one child per transfer, drops root when needed, verifies zero effective capabilities, sets `no_new_privs`, applies address-space, CPU, process-count, file-size, core, and descriptor limits, and installs a seccomp-BPF allowlist before decoding. Network namespace isolation is attempted where the kernel permits it; socket creation is denied by seccomp regardless. | The parent owns the watchdog and kills a stuck child. The child cannot open parent-created secrets, create files, create sockets, fork, exec, or regain uid 0 after sandboxing. Each transfer restart creates a fresh process. | WP-11 probe passes in the hardened transport preset on WSL Clang 18.1.3. The prototype is Linux x86_64 only and remains separate from the transport archive verifier. |
| IPC envelope to routed message | `parse_hardened_ipc_message` -> `HardenedIpcMessage` | IPC input is one fixed 24-byte little-endian envelope plus an optional opaque payload. Magic, version, type, reserved flags, nonzero transfer generation, exact payload length, fixed status code, full-message size, frame-payload size, and object-payload size are checked before routing. The parser allocates no memory and returns only payload offset and length. | Unknown versions/types/status codes, generation zero, length mismatch, reserved flags, over-limit payloads, missing required payloads, and payloads on control/status/failure messages all reject. There is no pointer, filename, nested object, free-form diagnostic, path, or application-command field. | Unit-tested in the hardened transport profile and independently fuzzed by `fuzz_hardened_ipc` with deterministic valid and fail-closed corpus seeds. The full contract is recorded in `docs/security/HARDENED_IPC.md`. |
| Reassembled bytes to decompressed stream | `recover_contents` -> `init_decompress` -> `cimbard_decompress_read` -> `zstd_decompressor::write_once` | Generic receive state retains compressed bytes and a streaming zstd context. Input is bounded by transfer policy, but decompressed output has no total size bound. | One process-global decompressor and object buffer are retained. zstd errors become negative API results. | Audit-only generic path. The hardened transport artifact verifier rejects zstd and decompression symbols/strings; decompression-bomb risk remains confined to compatibility targets. |
| Metadata filename to filesystem | `cimbard_get_filename`, `zstd_header_check::get_filename`, `File::basename`, `decompress_on_store` / `write_on_store` in `fountain_decoder_file_sink.h` | Filename comes from compressed content; basename strips directory components. Generic callbacks construct a path under a caller-selected directory and allocate a full recovered vector before writing. The hardened profile façade includes none of these operations. | Write/decompression failures are not consistently surfaced by every generic callback. Files may be partially written. | Compatibility-only. The hardened archive and linked profile test reject filename-parser, filesystem, file-stream, and output-callback symbols/strings. |
| Concurrent fountain submission | `concurrent_fountain_decoder_sink::write` / `process` / state accessors -> `fountain_decoder_sink` | Producers copy packets into a concurrent queue. Exactly one blocking, RAII-protected drainer mutates the decoder; state accessors use the same write lock and status snapshots use a fixed write-then-read lock order. | Queue allocation failure is reported, null input is rejected, callback exceptions release the lock, and every successful producer waits for a drain so the final queued packet cannot be stranded. Wirehair initialization uses one program-wide inline local static across translation units. | Four-writer/live-observer and cross-translation-unit initialization regressions pass under ThreadSanitizer. |
| Persistent compatibility receiver state | `_sink`, `_decId`, `_reassembled`, `_dec`, `_debugFrame`, timers, reporting string | Process-global objects retain decoder, recovered object, debug image, and diagnostics across calls. Configuration is thread-local, so a mutex alone would not create coherent shared ownership. | The C header now requires all `cimbard_*` receiver operations, reset, and reconfiguration to be serialized by one worker/thread. | Compatibility-only and explicitly not thread-safe. Separate WASM workers isolate instances. The hardened transport artifact does not link or expose this API. |

## Review conclusions

- The checked browser ingress rejects malformed dimensions, lengths, formats,
  and null buffers before OpenCV allocation.
- Compatibility file inputs use the bounded stb_image v2.30 path; production
  code contains no OpenCV file-decode entry point.
- Frame-to-packet allocations are configuration-sized after the raw-frame cap.
- Metadata-derived decoder allocation is policy-checked before Wirehair creation.
- The corrected-packet product artifact now has a total Wirehair decoder memory
  budget, opaque single-take output, and no decompression, filename, or
  filesystem path.
- The Linux sandbox prototype puts the corrected-packet decoder in a non-root,
  no-capability, seccomp-filtered child with resource limits, a watchdog, and
  per-transfer restart.
- The OIP-to-SCP IPC envelope is fixed-size, generation-scoped, and fuzzed
  independently; the full product still needs fixed camera dimensions/stride
  and Secure Core authentication integration.
- The concurrent fountain wrapper and Wirehair initialization pass focused
  ThreadSanitizer regressions. The generic C receive API remains stateful and
  explicitly single-worker only; it is excluded from the hardened artifact.

## State inventory

Document and bound all persistent or semi-persistent state:

- Current raw frame and derived image matrices.
- Extractor calibration and colour-correction state.
- Active transfer identity.
- Claimed object length.
- Fountain decoder allocation.
- Seen block identifiers.
- Progress counters.
- Completed-transfer records.
- Frame queues.
- Diagnostic counters.
- Output buffer.

## Source-review checklist

For every function reachable from frame input, record:

- File and symbol.
- Caller and callee.
- Input ownership.
- Trusted versus untrusted fields.
- Maximum size.
- Allocation behaviour.
- Arithmetic operations involving lengths or coordinates.
- Exception and error behaviour.
- Persistent state modified.
- Filesystem or IPC effects.
- Existing tests.
- Fuzz target covering the function.

## High-priority code areas

1. Image extraction and perspective correction.
2. Symbol decoding and corrected-payload construction.
3. `FountainMetadata` parsing and serialization.
4. `fountain_decoder_sink` context creation and stream-slot handling.
5. `fountain_decoder_stream` chunk assembly.
6. `FountainDecoder` duplicate tracking and Wirehair calls.
7. zstd streaming and output-length handling.
8. filename extraction and output path construction.
9. concurrent queue and receiver shutdown paths.
10. CLI and test-only paths that must be excluded from production builds.

## Remaining investigation

- Exact dependency revisions and whether vendored copies diverge from upstream.
- Hardware-specific resident-set and peak allocator measurements for each final
  product object profile; the code-level Wirehair heap upper bound is enforced.
- Whether OpenCV exceptions are consistently caught at the process boundary.
- Whether all output-write failures propagate to the caller.
- Repeated process restart and third-party callers that bypass
  `FountainInit::init()` and call Wirehair's raw global initializer directly.
