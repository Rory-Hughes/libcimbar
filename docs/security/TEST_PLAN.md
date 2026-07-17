# Security Test Plan

## Test identity

Every recorded result must include:

- Repository and commit.
- Compiler and version.
- Build type and flags.
- Operating system or container digest.
- Architecture.
- OpenCV and dependency versions.
- Test corpus revision.
- Sanitizer options.

## Compiler configurations

Maintain at least:

- GCC release build.
- Clang release build.
- Clang ASan plus UBSan.
- Clang MemorySanitizer where a fully instrumented dependency stack is practical.
- Clang ThreadSanitizer for concurrent receiver paths.
- ARM64 build.
- Hardened product release build.

## Static analysis

Run:

- clang-tidy security and bug-prone checks.
- CodeQL C/C++ analysis.
- cppcheck.
- Clang static analyzer.
- Dependency and licence scanning.
- Secret scanning.

Custom review queries should target:

- Size-derived allocations.
- Signed and unsigned conversion.
- Shift and multiplication overflow.
- Pointer arithmetic.
- Unchecked library status.
- Unbounded containers.
- File creation.
- Assertions reachable from hostile input.
- Global mutable state.

## Fuzz targets

### Fountain metadata

Input arbitrary byte strings of at least six bytes. Exercise all field accessors and canonical serialization. Require deterministic, crash-free behaviour.

### Fountain state machine

Use structured operations:

- Start transfer.
- Submit block.
- Duplicate block.
- Change claimed size.
- Change transfer ID.
- Recover.
- Cancel.
- Timeout.
- Reset.

Assert bounded memory, bounded decoder count, rejection of conflicting metadata, and complete cleanup.

### Corrected payload

Fuzz the boundary after symbol and ECC handling. This target should be fast and receive the largest continuous-fuzzing allocation.

### Raw frame and geometry

Fuzz fixed-format raw images and constrained dimensions. Seed with valid frames, crops, rotations, noise, extreme contrast, damaged anchors, and real camera captures.

### End-to-end frame sequence

Represent a sequence including reordering, dropping, duplication, delay, pixel mutation, and metadata mutation. Ensure output is either the exact source object or no object.

### Compression and filenames

Audit-only targets for upstream generic functionality. These paths must not ship in the message or wallet transport profile.

## Property tests

- `decode(encode(payload)) == payload` for supported sizes and modes.
- Reordering and permitted loss do not alter recovered bytes.
- Duplicate frames do not alter recovered bytes.
- Conflicting metadata never produces output.
- Completion occurs at most once.
- Reset returns all state and resource counters to baseline.
- Different compiler and architecture builds produce byte-identical successful output.

## Boundary corpus

Include:

- Zero and minimum valid lengths.
- Every length around header and chunk boundaries.
- Maximum allowed object for each profile.
- One block and many blocks.
- Block IDs at zero, one, maximum-minus-one, maximum, and overflow boundaries.
- Repeated duplicate blocks.
- Same transfer ID with changed size.
- Same block ID with changed payload.
- Correctable and uncorrectable ECC errors.
- Empty, all-zero, all-one, patterned, and high-entropy content.

## Resource testing

Record peak memory, CPU time, per-frame latency, and state counts for:

- Valid transfer.
- Oversized size metadata.
- Endless unique blocks.
- Endless duplicate blocks.
- No-progress sequences.
- Almost-complete transfers.
- Rapid transfer cancellation.
- Repeated decoder restart.

## Failure-path testing

Inject:

- Camera disconnect.
- IPC disconnect.
- Allocation failure.
- Wirehair initialization and decode failures.
- Output-buffer refusal.
- Watchdog timeout.
- Power interruption in the surrounding device integration.

After every failure, the next valid transfer must start from clean state.

## CI tiers

### Pull request

- Build and unit tests.
- ASan/UBSan tests.
- Static analysis.
- Short fuzz smoke run.

### Nightly

- Longer parallel fuzzing.
- Corpus minimization.
- Cross-build differential tests.
- Leak and resource-growth loops.

### Release candidate

- Full regression corpus.
- Extended continuous fuzzing.
- ARM64 hardware test.
- Adversarial optical laboratory suite.
- Sandbox boundary test.
- Independent-auditor retest.

## Acceptance rule

Coverage percentage is supporting evidence, not the acceptance criterion. Critical branches, error paths, and state transitions must be explicitly tied to tests.
