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
- **Security consequences:** A single generic decoder now bounds active streams, aggregate claimed object bytes, unique blocks, packets processed per call, frame attempts, consecutive no-progress attempts, duration, and retained terminal state. Duration is checked on submission or an explicit expiry sweep. Codec byte limits are optional for the compatibility sink but mandatory for the restricted product session under ADR-LC-017. Deployment process limits remain required for non-codec allocations and defence in depth.
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
- **Validation required:** Test invalid policies, exact-byte recovery, second-take refusal, post-completion submission refusal, fail-closed oversized metadata, cancellation, reset, generic compatibility builds, deterministic allocation failures, and output recovery refusal.

## ADR-LC-015: Contain allocation and recovery failures at the restricted session boundary

- **Status:** Accepted
- **Context:** Decoder map/codec construction and the exact-length completed-output vector can allocate after a frame has passed policy checks. The output-vector path caught all exceptions, while decoder-state allocation exceptions could escape the restricted boundary. Recovery refusal was fail-closed but had no deterministic regression path.
- **Decision:** Catch only `std::bad_alloc` and `std::length_error` around decoder submission and completed-output allocation, map them to stage-specific negative results, clear decoder and output state, and make the session terminal until reset. Compile narrow fault controls only into `fountain_test` to force decoder allocation, output allocation, and recovery refusal; normal builds contain neither the controls nor their branches.
- **Reason:** Resource exhaustion is an expected hostile-input failure mode and must not cross the product boundary as an uncaught C++ exception or leave reusable partial state. Deterministic injection is required to keep these otherwise unreliable paths under regression coverage.
- **Alternatives considered:** Override global allocation in the test binary, expose a runtime allocator/callback, or continue testing only naturally occurring failures. Global allocation overrides are brittle and can fail unrelated test infrastructure, a caller-controlled callback enlarges the production surface, and natural exhaustion is nondeterministic.
- **Security consequences:** Expected allocation failures and output refusal expose no bytes, release partial reconstruction state, reject subsequent frames, and permit successful exact-byte decoding only after explicit reset. Unexpected exception types are not hidden.
- **Compatibility consequences:** Normal consumers gain one new negative result, `decoder_allocation_failed`. Fault controls exist only when `CIMBAR_ENABLE_SESSION_TEST_FAULTS` is explicitly defined for a test target.
- **Validation required:** Under ASan/UBSan, force each failure, assert terminal failed state and no completed object, assert submission refusal before reset, then reset and recover the exact original bytes. Preprocess a normal consumer and verify that test fault identifiers are absent.

## ADR-LC-016: Ship corrected-packet decoding as a separately verified byte-only artifact

- **Status:** Accepted
- **Context:** Header dependency checks proved that the restricted session did not directly include zstd or filesystem helpers, but the fork had no compiled product artifact or transitive link boundary. Generic CLI and browser targets still intentionally link decompression and filename code.
- **Decision:** Add the `LIBCIMBAR_BUILD_HARDENED_TRANSPORT` option, the `cimbar_hardened_transport` static library, and a named `hardened-transport` release preset. Expose only explicit-policy corrected-packet submission and single-take opaque bytes through `HardenedFountainTransport`. Require the target's direct link interface to equal Wirehair, and scan both its archive and fully linked profile test after every link and through an always-executed verification target for zstd, decompression, filesystem, file-stream, filename-parser, compatibility-output, and test-fault terms.
- **Reason:** A dedicated artifact and link closure provide stronger evidence than source-header inspection and make the safe integration path obvious to the personal project consuming this fork.
- **Alternatives considered:** Reuse the generic CLI binary with runtime flags, rely only on documentation, or immediately delete compatibility targets. Runtime flags and documentation leave unsafe code linked into the product, while deleting reference tools is unnecessary once target separation and artifact verification are enforced.
- **Security consequences:** Message and wallet reconstruction artifacts contain no decompression or filesystem output dependency and expose no filename, path, directory, callback, or application-route operation. The verifier fails the build if forbidden terms re-enter the archive or linked profile test.
- **Compatibility consequences:** Existing generic tools are unchanged. Hardened consumers select the new target/preset. Root configuration still discovers OpenCV, but the hardened transport target does not link it.
- **Validation required:** Build the named release preset, recover exact message and wallet bytes, reject a second take, assert the CMake link interface is Wirehair-only, inspect dynamic dependencies, and pass post-build archive/executable scans plus the full sanitizer suite.

