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
