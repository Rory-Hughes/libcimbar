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
- [ ] Identify immutable upstream revisions for every vendored library.
- [x] Record declared licences and explicitly identify missing provenance.
- [x] Generate an initial SPDX SBOM with deterministic vendored-source fingerprints.
- [ ] Establish vulnerability monitoring for OpenCV, Wirehair, zstd, libcorrect, stb, and other reachable dependencies.

**Exit:** Every shipped dependency has source, version, licence, and update owner. The initial inventory is not yet release-complete because external dependencies are not locked and several vendored revisions remain unknown.

## WP-03: Establish sanitizer and static-analysis baselines

**Priority:** P0  
**Dependencies:** WP-01

- [x] Enable GitHub Actions for the fork if required.
- [x] Run ASan and UBSan presets and record the repaired baseline.
- [ ] Attempt MemorySanitizer with compatible dependencies.
- [ ] Run ThreadSanitizer on concurrent receiver paths.
- [x] Run cppcheck against first-party sources and record the baseline.
- [ ] Run clang-tidy, CodeQL, and Clang static analyzer.
- [x] Record and triage cppcheck findings before fixing or suppressing warnings.

**Exit:** Baseline diagnostics are triaged and new blocking findings fail CI. Initial cppcheck evidence is recorded in docs/security/evidence/2026-07-21-cppcheck-first-party.md, and the repaired ASan/UBSan and fuzzer baseline is recorded in docs/security/evidence/2026-07-21-ci-sanitizer-alignment.md. MemorySanitizer, ThreadSanitizer, clang-tidy, CodeQL, and Clang static analyzer work remain outstanding.

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
- [ ] Seed and run the metadata corpus.
- [ ] Design structured state-machine input operations.
- [ ] Add start, block, duplicate, conflict, recover, cancel, timeout, and reset operations.
- [ ] Measure peak state and memory.
- [ ] Add regression corpus entries for every defect.

**Exit:** Conflicting and adversarial transfer sequences remain bounded and deterministic.

## WP-06: Introduce a bounded transfer policy

**Priority:** P0  
**Dependencies:** WP-04, WP-05

- [x] Add `FountainDecoderLimits` for object size, active streams, unique blocks, completed-transfer retention, and full-ID stream-slot binding.
- [x] Reject object size before decoder allocation.
- [x] Limit the default active transfer count to one.
- [x] Bound unique-block tracking and completed-transfer retention.
- [ ] Define `TransferPolicy` and object classes selected by the Secure Core.
- [x] Limit block identifiers, packets per frame, frames, and no-progress frames.
- [x] Limit transfer duration and aggregate active object bytes across streams.
- [ ] Instrument or otherwise bound third-party codec overhead against a total decoder memory budget.
- [x] Define deterministic cancel and reset with bounded cancellation retention.
- [ ] Ensure completion occurs at most once.

**Exit:** Optical metadata cannot enlarge Secure-Core-selected resource limits.

## WP-07: Replace generic output with bounded object API

**Priority:** P0  
**Dependencies:** WP-06

- [ ] Design `DecoderSession` API.
- [ ] Return exact-length opaque bytes only.
- [ ] Remove filename, MIME, path, and application-route semantics.
- [ ] Ensure partial objects never cross the API.
- [ ] Test allocation failure and output refusal.

**Exit:** The decoder has no generic file or application action interface.

## WP-08: Remove decompression and filesystem paths from product profile

**Priority:** P0  
**Dependencies:** WP-04

- [ ] Create product build option or target.
- [ ] Exclude zstd from message and wallet profile.
- [ ] Exclude filename parsing.
- [ ] Exclude arbitrary filesystem output.
- [ ] Exclude output-directory selection.
- [ ] Verify removed symbols and dependencies are absent from the production binary.

**Exit:** Product profile reconstructs bytes without decompression or file creation.

## WP-09: Raw-frame and geometry fuzzing

**Priority:** P1  
**Dependencies:** WP-03, WP-04

- [ ] Define supported raw frame formats and dimensions.
- [ ] Build a constrained raw-frame fuzz input.
- [ ] Seed with valid, cropped, rotated, noisy, overexposed, and damaged frames.
- [ ] Exercise empty matrices, invalid ROI, singular transforms, NaN, infinity, and extreme coordinates.
- [ ] Measure worst-case per-frame CPU.

**Exit:** Malformed frames fail within the per-frame resource budget.

## WP-10: End-to-end frame-sequence fuzzer

**Priority:** P1  
**Dependencies:** WP-05, WP-09

- [ ] Encode structured frame sequences.
- [ ] Support reordering, dropping, duplication, delay, and mutation.
- [ ] Assert exact source bytes or no output.
- [ ] Assert single completion and clean reset.

**Exit:** No mixed or partial object is emitted under adversarial sequencing.

## WP-11: Decoder sandbox prototype

**Priority:** P1  
**Dependencies:** WP-07

- [ ] Run decoder as non-root.
- [ ] Remove network access and capabilities.
- [ ] Apply read-only filesystem or no filesystem.
- [ ] Add seccomp allowlist.
- [ ] Add memory, CPU, process, and descriptor limits.
- [ ] Add watchdog and per-transfer restart.
- [ ] Attempt boundary escape from a deliberately compromised decoder.

**Exit:** Decoder compromise cannot directly access secrets, network, arbitrary files, or secure-core memory.

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