## ADR-LC-017: Reserve worst-case Wirehair decoder heap before admission

- **Status:** Accepted
- **Context:** Claimed object bytes and duplicate tracking were bounded, but Wirehair separately allocates input staging, peeling workspace, and a deferred Gaussian-elimination matrix. The matrix allocation occurs only when solving and its size depends on the number of deferred columns, so checking current allocation after decoder construction cannot provide safe admission control.
- **Decision:** Add vendored Wirehair APIs that calculate a conservative decoder-owned heap upper bound before construction and report current codec-owned allocation for verification. The estimate uses Wirehair's own constants and structure sizes, assumes all input columns may be deferred, and includes the codec object plus alignment overhead for all three buffers. Add per-stream and aggregate codec-memory limits to `FountainDecoderLimits`; reserve the estimate before `try_emplace`, release it with stream state, and require the single-stream restricted policy to set equal non-zero limits.
- **Reason:** Optical metadata must not cause a delayed, unbudgeted solver allocation. Calculating inside the vendored codec prevents the wrapper's accounting from drifting from Wirehair's private layout and constants.
- **Alternatives considered:** Measure resident set after allocation, impose only an OS process limit, duplicate an approximate formula in the wrapper, or add a caller-controlled allocator. Post-allocation measurement is too late, process limits do not provide per-transfer admission, duplicated formulas drift, and a new allocator callback enlarges the product surface.
- **Security consequences:** A transfer whose worst-case Wirehair heap exceeds either policy limit is rejected before any decoder state is created. Aggregate reservations prevent multiple allowed streams from exceeding the selected codec budget. The bound excludes wrapper containers, output bytes, libraries, stacks, and allocator metadata beyond Wirehair's explicit alignment request, so deployment-level memory containment remains necessary.
- **Compatibility consequences:** Generic sinks retain codec budgeting as an opt-in pair of zero-default limits. `fountain_decoder_session` policies now require explicit codec limits. Two additive functions extend the vendored Wirehair C API.
- **Validation required:** Reject one byte below the required bound without state mutation; admit the exact bound; enforce the aggregate limit across streams; clear reservations on reset; reject a restricted policy without a codec budget; and show that current allocation remains within the estimate after the solve matrix is allocated under ASan/UBSan.

## ADR-LC-018: Commit reproducible, semantically named fountain fuzz regressions

- **Status:** Accepted
- **Context:** The metadata and state fuzzers previously started from an empty corpus. Smoke campaigns found coverage but did not preserve reviewed boundary values or transfer sequences, so fixed state-machine defects could lose direct regression reachability between runs.
- **Decision:** Generate committed binary corpus entries from a dependency-free Python script and record each seed's target, length, SHA-256 digest, and security purpose in a deterministic manifest. Use explicit little-endian decoding for state-harness scalar operands. CI verifies the committed corpus, copies it to a disposable writable directory, and seeds both sanitizer campaigns from that copy.
- **Reason:** Named generated cases make binary inputs reviewable and reproducible while retaining libFuzzer's native corpus format. A writable copy prevents fuzz-discovered inputs from silently changing the reviewed source corpus.
- **Alternatives considered:** Commit only coverage-minimized hash-named files, rely on fresh random campaigns, or hand-maintain binary files without a generator. Coverage minimization discarded semantically distinct metadata boundaries, random campaigns do not guarantee regression execution, and opaque hand-maintained binaries are difficult to audit.
- **Security consequences:** Every fountain defect fixed on this branch has a permanent seed for its relevant rejection or terminal-state path, including stream-slot collision, mixed batches, block limits, no-progress cancellation, timeout/reset release, and completion replay after bounded detail eviction. Corpus integrity failures stop CI before fuzzing.
- **Compatibility consequences:** Fuzzer operand encoding is now architecture-independent little endian. Production formats and APIs are unchanged.
- **Validation required:** Regenerate and check all seeds, execute both targets from the seeded corpus under ASan/UBSan, run coverage merge/minimization, confirm every state seed contributes coverage, enforce campaign timeout/RSS limits, and verify the committed corpus is unchanged after the run.

