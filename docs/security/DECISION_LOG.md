# Security Decision Log

Record significant audit and hardening decisions here. Do not silently change a security boundary.

## ADR-LC-001: Freeze the audit baseline

- **Status:** Accepted
- **Decision:** Use branch `audit-baseline-681e18e` at commit `681e18eb61a059f4a796bc6ef097d24b45c430eb` as the immutable initial reference.
- **Reason:** Findings, benchmarks, and remediation diffs require a stable target.
- **Consequence:** All fixes and scaffolding occur on other branches.

## ADR-LC-002: Treat optical input as hostile network input

- **Status:** Accepted
- **Decision:** Assume a compromised companion can intentionally generate every frame and transfer sequence.
- **Reason:** The air gap prevents ordinary network connectivity but does not authenticate displayed optical content.
- **Consequence:** All metadata, dimensions, geometry, block identifiers, timing, and output semantics require hostile-input controls.

## ADR-LC-003: Keep libcimbar outside the trusted core

- **Status:** Accepted
- **Decision:** Run camera and libcimbar processing in an Optical Interface Processor or isolated process with no secrets.
- **Reason:** Image and codec processing is a large attacker-controlled C/C++ surface.
- **Consequence:** Decoder compromise should not directly expose messaging or wallet keys.

## ADR-LC-004: Build a restricted transport profile

- **Status:** Accepted
- **Decision:** Product integration will use `libcimbar-transport`, not the generic file-transfer application.
- **Reason:** Filenames, generic filesystems, arbitrary image containers, compression, runtime mode selection, and GUI functionality are unnecessary for the secure transport boundary.
- **Consequence:** Compatibility with generic libcimbar file workflows is secondary to attack-surface reduction.

## ADR-LC-005: Secure Core selects resource policy

- **Status:** Accepted
- **Decision:** Object class and maximum size are selected before scanning and cannot be enlarged by optical metadata.
- **Reason:** Metadata-controlled allocation creates denial-of-service and memory-safety risk.
- **Consequence:** Pairing, messaging, wallet, and firmware-update transfers use separate bounded policies.

## ADR-LC-006: One active incoming transfer initially

- **Status:** Accepted for v0.1
- **Decision:** Permit one reconstruction context at a time.
- **Reason:** Multiplexing unauthenticated optical transfers adds state-collision and resource-management complexity without an essential initial use case.
- **Consequence:** New incoming transfers are rejected or require explicit cancellation of the current session.

## ADR-LC-007: No message or wallet decompression in decoder domain

- **Status:** Accepted
- **Decision:** Remove zstd from the message and wallet product profiles.
- **Reason:** Encrypted content is not meaningfully compressible after encryption, and hostile decompression adds expansion and parser risk.
- **Consequence:** Any optional pre-encryption compression must occur inside the Secure Core under a separate reviewed format.

## ADR-LC-008: No application interpretation at optical boundary

- **Status:** Accepted
- **Decision:** The decoder returns one bounded opaque byte object only.
- **Reason:** A barcode reconstruction is not proof of message, wallet, or firmware authenticity.
- **Consequence:** The Secure Core copies, authenticates, and parses the object before taking action.

## ADR-LC-009: Bound generic fountain decoder state

- **Status:** Accepted
- **Decision:** `fountain_decoder_sink` accepts `FountainDecoderLimits` and checks claimed object size and aggregate active object bytes before constructing a stream. The generic default retains the documented 25-bit protocol range, caps aggregate active object bytes to the same range, allows one active stream, caps unique blocks, accepts at most 16 packets per frame call, permits at most 64,000 frames and 1,024 consecutive no-progress frames per transfer, expires transfers after one hour, keeps eight completed-transfer identifiers and eight cancellation tombstones, and binds each active storage slot to its full metadata ID.
- **Reason:** Optical metadata previously selected decoder allocation directly, while seen block identifiers, packet processing, incomplete transfer lifetime, and terminal records could grow without a defined bound.
- **Alternatives considered:** Reducing the generic default to a product-size limit would break the documented 33 MiB transfer capability. Product integrations must instead set their smaller Secure-Core-selected limit explicitly.
- **Security consequences:** A single generic decoder now bounds active streams, aggregate claimed object bytes, unique blocks, packets processed per call, frame attempts, consecutive no-progress attempts, duration, and retained terminal state. Duration is checked on submission or an explicit expiry sweep. Exact third-party codec overhead is not yet charged against a byte budget, so deployment memory limits and allocator instrumentation remain required.
- **Compatibility consequences:** Concurrent transfers now require an explicit policy with a larger `maximum_active_streams` value.
- **Validation required:** Unit tests and stateful fuzzing must cover oversized metadata and frame calls, active-stream and aggregate-byte rejection, frame, no-progress, and duration cancellation, bounded completion and cancellation retention, duplicate blocks, reset, and valid transfers near each selected limit.

