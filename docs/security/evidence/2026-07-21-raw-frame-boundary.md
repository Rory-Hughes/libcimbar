# Raw-Frame Boundary Hardening

## Scope

- Date: 2026-07-21
- Boundary: browser/WASM frame memory to OpenCV receive processing
- Components: `web/recv-worker.js`, `cimbar_recv_js.h/.cpp`, receive tests

## Finding

The legacy receive API accepted a frame pointer, unsigned dimensions, and a
format but no source allocation length. It converted dimensions to signed
OpenCV arguments, calculated YUV rows, cloned caller memory, and wrote decoded
packets through caller pointers without a complete validation boundary. C++ or
OpenCV exceptions could also cross the C ABI.

## Remediation

- Added `cimbard_scan_extract_decode_checked` with an exact source byte length.
- Restricted input to tightly packed RGB, RGBA, NV12, and I420.
- Required even width and height for NV12 and I420.
- Used checked 64-bit size arithmetic and capped generic frames at 16,777,216
  pixels before conversion to signed OpenCV dimensions.
- Rejected null input and output pointers before processing.
- Contained OpenCV, standard, and unknown exceptions at the C ABI.
- Migrated the browser worker to pass its actual `pixels.length`.
- Retained the legacy function as a compatibility wrapper; it is explicitly not
  approved for the hardened product profile.
- Corrected debug-frame copying to account for channel element size.

## Verification

The receive regression test covers null frame/output/report/debug pointers,
zero dimensions, unsupported formats, mismatched frame length, odd subsampled
dimensions, the pixel ceiling, insufficient output storage, null fountain
input, and null filename/decompression outputs.

```text
Windows MinGW cimbar_js_test: passed
Linux Clang ASan/UBSan cimbar_js_test: passed
```

## Remaining Risk

- The hardened product still needs one fixed camera format, dimensions, and
  stride contract rather than the generic four-format boundary.
- The compatibility API cannot validate actual source allocation length.
- Geometry CPU cost and finite/singular transform rejection require dedicated
  raw-frame fuzzing and resource measurements.
- Receive globals are not synchronized and remain a ThreadSanitizer target.