## ADR-LC-019: Serialize the concurrent fountain wrapper and isolate compatibility globals

- **Status:** Accepted
- **Context:** The concurrent fountain wrapper read decoder containers without its write mutex, manually unlocked after decode/callback work, and used `try_lock` draining that could leave a final enqueued packet unprocessed. `FountainInit::init` had header-level internal linkage, creating an independent once-guard in each translation unit around Wirehair's process-global initializer. Separately, the `cimbard_*` compatibility receiver combines process-global state with thread-local configuration.
- **Decision:** Use blocking RAII drain ownership, synchronize every decoder-state accessor, report queue/null-input failures, and enforce one write-then-read lock order. Give `FountainInit::init` program-wide inline linkage. Keep the compatibility C receiver explicitly single-worker and outside the hardened byte-only artifact rather than claiming that a mutex repairs its mixed ownership model.
- **Reason:** Packet loss, data races, stuck locks, and duplicate global initialization are unacceptable in an untrusted receiver. Compatibility globals require an ownership redesign, not superficial locking.
- **Alternatives considered:** Retain opportunistic `try_lock` draining, add atomics only to counters, or place one recursive mutex around the compatibility C API. Opportunistic draining has a lost-wakeup window, counter atomics do not protect decoder containers, and a C-API mutex cannot make thread-local mode configuration coherent or protect returned raw pointers after lock release.
- **Security consequences:** The intended concurrent packet sink serializes all Wirehair state and cannot strand a successful enqueue. Wirehair initialization occurs once across first-party translation units. Compatibility receiver users must provide one-worker isolation; the product artifact exposes neither those globals nor their raw-buffer API.
- **Compatibility consequences:** Concurrent sink producers may block until the current drain/callback completes instead of returning after an unsuccessful `try_lock`. Queue exhaustion is no longer silently accepted. Existing serialized C/WASM compatibility users are unchanged.
- **Validation required:** Run a two-translation-unit initialization race and simultaneous four-writer/live-observer sink regression under TSan, run the complete fountain test binary under TSan and ASan/UBSan, and reverify the release hardened artifact.

## ADR-LC-020: Validate cell regions before pixel arithmetic

- **Status:** Accepted
- **Context:** `Cell` used signed products to form raw pixel offsets, did not validate caller-selected subregions, reversed row and column bounds for non-contiguous matrices, and accumulated pixel totals and counts in 16-bit integers. An existing asymmetric test encoded the resulting out-of-ROI mean as expected behavior.
- **Decision:** Use one row-wise implementation for continuous and strided matrices, validate depth/channels and full ROI containment before accessing a row, calculate offsets in `std::size_t`, and accumulate in 64-bit totals with `std::size_t` counts. Invalid regions return a zero sample.
- **Reason:** Cell sampling is downstream of attacker-controlled image geometry. Logical ROI overreads and silent arithmetic wrap can corrupt decoded symbols even when an allocation-level bounds violation does not occur.
- **Alternatives considered:** Add casts only at the reported multiplication, retain separate continuous/non-contiguous fast paths, or rely on fixed current grid dimensions. Casts would preserve incorrect traversal, duplicate paths had already diverged, and fixed configuration is not a substitute for validating a reusable hostile-input primitive.
- **Security consequences:** Pixel reads remain within the validated matrix region, large samples cannot overflow 16-bit totals, and malformed regions fail without exposing adjacent logical pixels to symbol classification.
- **Compatibility consequences:** Rectangular non-contiguous cells now return their actual OpenCV-equivalent mean. The former bug-dependent `191` test value changes to the correct region mean of `233`.
- **Validation required:** Compare rectangular strided RGB and grayscale regions with `cv::mean`, exercise a sample large enough to overflow the previous accumulators, reject negative and overrun regions, and run translator/encoder tests under ASan/UBSan and the focused Clang gates.

