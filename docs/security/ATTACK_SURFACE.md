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
| Browser frame to C boundary | `web/recv-worker.js:RecvWorker.on_frame` -> `cimbard_scan_extract_decode_checked` | JavaScript copies the camera frame into WASM memory and passes its exact byte length. RGB, RGBA, NV12, and I420 are accepted; subsampled dimensions must be even; tightly packed size must match exactly; total pixels are capped at 4096 x 4096. | Invalid calls return a named negative result before OpenCV. | Reviewed and unit-tested. Fixed product dimensions and stride policy remain to be selected. |
| Raw bytes to owned RGB image | `expected_frame_size` -> `get_rgb` -> `cv::Mat::clone` / `cv::cvtColor` | Input is borrowed only until `clone`; OpenCV owns the resulting `UMat`. Checked 64-bit arithmetic precedes conversion to signed OpenCV dimensions. | `cv::Exception`, `std::exception`, and unknown exceptions are contained at the C ABI and return `CIMBARD_SCAN_PROCESSING_ERROR`. | Reviewed for the checked API. The compatibility wrapper cannot validate actual input length. |
| Frame to anchors | `Extractor::extract` -> `Scanner::Scanner` -> `Scanner::preprocess_image` -> `Scanner::scan_primary` / `add_bottom_right_corner` | Scanner creates a grayscale image and candidate vectors bounded indirectly by the checked frame limit. Pixel access is against scanner-selected ranges. Candidate deduplication is quadratic in candidate count. | Fewer than four anchors returns `Extractor::FAILURE`. Scanner and OpenCV objects are per-call. | Existing unit coverage; no raw-frame fuzzer or measured CPU ceiling. |
| Anchors to normalized image | `Corners` -> `Deskewer::deskew` -> `cv::getPerspectiveTransform` -> `cv::warpPerspective` | Four selected anchor centers drive a transform into a configuration-sized output image. | The checked C entry contains OpenCV exceptions. Generic CLI entry points do not consistently contain them. | Geometry validity, finite transform values, and worst-case CPU remain open. |
| Normalized image to cell symbols | `Decoder::decode_fountain` -> `CimbReader::CimbReader` / `preprocessSymbolGrid` -> `read` / `read_color` | Reader owns preprocessed symbol/color buffers and iterates a configuration-derived fixed cell count. Sampling coordinates derive from the normalized grid. | Decode state is local to one frame. Configuration is thread-local, but receive globals are not. | Unit-tested with valid and damaged samples; direct malformed-grid fuzzing remains open. |
| Symbols to corrected payload | `Decoder::do_decode` -> `bitbuffer::flush` -> `reed_solomon_stream::write` -> `ReedSolomon::decode` | Symbol and color capacities, ECC block size, and interleave maps come from the selected configuration. Temporary vectors are sized from the fixed read count. | Failed ECC blocks become `BadChunk`; `aligned_stream::mark_bad_chunk` drops the affected aligned packet instead of forwarding partial bytes. | Reviewed and unit-tested; corrected-payload fuzz target remains open. |
| Corrected bytes to frame packets | `aligned_stream::flush` / `CimbReader::update_metadata` -> `escrow_buffer_writer::write` | Packets are aligned to configured fountain chunk size. Escrow output is caller-owned and limited by the configured chunk count checked at the C boundary. Metadata parsing is six bytes. | A bad or incomplete packet is not forwarded. Escrow refuses a wrong-sized packet or exhausted output slot. | Reviewed and covered by alignment, ECC, and end-to-end tests. |
| Packet metadata to transfer context | `cimbard_fountain_decode` -> `fountain_decoder_sink::decode_frame` -> `FountainMetadata` | Frame pointer, exact chunk alignment, object size, aggregate object bytes, active streams, packet count, transfer slot/full ID, and every packet's block ID are checked before decoder construction. All packets batched in one call must carry the same full transfer ID. | Conflicting or out-of-policy packets reject the whole call before state mutation. Completed encode IDs remain terminal until explicit reset; cancellation records are bounded. Frame, no-progress, and duration limits deterministically cancel and release state. | Reviewed, unit-tested, and state-fuzzed, including mixed batched metadata and at-most-once completion. |
| Packet payload to Wirehair | `fountain_decoder_stream::write` / `decode` -> `FountainDecoder::decode` -> `wirehair_decode` | A fixed chunk buffer strips the six-byte header only after the sink validates every aligned packet. Unique block tracking is bounded; duplicate IDs are rejected before Wirehair. Object size, packet size, and maximum block ID are policy inputs. | Wirehair result controls completion; move/destructor paths release the codec. | ASan/UBSan clean after alignment backports. Exact codec overhead is not charged to the decoder memory budget. |
| Completed transfer to bytes | `fountain_decoder_session::submit_frame` / `take_completed_object` -> `fountain_decoder_sink::recover` -> `FountainDecoder::recover` -> `wirehair_recover` | The restricted session requires a Secure-Core-selected object class and limits, allocates exactly the validated object length, and owns the completed vector until one move-only take. It exposes no callback, filename, decompression, or filesystem operation. | Any decoder rejection fails the session closed and clears partial state. Success becomes terminal until the object is taken or the session is explicitly cancelled/reset. A second take returns no object. | Exact output, single-take, invalid policy, fail-closed rejection, cancellation, and reset are unit-tested. Deterministic allocation-failure injection remains open. |
| Reassembled bytes to decompressed stream | `recover_contents` -> `init_decompress` -> `cimbard_decompress_read` -> `zstd_decompressor::write_once` | Generic receive state retains compressed bytes and a streaming zstd context. Input is bounded by transfer policy, but decompressed output has no total size bound. | One process-global decompressor and object buffer are retained. zstd errors become negative API results. | Audit-only generic path. Remove from message/wallet profile; decompression-bomb risk remains. |
| Metadata filename to filesystem | `cimbard_get_filename`, `zstd_header_check::get_filename`, `File::basename`, `decompress_on_store` / `write_on_store` in `fountain_decoder_file_sink.h` | Filename comes from compressed content; basename strips directory components. Generic callbacks construct a path under a caller-selected directory and allocate a full recovered vector before writing. The core sink and restricted session no longer include these helpers or their zstd/filesystem headers. | Write/decompression failures are not consistently surfaced by every generic callback. Files may be partially written. | Isolated to an explicit compatibility header; it must remain absent from the hardened product target. |
| Persistent receiver state | `_sink`, `_decId`, `_reassembled`, `_dec`, `_debugFrame`, timers, reporting string | Process-global objects retain decoder, recovered object, debug image, and diagnostics across calls. | `cimbard_configure_decode` resets only the sink when mode changes. Calls are not synchronized. | ThreadSanitizer and explicit reset/ownership API remain open. Separate WASM workers isolate instances but do not make the C API thread-safe. |

## Review conclusions

- The checked browser ingress rejects malformed dimensions, lengths, formats,
  and null buffers before OpenCV allocation.
- Frame-to-packet allocations are configuration-sized after the raw-frame cap.
- Metadata-derived decoder allocation is policy-checked before Wirehair creation.
- The product profile still requires fixed camera dimensions/stride, bounded
  geometry CPU, a total decoder memory budget, opaque single-take output, and
  removal of decompression, filename, and filesystem paths.
- The generic C receive API is stateful and not thread-safe.

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
- Maximum allocation performed by Wirehair for each supported object size.
- Whether OpenCV exceptions are consistently caught at the process boundary.
- Whether all output-write failures propagate to the caller.
- Whether any global initialization is unsafe under repeated process restart or concurrency.
