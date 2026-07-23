# Hardened Product Profile

## Component name

Working name: `libcimbar-transport`.

The component is an optical byte-transport service, not a generic file-transfer application.

## Retained features

- Fixed raw-camera-frame input.
- One selected cimbar mode for the first product iteration.
- Required geometry and anchor extraction.
- Tile and colour classification.
- Reed-Solomon correction.
- Fountain reconstruction only if reliability benchmarks justify it.
- Opaque byte-object output.
- Encoder support required for outgoing optical transfer.
- Small bounded health and progress counters.

## Removed features

The production profile must compile out:

- Arbitrary PNG, JPEG, or other image-file loading.
- Command-line parsing.
- Generic GUI code.
- WASM and browser integration.
- Arbitrary file transfer.
- Filename metadata.
- Output-directory selection.
- Arbitrary filesystem writes.
- zstd compression and decompression for messages and wallet data.
- Runtime mode selection.
- Unused cimbar modes.
- Debug image dumping.
- Environment-variable configuration.
- Dynamic plugin loading.

## Proposed API

```cpp
struct TransferPolicy {
    ObjectClass object_class;
    std::size_t maximum_object_size;
    std::uint32_t maximum_blocks;
    std::uint32_t maximum_block_id;
    std::uint32_t maximum_frames;
    std::uint32_t maximum_no_progress_frames;
    std::chrono::milliseconds maximum_duration;
    std::size_t memory_budget;
};

class DecoderSession {
public:
    static Result<DecoderSession> create(const TransferPolicy& policy);
    FrameResult submit_frame(const RawFrame& frame);
    TransferStatus status() const;
    Result<BoundedObject> take_completed_object();
    void cancel();
};
```

The final API may differ, but it must preserve explicit policy, bounded output, single completion, deterministic cancellation, and no filesystem semantics.

### Implemented fountain-stage boundary

`fountain_decoder_session` now provides the reconstruction-stage portion of
this contract. Construction requires a valid `FountainTransferPolicy` with an
explicit object class and decoder limits. The session exposes only status,
frame submission, cancellation/reset, and a single-take exact-length byte
vector. It has no output callback, filename, path, decompression, or filesystem
interface. Any negative decoder result fails the session closed and clears
partial state; explicit reset is required before reuse. Decoder-state and
completed-output allocation failures are contained at this boundary, as is
output recovery refusal. None can expose a partial object, and all require an
explicit reset before the session accepts another frame.

The policy must also select equal, non-zero
`maximum_codec_memory_bytes` and `maximum_active_codec_memory_bytes` values.
Before constructing Wirehair, the sink reserves a conservative decoder heap
bound calculated inside the vendored codec. It covers the codec object, input
staging, peeling workspace, worst-case deferred solve matrix, pivot/heavy data,
and Wirehair's explicit alignment overhead. Admission over either the
per-stream or aggregate budget fails without creating stream state. This is a
codec-owned heap bound, not a replacement for a process memory limit. Duplicate
tracking uses a policy-sized fixed-index bitmap after block-ID validation; its
absolute 16-bit protocol maximum is 8 KiB per decoder and smaller product
policies allocate proportionally less. With completed-transfer retention set to
zero, successful recovery also skips compatibility filename construction.

Generic file/decompression callbacks remain available only through
`fountain_decoder_file_sink.h` for compatibility applications. The restricted
header does not include those dependencies.

### Compiled hardened transport artifact

`cimbar_hardened_transport` is the compiled, byte-only product-profile façade
for corrected fountain packets. Its public methods are limited to explicit
policy construction, status, frame submission, one exact-byte take,
cancellation, and reset. The CMake target links only Wirehair.

Build and verify the release profile with:

```bash
cmake --preset hardened-transport
cmake --build --preset hardened-transport
ctest --preset hardened-transport
```