## ADR-LC-021: Use fixed-index duplicate tracking and a focused MemorySanitizer closure

- **Status:** Accepted
- **Context:** `FountainDecoder` retained attacker-selected unique block identifiers in a `std::set`, creating one heap node per identifier up to the configured limit and outside the Wirehair codec-memory accounting. A full MemorySanitizer build is not reliable with the distribution's uninstrumented C++ and OpenCV dependencies, while the byte-only product profile has a small dependency closure.
- **Decision:** Represent seen 16-bit block identifiers with a value-initialized, policy-sized bitmap capped at 65,536 bits. Pass the sink's maximum block identifier into each decoder. Avoid constructing a completion filename when completed-transfer history is disabled. Add a MemorySanitizer preset and CI job that build and execute only the hardened byte transport profile with origin tracking enabled.
- **Reason:** Duplicate tracking must have a deterministic memory ceiling independent of packet order, and sanitizer evidence must correspond to a closure that can execute without known uninstrumented image-stack dependencies.
- **Alternatives considered:** Keep the ordered set under the unique-block count, charge allocator-dependent tree nodes against the codec budget, or claim a full-project MemorySanitizer baseline. The set remained unnecessarily allocation-heavy, allocator accounting would be non-portable, and the full-project claim would be misleading without an instrumented dependency sysroot.
- **Security consequences:** Duplicate state is direct-indexed and uses at most 8 KiB per decoder at the protocol maximum; product policies allocate less. Out-of-policy block identifiers reject before indexing or Wirehair. The focused product test reports uninitialized reads in first-party and instrumented vendored code, but this is not evidence that uninstrumented external libraries are clean.
- **Compatibility consequences:** Public decoding behavior and progress counts remain unchanged. A duplicate submitted after the unique-block ceiling is reached remains a rejected input. Compatibility sinks that retain completed filenames continue to construct and store them.
- **Validation required:** Exercise the exact maximum block identifier, reject the next identifier without changing progress, run the fountain and hardened-profile tests under ASan/UBSan, run the byte-only profile under MemorySanitizer with origin tracking, and reverify the release artifact exclusions.

## ADR-LC-022: Gate production CodeQL and validate Wirehair dimensions before narrowing

- **Status:** Accepted
- **Context:** The production CodeQL `security-extended` baseline reported one first-party size multiplication that occurred before widening and 41 vendored findings. Line-by-line review of the nine Wirehair reports found bounded loop-width and SIMD pointer-scaling patterns, but nearby review exposed a distinct defect: `ChooseMatrix` narrowed an untrusted wide block count to 16 bits before checking Wirehair's 64,000-block maximum. A count such as 65,538 could wrap to two and initialize the wrong matrix dimensions.
- **Decision:** Build all production C and C++ surfaces through a named manual-build CodeQL preset, retain raw SARIF, and fail CI on any first-party production result while recording vendored results separately. Calculate Wirehair's ceiling division without additive overflow, validate it in 64 bits before narrowing, reject block sizes beyond the signed length accepted by gf256, and retain the upstream 32-bit offset guard. Require exact full writes when saving a color-correction matrix.
- **Reason:** A static-analysis baseline is useful only when its compiled source closure is reproducible and new owned findings are blocking. Manual review must extend beyond the exact reported line because an analyzer warning can expose a more serious adjacent invariant failure.
- **Alternatives considered:** Analyze tests and every optional target, ignore all vendored results, fail on the entire vendored baseline, or suppress the Wirehair rules. Tests would dilute the production closure, ignoring dependencies would lose useful update evidence, immediately failing on 41 inherited reports would make the gate unusable, and suppression would conceal future dependency changes.
- **Security consequences:** First-party production CodeQL findings now fail CI. Color-correction writes reject non-contiguous, oversized, or partial output. Direct Wirehair decoder creation cannot accept a wrapped block count and construct undersized state for a much larger claimed message. Vendored fmt, stb, zstd, and Wirehair reports remain visible in SARIF and require re-triage when those dependencies are updated.
- **Compatibility consequences:** Valid Wirehair inputs within the documented two-to-64,000-block range are unchanged. Previously accepted out-of-range inputs that wrapped after conversion now fail. The `codeql` preset builds production applications and the hardened transport but not unit-test targets.
- **Validation required:** Rebuild a clean CodeQL database from the named preset, confirm zero first-party production findings, run the ownership gate against its SARIF, reject the 65,538 one-byte-block regression under ASan/UBSan, run the complete sanitizer suite, and reverify the release artifact and SBOM after the vendored-source change.

