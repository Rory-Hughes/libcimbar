# Hardened Transport Artifact Evidence

## Scope

- Date: 2026-07-21
- Target: `cimbar_hardened_transport`
- Linked validation artifact: `hardened_transport_profile_test`
- Toolchain: Clang 18.1.3 and CMake/Ninja under WSL

## Finding

The restricted fountain session had a narrow source API and no direct zstd or
filesystem includes, but it remained header-only. There was no independently
buildable product artifact, no enforced transitive link boundary, and no binary
check preventing compatibility output code from entering a future product
link.

## Remediation

- Added `LIBCIMBAR_BUILD_HARDENED_TRANSPORT`, enabled by default.
- Added the compiled `HardenedFountainTransport` façade. Its API accepts an
  explicit transfer policy and corrected packet bytes, then returns one opaque
  exact-length byte vector. It exposes no output callback or application
  semantics.
- Added the `hardened-transport` configure, build, and test presets.
- Restricted the direct CMake link interface to exactly `wirehair`; configuration
  fails if another dependency is added.
- Added a post-build scanner for the static archive and fully linked profile
  test. It rejects zstd, decompression helpers, filesystem and file-stream
  symbols, filename parsers, browser compatibility output functions, and
  session test-fault controls.
- Added the always-executed `verify_hardened_transport` target so the named and
  default builds rescan existing artifacts when only verifier policy changes.
- Added exact-byte message and wallet reconstruction coverage, including
  second-take refusal.
- Added a negative-control test that requires the verifier to reject a
  synthetic artifact containing a forbidden zstd marker.

## Release-Profile Verification

```text
cmake --preset hardened-transport: passed
cmake --build --preset hardened-transport: passed
cimbar_hardened_transport archive scan: passed
hardened_transport_profile_test linked-artifact scan: passed
ctest --preset hardened-transport: 2/2 passed
full ASan/UBSan CTest: 11/11 passed
compiler header-dependency scan: no forbidden header
release compile-command scan: no forbidden path or define
```

The named build compiled only nine target-closure steps: Wirehair, the hardened
transport archive, and the linked profile test. zstd was not part of the target
closure. The archive's only undefined project-library symbols were
`wirehair_decode`, `wirehair_decoder_create`, `wirehair_free`, `wirehair_init_`,
and `wirehair_recover`. `ldd` on the release linked test reported only the Linux
loader, libc, libm, libgcc_s, and libstdc++; no zstd or OpenCV library was
linked.

## Remaining Risk

- This artifact begins at corrected fountain packets. The full product still
  needs a raw-frame `DecoderSession` that composes the checked camera boundary
  with this library.
- Root CMake configuration discovers OpenCV even for a transport-only build,
  although the hardened transport artifact does not link it.
- Wirehair's internal allocation overhead is not yet charged against the total
  decoder memory budget.
- Process sandboxing and the Secure-Core IPC boundary remain separate work
  packages.
