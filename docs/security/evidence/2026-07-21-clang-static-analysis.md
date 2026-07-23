# Clang First-Party Static-Analysis Baseline

## Scope

- Date: 2026-07-21
- Branch: `hardening-scaffold`
- Reviewed commit before working-tree remediation: `b028e9d18d662ac07bdd7f030887843dd06a80e0`
- Platform: Ubuntu 24.04 under WSL2, x86-64
- Toolchain: clang-tidy 18.1.3, Clang static analyzer 18.1.3
- Compilation database: `out/build/hardened-transport/compile_commands.json`
- Tidy scope: 15 non-test first-party translation units under `src/lib`
- Analyzer scope: first-party production sources, excluding tests, executables,
  and vendored translation units

Graphify's code-only map contained 1,274 nodes and 2,561 edges from 152 source
files. It was used to prioritize hostile-input functions with pointer
arithmetic, size-derived allocation, buffer copying, and persistent mutation.
Generated graph artifacts were removed after the review.

## clang-tidy Configuration

The blocking gate enables path-sensitive core, C++, security, and Unix
analyzer checks plus a narrow set of bug-prone memory/arithmetic and CERT
checks. System and vendored headers are not part of the actionable file scope.

A broader exploratory pass also enabled all `bugprone-*` and `cert-*` checks.
It was dominated by implementation-defined conversions in fixed configuration
geometry and style reports. Those are retained as review backlog rather than
being bulk-cast or mechanically suppressed.

## Confirmed Finding: Rectangular Cell Traversal

`Cell::mean_rgb` traversed non-contiguous matrices with rows and columns
reversed. For a 4 by 6 ROI it read six pixels from four backing rows instead
of four pixels from six rows, crossing the logical ROI on each row. The
continuous RGB and grayscale variants also formed pixel offsets using signed
products without validating the requested region. All accumulators and sample
counts were 16-bit and silently overflowed beyond small cell sizes.

The existing asymmetric regression expected `191`, the value produced by the
incorrect traversal. OpenCV's mean for the actual ROI is `233`; the corrected
implementation and authoritative comparison exposed the stale expectation.

Remediation:

- Validate matrix depth, channel count, positive region dimensions, and all
  region bounds before reading.
- Traverse by `cv::Mat::ptr(row)` so continuous and strided matrices use one
  implementation.
- Compute pixel offsets in `std::size_t` only after bounds validation.
- Accumulate totals in `std::uint64_t` and sample counts in `std::size_t`.
- Add rectangular strided RGB, large continuous RGB, grayscale subregion, and
  invalid-region tests.

## Additional Hardening

- `escrow_buffer_writer` now rejects null buffers, null input, zero-sized
  contracts, and spans larger than `PTRDIFF_MAX`; buffer offsets are calculated
  in `std::size_t` after constructor validation.
- `File::close` now returns the actual `fclose` result instead of reporting
  success unconditionally.
- Constant size products are evaluated directly in their destination-width
  integer types.
- A reserved local identifier and avoidable static-initialization report were
  removed without changing protocol behavior.

## Static Analyzer Result

The standalone analyzer emitted 13 dead-store findings, all inside vendored
`src/third_party_lib/stb/stb_image.h`, and zero first-party production
findings. `scripts/check-clang-sarif.py` enforces this ownership distinction and
fails if a merged SARIF result points into non-test `src/lib`, `src/profile`,
or `src/exe` code.

The vendored stb reports are not treated as first-party fixes. They should be
re-evaluated when the fork identifies and upgrades the stb revision.

## Verification

```text
Focused post-fix clang-tidy: 0 first-party diagnostics
Clang static analyzer:       0 first-party findings, 13 vendored exclusions
ASan/UBSan translator test:  passed
ASan/UBSan encoder test:     passed
```

The translator run executes 43 test cases and 12,852 assertions, including the
new hostile-region regressions. The encoder test includes null-pointer and
impossible-span escrow contracts.

## CI Gate

`.github/workflows/security-static-analysis.yml` regenerates the release
compilation database, treats every focused clang-tidy diagnostic as an error,
runs the standalone analyzer, ownership-filters its SARIF, and uploads the raw
merged SARIF artifact for review.

## Residual Work

- Run CodeQL independently; do not treat Clang's clean result as equivalent.
- Attempt MemorySanitizer only with a compatible instrumented dependency
  closure.
- Triage geometry conversion reports as part of the raw-frame/geometry fuzzing
  work, where fixed product dimensions and CPU bounds can be applied.
