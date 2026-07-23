# Security Hardening Backlog

GitHub Issues were disabled when this backlog was initialized. Move these work packages into issues or a project board later if desired.

## WP-01: Reproduce the frozen baseline

**Priority:** P0  
**Dependencies:** None

- [x] Verify origin/audit-baseline-681e18e resolves to 681e18eb61a059f4a796bc6ef097d24b45c430eb.
- [x] Record submodule state.
- [ ] Build and install the full baseline target set in a clean environment.
- [ ] Run the complete CTest suite.
- [x] Run Python usage tests against the built baseline cimbar executable.
- [x] Hash the baseline cimbar executable.
- [x] Record baseline cimbar linked libraries.
- [ ] Capture baseline performance and peak memory.

**Exit:** A second environment can reproduce the baseline. Initial Windows evidence is recorded in docs/security/evidence/2026-07-21-windows-mingw-baseline.md; the complete build and CTest suite are currently blocked by baseline configuration and source-compatibility defects.

## WP-02: Generate the dependency inventory and SBOM

**Priority:** P0  
**Dependencies:** WP-01

- [x] Record direct local system dependency versions in docs/security/DEPENDENCY_INVENTORY.md.
- [x] Identify immutable upstream revisions for every vendored library.
- [x] Record declared licences and explicitly identify missing provenance.
- [x] Generate an initial SPDX SBOM with deterministic vendored-source fingerprints.
- [x] Establish vulnerability monitoring for OpenCV, Wirehair, zstd, libcorrect, stb, and other reachable dependencies.
- [x] Remove OpenCV decoding from production file-input paths and enforce reviewed, expiring exceptions for the seven remaining package-level findings.

**Exit:** Every direct dependency has an immutable source revision, version evidence, licence, update owner, and automated commit-level advisory monitoring. The pinned Windows vcpkg baseline fixes the external dependency graph, and CI couples the OSV lock to the source-digested SPDX catalog. The hardened transport config has no image-stack dependency. Production file inputs use bounded stb_image rather than OpenCV decoding; the exact OpenCV exception set expires on 2026-10-22 and fails if vulnerable calls return. A release-complete transitive SBOM and Linux package lock are separate release-engineering follow-up. WP-02 is complete.

## WP-03: Establish sanitizer and static-analysis baselines

**Priority:** P0  
**Dependencies:** WP-01

- [x] Enable GitHub Actions for the fork if required.
- [x] Run ASan and UBSan presets and record the repaired baseline.
- [x] Attempt MemorySanitizer with compatible dependencies.
- [x] Run ThreadSanitizer on concurrent receiver paths.
- [x] Run cppcheck against first-party sources and record the baseline.
- [x] Run clang-tidy against first-party production sources.
- [x] Run CodeQL.
- [x] Run Clang static analyzer and ownership-filter its SARIF output.
- [x] Record and triage cppcheck findings before fixing or suppressing warnings.

**Exit:** Baseline diagnostics are triaged and new blocking findings fail CI. Initial cppcheck evidence is recorded in docs/security/evidence/2026-07-21-cppcheck-first-party.md, the repaired ASan/UBSan and fuzzer baseline is recorded in docs/security/evidence/2026-07-21-ci-sanitizer-alignment.md, the focused receiver-concurrency result is recorded in docs/security/evidence/2026-07-21-thread-sanitizer-receiver.md, the focused MemorySanitizer result is recorded in docs/security/evidence/2026-07-21-memory-sanitizer-hardened-transport.md, the Clang source-analysis baseline is recorded in docs/security/evidence/2026-07-21-clang-static-analysis.md, and the production CodeQL baseline is recorded in docs/security/evidence/2026-07-22-codeql-cpp-production.md. WP-03 is complete.

## WP-04: Complete the function-level attack-surface map

**Priority:** P0  
**Dependencies:** WP-01

- [x] Map raw frame to geometry extraction.
- [x] Map symbol extraction to ECC.
- [x] Map corrected payload to metadata.
- [x] Map fountain state and Wirehair calls.
- [x] Map optional decompression and output.
- [x] Record ownership, bounds, allocation, state, and failure behaviour for each transition.

**Exit:** Every hostile-input transition has a function-level review status in docs/security/ATTACK_SURFACE.md. Open controls are tracked in the owning work packages rather than treated as completed remediation.

## WP-05: Fountain metadata and state-machine fuzzing

**Priority:** P0  
**Dependencies:** WP-03

