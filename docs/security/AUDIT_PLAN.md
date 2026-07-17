# Audit and Hardening Plan

## Phase 0: Freeze and reproduce

### Deliverables

- Frozen audit branch and commit.
- Recursive source archive.
- Build container or documented environment.
- Compiler, linker, CMake, OpenCV, and system dependency versions.
- Test output and binary hashes.
- Initial SBOM.

### Exit criteria

- A second clean environment can reproduce the baseline build and tests.
- Every result identifies an exact commit and toolchain.

## Phase 1: Map the attack surface

### Deliverables

- Function-level decoder data-flow map.
- Trust and ownership annotations.
- Allocation and persistent-state inventory.
- Dependency-reachability map.
- Initial abuse cases.

### Exit criteria

- Every stage from raw frame to output sink has an owner and review status.
- Every attacker-controlled length and identifier is documented.

## Phase 2: Establish automated analysis

### Deliverables

- Strict compiler-warning configuration.
- ASan and UBSan build.
- MemorySanitizer build where dependencies permit.
- ThreadSanitizer build for concurrent paths.
- clang-tidy, CodeQL, and cppcheck baselines.
- CI smoke fuzzing.

### Exit criteria

- Baseline findings are recorded rather than silently suppressed.
- New blocking diagnostics fail CI.

## Phase 3: Build fuzzing coverage

### Priority targets

1. Fountain metadata parsing.
2. Fountain state machine and Wirehair wrapper.
3. Corrected frame payload.
4. Geometry and raw-frame extraction.
5. End-to-end frame sequences.
6. zstd and filename handling for upstream assessment only.

### Exit criteria

- Every critical parser has a harness and seed corpus.
- Every discovered crash has a regression input.
- Memory and timeout limits are enforced by the harness environment.

## Phase 4: Manual source review

Review themes:

- Integer and allocation safety.
- Image geometry and OpenCV matrix assumptions.
- ECC failure semantics.
- Fountain context lifecycle.
- Duplicate and completed-state tracking.
- Concurrency and shutdown.
- Decompression limits.
- Filesystem behaviour.
- Error and exception containment.

### Exit criteria

- Every high-risk component has a completed checklist.
- Findings are severity-rated and assigned.

## Phase 5: Implement the restricted product profile

### Changes

- Introduce explicit transfer policy.
- Reject oversized metadata before allocation.
- Limit active transfers to one.
- Bound block and progress tracking.
- Remove completed-object persistence.
- Remove zstd from message and wallet builds.
- Remove filename and generic filesystem support.
- Replace image-file input with fixed raw-frame input.
- Add a bounded object-return API.

### Exit criteria

- Product profile contains only required code paths.
- Generic file-transfer features are unreachable or absent from the production binary.

## Phase 6: Sandbox and IPC

### Deliverables

- Dedicated decoder process or processor-domain configuration.
- Seccomp or equivalent system-call policy.
- Resource limits and watchdog.
- Fixed typed IPC schema.
- Restart and state-cleanup tests.

### Exit criteria

- Deliberate decoder compromise cannot directly access secrets, network, arbitrary files, or secure-core memory.

## Phase 7: Adversarial optical testing

Test:

- Conflicting transfer identities and sizes.
- Endless duplicates and unique blocks.
- Almost-complete transfers.
- No-progress sequences.
- Malformed geometry and anchors.
- Adversarial colour and frame timing.
- Camera interruption.
- Process restart and IPC loss.

### Exit criteria

- Peak resource use remains within policy.
- No partial data crosses IPC.
- A valid subsequent transfer succeeds after every tested failure.

## Phase 8: Independent review

Commission review of:

- C/C++ memory safety.
- Image and geometry code.
- Fountain and Wirehair integration.
- Dependency use.
- Sandbox and IPC.
- Build and release chain.

### Exit criteria

- Critical and high findings are closed.
- Medium findings are fixed or formally accepted.
- Auditor retests the final commit.

# Release gates

## Scope gate

- Generic filesystem output absent.
- Filename handling absent.
- Message/wallet decompression absent.
- Arbitrary image-file loading absent.
- Unused modes absent.

## Memory-safety gate

- ASan and UBSan clean.
- No unresolved critical or high static-analysis findings.
- Length arithmetic reviewed.

## Resource-safety gate

- Size rejected before allocation.
- Memory and CPU budgets enforced.
- Block, frame, stream, and no-progress counters bounded.
- State returns to baseline after reset.

## Fuzzing gate

- Critical paths covered by harnesses.
- No known reproducible crash, timeout, or uncontrolled allocation.
- Regression corpus preserved.

## Isolation gate

- No secrets or network in decoder domain.
- IPC is fixed and bounded.
- Process restart clears state.
- Penetration test covers attempted boundary crossing.

## Review gate

- Independent assessment complete.
- Accepted risks documented.
- Release commit and artifact hashes recorded.
