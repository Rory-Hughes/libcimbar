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

## Questions requiring desktop investigation

- Exact dependency revisions and whether vendored copies diverge from upstream.
- Maximum allocation performed by Wirehair for each supported object size.
- Maximum number of distinct block IDs accepted before completion.
- Whether current stream-slot reuse can mix incompatible transfer state.
- Whether OpenCV exceptions are consistently caught at the process boundary.
- Whether decoder classes are safe to destroy after partial initialization.
- Whether all output-write failures propagate to the caller.
- Whether any global initialization is unsafe under repeated process restart or concurrency.
