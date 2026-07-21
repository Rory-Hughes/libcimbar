# Cppcheck First-Party Baseline

## Scope

- Date: 2026-07-21
- Tool: Cppcheck 2.14.0
- Source scope: non-test C and C++ translation units under src/lib and src/exe
- Checks: warning, performance, and portability
- Language mode: C++17
- Result: no first-party error-level result from the bounded scan

The local cppcheck.exe was built with an invalid hard-coded FILESDIR under
R:. The required std.cfg is present under the Strawberry installation. The run
used a temporary R: substitution backed by ignored workspace artifacts and
removed it in a finally block. Raw logs are retained under out/analysis and
are not release artifacts.

The broad all-configurations run was stopped after it spent several minutes in
vendored fmt template analysis. Its partial output was retained only as tool
diagnostic evidence. The baseline below excludes tests and vendored source,
avoids --force, and is the actionable result.

## Resolved Findings

### File ownership

Cppcheck reported that File owns a FILE handle but had implicit copy operations.
Copying it could have caused two destructors to close the same handle.

Remediation:

- Delete File copy construction and copy assignment.
- Add noexcept move construction and move assignment that transfer ownership.
- Add compile-time copy/move assertions and a runtime move-assignment regression
  test in the compression test executable.

Verification:

- CTest: 9 of 9 passed after the change.
- The post-fix scan no longer reports noCopyConstructor or noOperatorEq for
  File.

### Embedded image decoding

Cppcheck's void-pointer arithmetic report exposed a real channel-accounting
defect in `cimb_translator/Common.cpp`: stb_image was asked for four output
channels while allocation and copying used the source channel count. The image
also used `free` instead of stb_image's matching deallocator.

Remediation:

- Validate encoded size, dimensions, source channels, and pixel count before
  decoding.
- Decode to a fixed four-channel buffer owned with `stbi_image_free`.
- Convert into an owning three-channel OpenCV matrix before releasing the stb
  buffer.
- Add RGB, RGBA, malformed-input, null-input, zero-length, and oversized-length
  tests.

Verification:

- The focused `Common.cpp` cppcheck scan exits successfully with no report.
- The image decoder unit tests pass in `cimb_translator_test`.

## Reviewed Remaining Reports

| Location | Report | Triage |
| --- | --- | --- |
| cimb_translator/Cell.h | _cols and _rows uninitialized | False positive. Both constructors explicitly initialize both fields. |
| cimb_translator/FloodDecodePositions.h | _topWidth uninitialized | Dead private field. It is not read by the current source; remove or initialize during later cleanup. |
| gui and extractor constructors | initialization-list and pass-by-reference suggestions | Performance-only, no hostile-input memory-safety conclusion from this run. |
| cimbar.cpp iterator return | return-by-reference suggestion | Performance-only. Review with iterator lifetime context before changing the API. |

## Remaining Analysis Work

1. Run Clang ASan/UBSan and clang-tidy after a compatible Clang toolchain is
   installed.
2. Remove or initialize the unused FloodDecodePositions::_topWidth field.
3. Run a release-build static-analysis scan from an exported compilation
   database and include the generated findings in the release evidence bundle.
