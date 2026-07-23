# Hardened IPC Parser Evidence

Date: 2026-07-23

Work package: WP-12, OIP-to-SCP IPC specification.

## Change validated

- Added `HardenedTransportIpc`, a dependency-free parser for the fixed
  OIP-to-SCP byte envelope.
- Added `docs/security/HARDENED_IPC.md` as the reviewed wire contract.
- Added hardened profile unit coverage for valid submit/completed/reset/status
  messages and fail-closed version, type, generation, size, limit, control
  payload, diagnostic-payload, and null-input cases.
- Added `fuzz_hardened_ipc` and ten deterministic corpus seeds under
  `fuzz/corpus/hardened_ipc/`.

## Parser contract

The accepted envelope is exactly one 24-byte little-endian `LCIP` header plus
an optional opaque payload. Version must be 1, flags must be zero, transfer
generation must be nonzero, message type and status code must be from fixed
enumerations, and the payload length must exactly match the remaining input.

Only `submit_frame` and `completed_object` may carry payloads. Control, status,
and failure messages reject all payload bytes, so decoder-originated IPC cannot
carry filenames, paths, pointers, nested objects, application commands, wallet
instructions, MIME types, or arbitrary diagnostic strings.

## Commands

```bash
py fuzz/corpus/generate.py --check
```

Result: passed.

```bash
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc "cmake --preset hardened-transport"
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc "cmake --build --preset hardened-transport"
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc "ctest --preset hardened-transport"
```

Result: configure and build passed. The hardened transport archive and linked
profile test both passed `VerifyHardenedTransportArtifact.cmake`. CTest passed
3/3: `hardened_transport_profile_test`,
`hardened_transport_verifier_rejects_forbidden`, and
`hardened_transport_sandbox_probe`.

```bash
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc \
  "cmake -S fuzz -B out/fuzz-wp12 -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc \
  "cmake --build out/fuzz-wp12 --target fuzz_hardened_ipc"
wsl.exe --cd "C:\Users\rory\Documents\libcimbar audit" bash -lc \
  "rm -rf out/fuzz-wp12-corpus out/fuzz-wp12-artifacts && \
   mkdir -p out/fuzz-wp12-corpus/hardened_ipc out/fuzz-wp12-artifacts && \
   cp fuzz/corpus/hardened_ipc/*.bin out/fuzz-wp12-corpus/hardened_ipc/ && \
   ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 \
   UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
   ./out/fuzz-wp12/fuzz_hardened_ipc \
     out/fuzz-wp12-corpus/hardened_ipc \
     -runs=10000 \
     -max_len=512 \
     -rss_limit_mb=128 \
     -timeout=5 \
     -artifact_prefix=out/fuzz-wp12-artifacts/"
```

Result: Clang 18.1.3 configured and built `fuzz_hardened_ipc`. The fuzzer
loaded 10 committed IPC seeds and completed 10,000 ASan/UBSan runs with no
crash, leak report, assertion failure, or sanitizer finding.

## Notes

- Native Windows configure for the WSL-generated `hardened-transport` cache
  reported the expected CMake source-directory mismatch, so validation used the
  established WSL invocation for this checkout.
- The fresh fuzzer build used `out/fuzz-wp12` to avoid reusing an incompatible
  existing `out/fuzz` compiler cache.
