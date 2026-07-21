# Image And Transfer Hardening Evidence

## Scope

- Date: 2026-07-21
- Components: embedded bitmap decoding and generic fountain decoder limits
- Build: `windows-mingw-vcpkg`

## Embedded Image Decoder

The embedded image loader now validates the encoded length, dimensions,
channel metadata, and pixel count before decoding. It requests a fixed RGBA
layout from stb_image, releases that allocation with `stbi_image_free`, and
returns an owning three-channel OpenCV matrix. The decoder rejects dimensions
over 4,096 and images over 16,777,216 pixels.

Regression coverage includes RGB and RGBA PNG input plus null, empty,
malformed, and non-representable encoded lengths. A focused cppcheck scan of
`Common.cpp` completed without the previous `arithOperationsOnVoidPointer`
report.

## Fountain Transfer Limits

`FountainDecoderLimits` now bounds each generic decoder by default to:

- 33,554,431 aggregate claimed active object bytes;
- 16 fountain packets in one `decode_frame` call;
- 64,000 frame calls per transfer;
- 1,024 consecutive frame calls without decoder progress; and
- one hour of monotonic transfer duration; and
- eight retained cancellation tombstones.

Reaching the frame or no-progress limit frees the active decoder and retains a
bounded tombstone with the rejection reason. Replayed frames for that transfer
receive the same rejection instead of recreating decoder state. `reset()`
clears active streams, progress budgets, completion records, and cancellation
records deterministically.

Aggregate active object bytes are checked before constructing a new Wirehair
decoder. Transfer age uses `steady_clock`; callers may invoke an explicit
expiry sweep, and every frame submission also performs a sweep. Tests inject a
deterministic clock and do not depend on wall-clock sleeps.

Regression coverage verifies oversized frame rejection, useful traffic at the
frame limit, no-progress cancellation, replay rejection, bounded tombstone
retention, and reset behavior.

Additional regression coverage verifies the exact duration boundary, timeout
tombstone replay, byte-accounting release, pre-allocation aggregate-byte
rejection, and reset of aggregate accounting.

## Verification

- Full MinGW build: passed.
- CTest: 9 of 9 test executables passed.
- Focused `Common.cpp` cppcheck scan: passed with no findings.
- Focused fountain policy cppcheck scan: passed with no findings.
- `git diff --check`: passed; Git reported only existing line-ending notices.

## Remaining Work

WP-06 still requires Secure-Core-selected object classes, exact third-party
codec memory accounting or an equivalent hard process budget, and an
at-most-once completion contract that is not weakened when bounded completion
records age out.