## ADR-LC-023: Lock copied dependency provenance and fail on commit-range advisories

- **Status:** Accepted
- **Context:** Generic dependency discovery found no packages in `src/third_party_lib`, so an automatic OSV scan could report a false-clean result for copied C and C++ source. Source history and exact upstream comparisons established immutable revisions for all vendored libraries. The first explicit commit scan found two stb_image advisories in the production embedded-image path. A corrected all-package count also exposed seven OpenCV 4.11.0 findings, including five PNG-decoder findings reachable through generic file-input applications.
- **Decision:** Record every copied dependency and direct external build dependency as an immutable Git coordinate in both the SPDX catalog and `osv-scanner-custom.json`. Upgrade stb_image from v2.27 to the exact upstream v2.30 header. Pin the reference Windows vcpkg graph with a builtin baseline, disable OpenCV's broad defaults, and confine the remaining image dependencies to a `compatibility-apps` manifest feature. Configure the hardened transport without discovering or installing the image stack. Run OSV's pinned reusable workflows on changes, weekly, and on demand, failing on vulnerabilities and uploading SARIF. Before scanning, regenerate-check the vendored tree digests and require the OSV, SPDX, and vcpkg catalogs to agree.
- **Reason:** Copied source has no package-manager metadata for a scanner to discover. Explicit commits enable OSV's C/C++ commit-range matching, while source-tree digests expose local changes that a stale coordinate would otherwise conceal.
- **Alternatives considered:** Trust recursive source discovery, monitor only release tags, convert every dependency to a Git submodule immediately, or accept a documentation-only inventory. Recursive discovery missed the copied tree, tags are mutable names rather than the checked source identity, a full dependency-layout migration is outside the hardening scope, and documentation alone cannot stop drift.
- **Security consequences:** The stb upgrade clears OSV-2020-1372 and OSV-2021-1239. The unfiltered repository-wide scan truthfully records seven OpenCV findings. Production file decoding has moved off the five affected PNG paths; the remaining exception set is exact, justified, and expires on 2026-10-22. CI fails if a vulnerable production call returns, an exception expires, source digests become stale, or dependency coordinates diverge. The hardened transport configuration has no OpenCV, image-codec, GLFW, or OpenGL dependency. A filtered clean result is not proof of vulnerability absence because database and commit-range coverage are incomplete.
- **Compatibility consequences:** stb_image changes from upstream v2.27 to v2.30 without a local API wrapper change. Windows compatibility builds resolve from the recorded vcpkg baseline with a narrower OpenCV codec feature set. Generic TIFF and WebP decoding are no longer provided by that reference manifest. Linux system-package resolution remains distribution-selected and is not claimed to be locked by that manifest.
- **Validation required:** Verify the new stb header byte-for-byte against its locked upstream Git blob, run the explicit 15-package OSV scan and retain the seven OpenCV findings, parse and dry-run both vcpkg feature modes, regenerate and check the SPDX SBOM, run the cross-catalog dependency check, configure the hardened transport from a clean tree with no image dependency available, and rebuild the full ASan/UBSan suite.

## ADR-LC-024: Decode compatibility file inputs through the bounded stb path