## ADR-LC-010: Repair vendored Wirehair alignment defects in source

- **Status:** Accepted
- **Context:** The Clang ASan/UBSan CI baseline found unaligned scalar accesses in vendored Wirehair and gf256 code on valid test paths.
- **Decision:** Backport the applicable upstream Wirehair fixes for unaligned scalar loads and stores, workspace alignment, and triangle-word access. Keep UBSan alignment checks enabled.
- **Reason:** Disabling or suppressing alignment diagnostics would leave undefined behavior in reachable codec code and could conceal platform-specific faults.
- **Alternatives considered:** Suppress `-fsanitize=alignment`, replace Wirehair immediately, or update the entire vendored dependency. Suppression was unsafe, replacement was out of scope, and a full dependency update would add unrelated change to the hardening branch.
- **Security consequences:** The known unaligned accesses are removed without weakening the sanitizer policy. The project still needs an identified upstream Wirehair revision and a repeatable dependency-update process.
- **Compatibility consequences:** The backport uses `memcpy` for unaligned scalar access and adds at most seven bytes of workspace padding; public APIs and encoded data remain unchanged.
- **Validation required:** Run all unit tests under Clang ASan/UBSan, both libFuzzer smoke campaigns, the normal Windows suite, and the SBOM consistency check.

## ADR-LC-011: Require exact-length checked raw-frame ingress

- **Status:** Accepted
- **Context:** The receive C API constructed OpenCV matrices from caller dimensions without receiving the source buffer length, bounding frame geometry, validating subsampled formats, or containing C++ exceptions.
- **Decision:** Add `cimbard_scan_extract_decode_checked`, require an exact tightly packed byte length, accept only RGB, RGBA, NV12, and I420, require even NV12/I420 dimensions, cap generic input at 4096 x 4096 pixels, and contain processing exceptions at the C ABI. Migrate the browser receiver to the checked API while retaining the old function only for compatibility.
- **Reason:** Hostile dimensions and short buffers must be rejected before signed conversion, arithmetic, OpenCV allocation, or pixel access.
- **Alternatives considered:** Break the existing ABI, trust JavaScript allocation length implicitly, or accept arbitrary OpenCV formats. A new checked function preserves compatibility while giving maintained callers a verifiable contract.
- **Security consequences:** The maintained browser path now has checked raw-frame arithmetic and exact-length validation. The compatibility wrapper remains unsuitable for the hardened profile because it cannot know the allocation length.
- **Compatibility consequences:** Existing callers continue to link. New callers receive stable negative result codes for malformed raw-frame calls.
- **Validation required:** Unit-test null pointers, zero and excessive dimensions, unsupported formats, odd subsampled dimensions, wrong input/output lengths, and run the receive suite under ASan/UBSan.

## ADR-LC-012: Make successful fountain completion terminal within a decoder session

- **Status:** Accepted
- **Context:** The detailed completed-transfer cache is intentionally bounded. Once a full metadata ID aged out, replaying the same transfer could allocate a new decoder and invoke the output callback again. The wire format has only a seven-bit encode ID and no transfer generation or nonce, so it cannot distinguish a replay from legitimate encode-ID reuse within one session.
- **Decision:** Track all 128 encode IDs in a fixed bitset after successful recovery. Reject any later frame using a completed encode ID until `reset()` starts a new explicit decoder session. Keep the bounded full-ID map only for recent filename and status details. Expose `cimbard_reset_decode()` at the C/WASM boundary and clear transfer, recovered-output, decompressor, reporting, and debug-frame state together.
- **Reason:** A fixed 16-byte terminal set gives an exact, non-evicting at-most-once rule without attacker-controlled growth. Requiring reset for encode-ID reuse makes the otherwise ambiguous protocol transition explicit.
- **Alternatives considered:** Retain only recent full IDs, keep an unbounded full-ID set, or add a transfer generation to the existing wire format. Recent IDs reopen after eviction, an unbounded set violates the resource policy, and a generation field requires a new transport protocol.
- **Security consequences:** A successfully emitted object cannot be emitted again within the same decoder session, even after its detail record is evicted. Callers must treat `reset()` as a security-relevant session boundary. Callback implementations still need transactional or non-throwing output semantics because the terminal bit is set only after the callback returns successfully.
- **Compatibility consequences:** A sender cannot reuse an encode ID for a different object without an explicit receiver reset. Generic multi-object workflows that require reuse must use distinct sessions or a future versioned transport with a generation field.
- **Validation required:** Unit-test replay after detail eviction, conflicting file sizes with the same encode ID, explicit sink and C-API reset, cleared recovered output, and successful re-use after reset. Assert the same property in the state-machine fuzzer.