Every archive and linked profile-test build runs
`cmake/VerifyHardenedTransportArtifact.cmake`. The build fails if artifact
symbols or strings contain zstd, decompression callbacks, filename parsers,
`std::filesystem`, file streams, browser compatibility output functions, or
session test-fault controls. The named preset builds the always-executed
`verify_hardened_transport` target, so changing only the verifier still forces
a fresh scan.
The profile test reconstructs exact message and wallet bytes and refuses a
second take.

The full raw-frame `DecoderSession` that composes checked camera ingress with
this transport artifact remains outstanding. The current top-level configure
also discovers OpenCV even when only the transport target is built, although
OpenCV is not linked into the artifact.

## Initial object profiles

These values are provisional and require benchmarking.

| Profile | Maximum reconstructed object | Active transfers | Compression | Filesystem |
|---|---:|---:|---|---|
| Pairing | 32 KiB | 1 | none | none |
| Message | 64 KiB | 1 | none | none |
| Wallet | 256 KiB | 1 | none | none |
| Firmware update | hardware partition bound | 1 | separately reviewed | fixed staging area only |

The Secure Core selects the profile before scanning starts. Optical metadata can never increase the selected limit.

## State rules

- One active reconstruction in the initial version.
- No persistent completed-object map.
- Policy-sized duplicate bitmap, capped at 8 KiB per decoder.
- Conflicting object size or transfer identity invalidates the session.
- No-progress threshold aborts the session.
- Timeout aborts the session.
- Output is available once and is removed from decoder ownership after transfer.
- Success, rejection, cancellation, timeout, IPC loss, and process shutdown all clear state.

## Input rules

- Accept only documented raw pixel formats.
- Reject zero dimensions.
- Reject unsupported channel counts and strides.
- Reject any frame whose buffer length does not exactly satisfy its dimensions and format.
- Apply checked arithmetic to every dimension and length calculation.
- Treat NaN, infinity, singular transforms, and invalid regions of interest as rejection conditions.

## Output rules

The decoder may return only:

- Status.
- Transfer generation.
- Exact byte length.
- Opaque output bytes.
- Optional transport digest.
- Small coarse diagnostic code.

It must never return or create:

- A filename.
- A path.
- A MIME type.
- A command.
- An application route.
- A wallet instruction.
- A message recipient.
- An executable object.

## Deployment rules

The decoder process or processor domain must have:

- No cryptographic keys.
- No message plaintext.
- No wallet data.
- No network access.
- No arbitrary filesystem write access.
- A fixed camera interface.
- A fixed IPC endpoint.
- Memory and CPU limits.
- A watchdog.
- Process restart after each transfer where practical.

### Linux sandbox prototype

The `hardened-transport` preset now enables the opt-in
`LIBCIMBAR_BUILD_LINUX_SANDBOX_PROTOTYPE` target on Linux x86_64. The
`hardened_transport_sandbox_probe` executable is not linked into
`cimbar_hardened_transport`; it is a deployment prototype that exercises the
same corrected-packet `HardenedFountainTransport` boundary in a restricted
child process.

For each transfer, the parent forks a new child and enforces a watchdog. The
child drops root to uid/gid 65534 when needed, verifies non-root execution and
zero effective capabilities, sets `no_new_privs`, disables core dumps, applies
memory, CPU, process-count, file-size, core, and descriptor limits, attempts
network namespace isolation where permitted, and installs a seccomp-BPF
allowlist before decoding. After exact reconstruction, deliberate escape probes
for parent-secret file reads/writes, sockets, fork, exec, and uid 0 regain must
all fail.

This is the WP-11 process-isolation baseline for the future Secure Core split.
The final service still needs the fixed IPC contract, parser fuzzing, and
Secure Core authentication described in WP-12.

## Authentication boundary

The Secure Core must copy the completed bytes into its own bounded memory, parse the outer envelope, authenticate the object, and only then parse application content. Cimbar reconstruction alone never authorizes an action.