- [x] Add initial `FountainMetadata` libFuzzer harness.
- [x] Seed and run the metadata corpus.
- [x] Design structured state-machine input operations.
- [x] Add start, block, duplicate, conflict, recover, cancel, timeout, and reset operations.
- [x] Measure peak state and memory for the bounded smoke campaign.
- [x] Add deterministic regression corpus entries for the fountain state defects fixed on this branch.

**Exit:** Conflicting and adversarial transfer sequences remain bounded and deterministic. Nineteen named, hashed seeds are reproducible from `fuzz/corpus/generate.py`; CI verifies them, copies them to a writable campaign corpus, and runs both sanitizer fuzz targets with the seeded inputs.

## WP-06: Introduce a bounded transfer policy

**Priority:** P0  
**Dependencies:** WP-04, WP-05

- [x] Add `FountainDecoderLimits` for object size, active streams, unique blocks, completed-transfer retention, and full-ID stream-slot binding.
- [x] Reject object size before decoder allocation.
- [x] Limit the default active transfer count to one.
- [x] Bound unique-block tracking and completed-transfer retention.
- [x] Define the fountain-stage `FountainTransferPolicy` and object classes selected by the Secure Core.
- [x] Validate every packet in a frame and limit block identifiers, packets per frame, frames, and no-progress frames.
- [x] Limit transfer duration and aggregate active object bytes across streams.
- [x] Instrument and bound third-party codec overhead against per-stream and aggregate decoder memory budgets before codec construction.
- [x] Define deterministic cancel and reset with bounded cancellation retention.
- [x] Ensure successful completion occurs at most once per encode ID until explicit session reset.

**Exit:** Optical metadata cannot enlarge Secure-Core-selected resource limits. Wirehair now exposes a conservative worst-case decoder heap estimate and current-allocation instrumentation; the restricted session requires explicit equal per-stream and aggregate codec budgets for its single active stream.

## WP-07: Replace generic output with bounded object API

**Priority:** P0  
**Dependencies:** WP-06

- [x] Add a policy-required fountain-stage decoder session API.
- [x] Return exact-length opaque bytes through a single-take ownership transfer.
- [x] Keep filename, MIME, path, callback, and application-route semantics out of the restricted API; isolate generic filesystem helpers in a compatibility header.
- [x] Ensure partial objects never cross the restricted API.
- [x] Test decoder allocation failure, completed-output allocation failure, and output recovery refusal with production-disabled fault injection.

**Exit:** The decoder has no generic file or application action interface. Allocation and recovery refusal fail closed, expose no object, and require explicit reset before successful reuse.

## WP-08: Remove decompression and filesystem paths from product profile

**Priority:** P0  
**Dependencies:** WP-04

- [x] Create product build option or target.
- [x] Exclude zstd from message and wallet profile.
- [x] Exclude filename parsing.
- [x] Exclude arbitrary filesystem output.
- [x] Exclude output-directory selection.
- [x] Verify removed symbols and dependencies are absent from the hardened archive and fully linked profile test.

**Exit:** `cimbar_hardened_transport` reconstructs message and wallet bytes without decompression or file creation. Its CMake link interface is Wirehair-only, and every build scans both the archive and linked profile test for forbidden dependencies and symbols.

## WP-09: Raw-frame and geometry fuzzing

**Priority:** P1  
**Dependencies:** WP-03, WP-04

- [x] Define supported raw frame formats and dimensions.
- [x] Build a constrained raw-frame fuzz input.
- [x] Seed with valid, cropped, rotated, noisy, overexposed, and damaged frames.
- [x] Exercise empty matrices, invalid ROI, singular transforms, NaN, infinity, and extreme coordinates.
- [x] Measure worst-case per-frame CPU.

**Progress:** `fuzz_raw_frame` now covers the length-aware receive ABI, the
four accepted tight raw formats, malformed dimensions and byte lengths, scanner
edge probing, non-finite line geometry, and degenerate deskew transforms. The
target caps generated fuzz frames at 96 x 96 pixels; the public checked API
still accepts RGB, RGBA, NV12, and I420 up to the 4096 x 4096 pixel ceiling.
`cimbar_raw_frame_cpu_budget` runs five full-size hostile RGB patterns through
the checked ABI with OpenCV restricted to one CPU thread and fails when the
largest of three samples per pattern exceeds 250 ms. Scanner confirmation
working sets are capped at 64 candidates per stage before the nested scan
operations. On the reference WSL release build, the highest observed call was
106.962 ms at the public 4096 x 4096 limit. The evidence records the exact
toolchain, command, and individual pattern results.