- **Status:** Accepted
- **Context:** OSV commit matching reported five OpenCV PNG decoder memory-safety findings. Generic CLI, extractor, deskewer, and decoder helpers passed attacker-selected filenames to `cv::imread`, making those package findings reachable even though the hardened corrected-packet artifact excludes OpenCV.
- **Decision:** Route every production filename-to-image operation through `cimbar::load_img_file` and the updated stb_image v2.30 decoder. Cap encoded input at 64 MiB, dimensions at 4096 per side, decoded images at 16,777,216 pixels, and output at three owned RGB channels. Return an empty image on missing, empty, oversized, malformed, short-read, or expected allocation failure. Keep OpenCV image decoding only in trusted-fixture tests. Enforce this reachability boundary in the dependency-lock check and attach expiring OSV exceptions rather than deleting the package-level evidence.
- **Reason:** The same reviewed decoder already handles embedded image assets and has explicit pre-allocation bounds. Removing vulnerable entry points is lower risk than carrying an unshipped OpenCV overlay or waiting for a package revision that still reports unrelated, disabled paths.
- **Alternatives considered:** Suppress the OpenCV package wholesale, leave the compatibility tools blocked, add a local OpenCV overlay, or remove file-input tools. A package-wide suppression would hide new advisories, a permanent red gate loses signal, an overlay creates a large patch-maintenance burden, and the tools remain useful when their decode boundary is replaced.
- **Security consequences:** Attacker-selected image files no longer reach `cv::imread` or `cv::imdecode`. Input and decoded dimensions are bounded before owned matrix construction. Seven known OpenCV findings remain visible as reviewed exceptions through 2026-10-22; CI rejects new first-party uses of the affected decode APIs or `cv::split`. OpenCV encoders and trusted-fixture decode calls remain outside this hostile-input claim.
- **Compatibility consequences:** Supported compatibility input formats are those decoded by stb_image v2.30. One JPEG extraction fixture changes by one average-hash bit relative to OpenCV decoding; the exact stb-derived regression value is recorded. Missing or invalid files now fail before output creation instead of reaching `cv::cvtColor` with an empty matrix.
- **Validation required:** Search first-party production source for `cv::imread`, `cv::imdecode`, and `cv::split`; exercise valid and missing files through the bounded loader; run the filtered and unfiltered 15-package OSV scans; validate exception expiry and vcpkg feature restrictions; and pass all 12 ASan/UBSan tests.

## ADR-LC-025: Constrain raw-frame geometry fuzzing and reject degenerate transforms

- **Status:** Accepted
- **Context:** WP-09 required fuzzing the hostile raw-frame path beyond the existing ABI unit tests. The remaining geometry risks were scanner probes near matrix edges, non-finite line intersections, singular perspective transforms, and invalid deskew output dimensions.
- **Decision:** Add `fuzz_raw_frame` as an OpenCV-backed libFuzzer target for the checked receive ABI and geometry stack. The harness accepts RGB, RGBA, NV12, and I420 envelopes but caps generated fuzz frames at 96 x 96 pixels, mutates byte-length and dimension contracts, and separately feeds scanner, midpoint, and deskew paths with degenerate, non-finite, and extreme coordinate seeds. Scanner pixel probes outside the current matrix now return false. Deskew rejects empty images, invalid output geometry, non-finite transforms, and singular transforms before warp.
- **Reason:** The C ABI must remain generic for compatibility, but continuous fuzzing needs a tighter input generator to keep campaigns cheap and deterministic. Failing closed before OpenCV warp gives the C and C++ callers the same safe result for malformed geometry.
- **Alternatives considered:** Fuzz only the C ABI, fuzz full-size 4096 x 4096 frames, or rely on OpenCV exceptions for geometry failure. ABI-only fuzzing misses direct scanner and deskew corner cases, full-size frames make fuzzing too slow for pull-request smoke, and exception-only handling leaves generic C++ callers without a stable invalid-geometry result.
- **Security consequences:** Malformed raw frame envelopes, odd YUV dimensions, null inputs, edge probes, NaN, infinity, repeated corners, collinear corners, and invalid deskew output sizes now have sanitizer-backed coverage. The remaining WP-09 gap is a reproducible worst-case per-frame CPU budget.
- **Compatibility consequences:** Valid deskew inputs with finite non-singular transforms are unchanged. Previously warped degenerate transforms now return an empty matrix, matching existing empty-input failure behavior in compatibility wrappers.
- **Validation required:** Regenerate/check the corpus manifest, build the standalone fuzz workspace with Clang/libFuzzer, run the raw-frame corpus smoke under ASan/UBSan, and rerun extractor plus checked receive tests under ASan/UBSan.

