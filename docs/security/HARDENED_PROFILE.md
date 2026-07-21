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
partial state; explicit reset is required before reuse.

Generic file/decompression callbacks remain available only through
`fountain_decoder_file_sink.h` for compatibility applications. The restricted
header does not include those dependencies. The full raw-frame `DecoderSession`
and dedicated product build target remain outstanding.

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
- Bounded duplicate tracking.
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

## Authentication boundary

The Secure Core must copy the completed bytes into its own bounded memory, parse the outer envelope, authenticate the object, and only then parse application content. Cimbar reconstruction alone never authorizes an action.