**Exit:** Malformed frames fail within the 250 ms per-frame resource budget on
the reference single-threaded OpenCV environment. WP-09 is complete.

## WP-10: End-to-end frame-sequence fuzzer

**Priority:** P1  
**Dependencies:** WP-05, WP-09

- [x] Encode structured frame sequences.
- [x] Support reordering, dropping, duplication, delay, and mutation.
- [x] Assert exact source bytes or no output.
- [x] Assert single completion and clean reset.

**Progress:** `fuzz_frame_sequence` now creates bounded source objects, encodes
canonical Wirehair fountain packets, and drives them through the restricted
`fountain_decoder_session` rather than the generic callback/file sink. The
schedule grammar covers in-order completion, selected reordering, dropped
packets, duplicate replay, batched packet frames, transfer-delay expiry,
explicit reset/replay, and rejected packet-envelope mutations. On every
positive completion the harness takes the single session-owned object and
asserts exact byte equality with the generated source; any second completion
before reset or any object visible on a non-completion path traps. Deterministic
seeds cover the reviewed valid, reordered, duplicate, dropped/delayed,
mutated-envelope, batched, and reset/replay paths.

**Exit:** No mixed or partial object is emitted under adversarial sequencing in
the WP-10 sanitizer smoke campaign. WP-10 is complete.

## WP-11: Decoder sandbox prototype

**Priority:** P1  
**Dependencies:** WP-07

- [x] Run decoder as non-root.
- [x] Remove network access and capabilities.
- [x] Apply read-only filesystem or no filesystem.
- [x] Add seccomp allowlist.
- [x] Add memory, CPU, process, and descriptor limits.
- [x] Add watchdog and per-transfer restart.
- [x] Attempt boundary escape from a deliberately compromised decoder.

**Progress:** The hardened transport preset can now build a Linux x86_64
`hardened_transport_sandbox_probe` when
`LIBCIMBAR_BUILD_LINUX_SANDBOX_PROTOTYPE=ON`. The probe forks one child per
transfer, drops root to uid/gid 65534 when needed, verifies a non-root identity
with no effective capabilities, sets `no_new_privs`, applies memory, CPU,
process, descriptor, file-size, and core limits, and installs a seccomp-BPF
allowlist before decoding. The allowlist admits only basic exit, signal, memory,
time, identity, futex, read, write, and close syscalls needed by the
corrected-packet hardened transport. Open/read, open/write, socket, fork, exec,
and setuid escape probes are all denied after an exact-byte decode. A parent
watchdog kills a stuck child and each transfer runs in a new process.

**Exit:** A deliberately compromised sandbox child cannot directly read or
write the parent-created secret file, create a socket, fork another process,
exec a program, regain uid 0, retain effective capabilities, or expose partial
decoder output. WP-11 is complete for the Linux prototype.

## WP-12: OIP-to-SCP IPC specification

**Priority:** P1  
**Dependencies:** WP-07

- [ ] Define fixed message types.
- [ ] Define version and transfer generation.
- [ ] Define maximum IPC message and object sizes.
- [ ] Prohibit pointers, filenames, nested objects, and arbitrary diagnostics.
- [ ] Fuzz the IPC parser independently.

**Exit:** IPC is smaller and easier to audit than the optical protocol.

## WP-13: Adversarial optical laboratory

**Priority:** P2  
**Dependencies:** WP-09, WP-10

- [ ] Build controllable screen/camera fixture.
- [ ] Automate brightness, angle, distance, frame rate, and frame mutation.
- [ ] Record memory, CPU, latency, errors, and post-reset operation.
- [ ] Test conflicting streams, no-progress streams, and interrupted transfers.

**Exit:** Resource and recovery requirements hold on representative hardware.

## WP-14: Independent audit preparation

**Priority:** P2  
**Dependencies:** WP-01 through WP-12

- [ ] Freeze release-candidate commit.
- [ ] Package threat model, data flow, SBOM, build instructions, fuzz targets, corpus, findings, and accepted risks.
- [ ] Commission C/C++, image/geometry, fountain, sandbox, IPC, and supply-chain reviews.
- [ ] Retest the final remediation commit.

**Exit:** Critical and high findings are closed and the final commit is independently retested.