## ADR-LC-026: Bound scanner candidate confirmation and gate full-size frame CPU

- **Status:** Accepted
- **Context:** The checked raw-frame ABI bounds pixel count but still passes accepted 4096 x 4096 frames into scanner preprocessing and candidate confirmation. The scanner's nested confirmation paths previously allowed candidate vectors to grow with hostile image structure, and no executable measured the actual end-to-end time spent in the checked ABI.
- **Decision:** Cap both primary and bottom-right scanner confirmation vectors at 64 candidates before invoking additional nested scans. Add `cimbar_raw_frame_cpu_budget`, a benchmark executable run from a release configuration that preallocates five full-size RGB hostile patterns, disables OpenCL, selects one OpenCV thread, times only `cimbard_scan_extract_decode_checked`, and fails when any of three measured samples per pattern exceeds 250 ms.
- **Reason:** Four anchors are sufficient for decode, so retaining an unbounded candidate set has no product value. A full-size, preallocated benchmark measures the hostile processing path instead of fuzzer, file-I/O, harness allocation, or browser overhead, while preserving the checked ABI's own allocation cost and exposing a repeatable release check through its exit status.
- **Alternatives considered:** Rely on the raw-frame fuzzer timeout, measure only the small fuzz generator maximum, or add a hard wall-clock abort inside the compatibility API. Fuzzer timeouts are campaign-level and sanitizer-inflated, small frames do not cover the public limit, and an in-process wall-clock abort cannot safely interrupt OpenCV work. Candidate caps plus a separately measurable deployment budget retain compatibility and give the product profile an enforceable acceptance check.
- **Security consequences:** Candidate confirmation and comparison work is bounded by a fixed 64-entry working set in addition to the image-size cap. On the reference WSL Clang 18/OpenCV 4.6 release build, the highest result over 15 full-size hostile scans was 106.962 ms; the 250 ms gate leaves headroom but must be rerun when the OpenCV version, compiler, hardware class, or frame policy changes.
- **Compatibility consequences:** Valid scanner inputs normally reach the four required anchors long before the cap. Inputs that would previously accumulate more than 64 confirmation candidates now fail extraction rather than spending additional CPU on ambiguous candidates.
- **Validation required:** Build the target in an optimized OpenCV-enabled configuration, run its full-size default suite, and rerun the extractor, checked-receive, and raw-frame fuzz checks under ASan/UBSan.

## ADR-LC-027: Fuzz encoded frame sequencing at the restricted session boundary