## ADR-LC-013: Validate every fountain packet before state mutation

- **Status:** Accepted
- **Context:** `decode_frame()` accepts multiple concatenated fountain packets. It parsed and policy-checked only the first six-byte header, while `fountain_decoder_stream` silently discarded the first four bytes of every later packet and forwarded their block IDs and payloads to the first transfer's Wirehair decoder. A later packet could therefore carry conflicting transfer metadata or an out-of-policy block ID without rejection.
- **Decision:** Require each `decode_frame()` input to contain one or more complete configured chunks. Before time accounting, allocation, or decoder mutation, parse every packet header, require the same full transfer ID as the first packet, and enforce a policy-selected maximum block ID for every packet.
- **Reason:** Packet batching is an untrusted container boundary. Validating only its first element made the transfer identity and block policy incomplete and allowed mixed-transfer payloads to reach one reconstruction context.
- **Alternatives considered:** Validate only at the browser C API, teach the stream to maintain partial-header validation state, or accept mixed packets and rely on downstream object authentication. Browser-only checks leave generic callers exposed, partial buffering adds state and ambiguity, and downstream authentication does not excuse emitting transport-corrupted objects.
- **Security consequences:** Mixed-transfer batches, partial chunks, and excessive block IDs fail without allocating or mutating decoder state. The decoder no longer accepts arbitrary byte fragmentation through `decode_frame()`; callers must submit complete fountain chunks.
- **Compatibility consequences:** Callers that previously streamed partial chunks must buffer to the configured chunk boundary. Existing browser, CLI, and encoder-generated calls already submit aligned chunks.
- **Validation required:** Unit-test misalignment, conflicting metadata in later packets, excessive block IDs, no state mutation on rejection, and valid multi-packet completion. Exercise mixed-packet rejection in the state-machine fuzzer and rerun the full sanitizer suite.

## ADR-LC-014: Introduce a policy-required single-take fountain session

- **Status:** Accepted
- **Context:** The generic sink supports caller callbacks, synthesized filenames, completed-detail maps, decompression helpers, and filesystem writers. Those features are incompatible with the restricted optical transport boundary, and the core sink header transitively pulled zstd, path, file, and formatting dependencies into every consumer.
- **Decision:** Add `FountainTransferPolicy` with a required known object class, one-active-transfer limits, and zero retained completed-detail records, and wrap the core sink in `fountain_decoder_session`. The wrapper fails closed on decoder rejection, recovers only the exact validated length, owns at most one completed byte vector, and transfers it once through `take_completed_object()`. Move generic file and decompression callbacks into `fountain_decoder_file_sink.h`; compatibility applications must include it explicitly.
- **Reason:** The product-facing reconstruction boundary should make unsafe output semantics unavailable by construction rather than relying on each caller to avoid generic helpers.
- **Alternatives considered:** Continue documenting safe use of `fountain_decoder_sink`, delete generic applications immediately, or place the restricted behavior in the browser C API. Documentation alone leaves dangerous capabilities adjacent, immediate deletion would unnecessarily break the fork's reference tools, and the browser/WASM surface is explicitly excluded from the eventual product profile.
- **Security consequences:** Restricted consumers cannot select filenames, write files, decompress, register output callbacks, or observe partial objects through the session API. A completed object has one owner and one take. Negative submissions clear decoder state and require explicit reset.
- **Compatibility consequences:** Generic tools retain their existing behavior by including the new compatibility header. Sources that accidentally relied on the sink header for zstd or formatting declarations must add direct includes.
- **Validation required:** Test invalid policies, exact-byte recovery, second-take refusal, post-completion submission refusal, fail-closed oversized metadata, cancellation, reset, and generic compatibility builds. Add deterministic allocation/output-refusal testing before closing WP-07.

## New decision template

### ADR-LC-NNN: Title

- **Status:** Proposed / Accepted / Rejected / Superseded
- **Context:**
- **Decision:**
- **Reason:**
- **Alternatives considered:**
- **Security consequences:**
- **Compatibility consequences:**
- **Validation required:**
