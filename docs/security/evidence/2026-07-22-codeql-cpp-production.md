# CodeQL C and C++ Production Baseline

## Scope

- Date: 2026-07-22
- Branch: `hardening-scaffold`
- Reviewed commit before working-tree remediation: `b028e9d18d662ac07bdd7f030887843dd06a80e0`
- Platform: Ubuntu 24.04 under WSL2, x86-64
- Compiler: Clang 18.1.3, `RelWithDebInfo`
- CodeQL CLI: 2.26.1
- C/C++ query pack: `codeql/cpp-queries` 1.7.0
- Query suite: `cpp-security-extended.qls`
- Downloaded bundle SHA-256: `38eca75ea296a6c48c52aff942b0678f06d3367140a5aa18caf80176943422ba`
- Extracted database baseline: 89,814 C/C++ lines of code

The manual build uses the `codeql` CMake configure and build presets. It traced
80 production compilation units while building the image/geometry libraries,
browser receiver, CLI tools, libcorrect, Wirehair, zstd, and the byte-only
hardened transport. Unit-test translation units are intentionally excluded.

## Initial finding and remediation

The initial suite returned 42 results: one first-party result and 41 vendored
results. `cpp/integer-multiplication-cast-to-long` identified
`DecoderPlus::save_ccm`, where matrix dimensions and element size were
multiplied before conversion to the unsigned length accepted by `File::write`.
The function also treated any non-zero result as success, including a partial
write.

The repair now requires a non-empty contiguous matrix, proves that the exact
byte count fits in `unsigned`, performs the product in `std::size_t`, and
requires `File::write` to return the complete byte count. A regression writes a
3 by 3 float color-correction matrix and compares the exact input and output
hashes.

## Final result

The database was deleted and recreated from a clean `out/build/codeql`
directory after remediation. The final suite returned:

```text
Total results:                   41
First-party production results:  0
Vendored results:                41
Vendored distribution:           fmt 1, stb 7, Wirehair 9, zstd 24
```

`scripts/check-codeql-sarif.py` independently accepted the generated SARIF
with zero first-party production findings. The maintained GitHub workflow uses
CodeQL Action v4, manual C/C++ build mode, and `security-extended`; it retains
the raw SARIF and runs the same ownership gate after analysis.

## Vendored triage

The fmt, stb, and zstd results are retained for dependency-update review. Those
dependencies are absent from the hardened byte-only artifact; they remain
reachable through compatibility image and compression targets.

The nine Wirehair results were reviewed at their source locations:

- Three `cpp/suspicious-pointer-scaling` reports are gf256 SIMD-lane pointer
  increments. The pointer's declared type is 128-bit and each increment is one
  16-byte lane; nearby AVX2 casts obscure this relationship from the query.
- Five `cpp/comparison-with-wider-type` reports compare 16-bit matrix indices
  against fields derived from Wirehair's validated maximum of 64,000 blocks.
- One `cpp/integer-multiplication-cast-to-long` report sizes the elimination
  matrix after the same bound. Its maximum dimensions keep the unsigned product
  below overflow before conversion.

These reports are classified as invariant-safe in the audited revision rather
than suppressed. Upstream comparison showed the loop forms remain current.

## Adjacent Wirehair defect found during triage

The review found a real issue that the suite did not report. Vendored
`Codec::ChooseMatrix` computed a wide message block count and immediately cast
it to `uint16_t`; only the narrowed value was checked against the 64,000-block
maximum. For example, a 65,538-byte message with one-byte blocks wrapped to two
blocks and could initialize codec state for the wrong dimensions.

The current upstream Wirehair implementation already validates before
narrowing. The applicable validation was backported: ceiling division is
performed without addition overflow, the wide result is checked against both
block-count bounds, block sizes above `INT32_MAX` are rejected for gf256, and a
32-bit host offset check is retained. A direct `FountainDecoder` regression for
65,538 one-byte blocks now rejects both the memory estimate and codec creation.

## Reproduction

CI reproduction:

```sh
cmake --preset codeql
cmake --build --preset codeql
```

The `security-codeql` workflow wraps that build in CodeQL initialization and
analysis. The local evidence database and final SARIF were retained under
`/home/byanymeans/.cache/libcimbar-codeql/` for inspection.

## Closing verification

```text
CodeQL security-extended:         0 first-party, 41 vendored
CodeQL SARIF ownership gate:      passed
ASan/UBSan full suite:            12/12 passed
Hardened release profile:          2/2 passed, archive and executable verified
MemorySanitizer focused profile:   1/1 passed
ThreadSanitizer focused paths:     2 cases, 15 assertions passed
Fountain fuzz corpus integrity:   19 seeds passed
SPDX SBOM regeneration/check:     passed
```

## Residual work

- Identify immutable upstream revisions for every vendored dependency.
- Re-evaluate all vendored findings when updating fmt, stb, Wirehair, or zstd.
- Keep compatibility compression and image surfaces outside the hardened
  transport artifact unless their risk and resource policies are accepted.