- **Status:** Accepted
- **Context:** `fuzz_fountain_state` covered synthetic sink operations and WP-09 covered hostile raw-frame geometry, but WP-10 still needed a higher-level harness that begins with a valid encoded object and then perturbs the delivery schedule seen by the product-facing restricted fountain session. Without this target, reorder/drop/duplicate/delay/reset interactions could regress while the lower-level state harness still passed.
- **Decision:** Add `fuzz_frame_sequence`, which generates a bounded source byte vector, encodes canonical Wirehair fountain packets, and submits whole single-packet or batched frames through `fountain_decoder_session`. The schedule grammar supports in-order completion, selected reordering, dropping, duplicate replay, transfer-delay expiry, explicit reset/replay, and rejected packet-envelope mutations. A positive completion must expose exactly one object, and that object must byte-match the generated source; non-completion paths must expose no object.
- **Reason:** The restricted session is the hardened boundary that owns exact-length output and reset/fail-closed semantics. Starting from a real encoder avoids false confidence from hand-written packet payloads, while keeping object size, packet count, block identifiers, active codec memory, frame count, and operation count bounded for sanitizer smoke campaigns.
- **Alternatives considered:** Extend the existing state fuzzer only, test through the generic filesystem callback sink, or make the harness depend on raw camera/OpenCV extraction. State-only coverage misses encoder/scheduler composition, the callback sink carries filename/filesystem behavior outside the product profile, and raw camera extraction would make sequence campaigns slow and dependent on image-stack availability.
- **Security consequences:** Adversarial sequencing now has sanitizer-backed coverage for exact output, no partial output, at-most-once completion before reset, timeout fail-closed behavior, reset reuse, and hostile packet envelopes rejected before output. Packet-content authentication remains a corrected-payload/ECC boundary concern; this target does not claim that unauthenticated arbitrary Wirehair payload corruption is detectable at the fountain layer.
- **Compatibility consequences:** No decoder behavior changes. The new fuzzer links only the fountain/Wirehair closure and leaves compatibility file/decompression sinks outside the target.
- **Validation required:** Regenerate/check the deterministic corpus, build the standalone fuzz workspace with Clang/libFuzzer, run the frame-sequence corpus smoke under ASan/UBSan, and keep the new target in sanitizer CI.

## ADR-LC-028: Prototype the decoder sandbox as a per-transfer Linux child

- **Status:** Accepted
- **Context:** The hardened transport artifact removed filename, decompression, and filesystem semantics, but a decoder memory-safety compromise could still abuse the hosting process if the decoder ran in the Secure Core address space. WP-11 required a concrete sandbox prototype before designing the final IPC boundary.
- **Decision:** Add an opt-in Linux x86_64 `hardened_transport_sandbox_probe` executable under the hardened transport profile. The parent forks a fresh child for each transfer and enforces a watchdog. The child drops root to uid/gid 65534 when needed, verifies non-root execution and zero effective capabilities, sets `no_new_privs`, disables core dumps, applies address-space, CPU, process-count, file-size, core, and descriptor limits, attempts network namespace isolation where the kernel permits it, then installs a seccomp-BPF allowlist before decoding through `HardenedFountainTransport`.
- **Reason:** A child process gives the Secure Core a hard restart boundary and a simple place to apply kernel limits before any untrusted transfer is reconstructed. Keeping the prototype outside `cimbar_hardened_transport` preserves the byte-only artifact and its symbol/string verifier while still testing the same corrected-packet decode surface.
- **Alternatives considered:** Link sandbox code into the transport library, rely only on in-process limits, use a container runtime, or wait for the final IPC design. Linking sandbox code into the library would pollute the artifact boundary, in-process limits do not protect Secure Core memory, a container runtime is too deployment-specific for the first prototype, and delaying sandbox evidence would leave WP-12 without an enforced trust-boundary baseline.
- **Security consequences:** The sandbox smoke test performs an exact decode, then deliberately attempts to read and write a parent-created secret, create a network socket, fork, exec `/bin/true`, and regain uid 0; all are denied. The parent kills a child that misses the watchdog deadline, and the second transfer runs in a distinct child process. Network namespace setup is best-effort because some unprivileged environments return `EPERM`; socket creation remains denied by seccomp either way.
- **Compatibility consequences:** No public decoder API changes. The sandbox prototype is Linux x86_64 only and opt-in through `LIBCIMBAR_BUILD_LINUX_SANDBOX_PROTOTYPE`; non-Linux builds skip it. The hardened transport archive and linked profile test remain subject to the existing forbidden-symbol verifier, while the sandbox probe is intentionally not scanned by that artifact verifier because it contains attack-probe syscall names.
- **Validation required:** Configure and build the hardened transport preset with the sandbox option enabled, run `ctest --preset hardened-transport`, verify all three hardened transport tests pass, and keep the sandbox probe in CI on Ubuntu.

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
