# Decoder Sandbox Prototype Evidence - 2026-07-22

## Scope

- Work package: WP-11 decoder sandbox prototype
- Components: `hardened_transport_sandbox_probe`, hardened transport preset,
  Linux process limits, seccomp-BPF, watchdog child runner
- Boundary: corrected fountain packets into the byte-only
  `HardenedFountainTransport` profile

## Remediation

- Added `src/profile/hardened_transport/test/LinuxSandboxProbe.cpp`.
- Added the opt-in `LIBCIMBAR_BUILD_LINUX_SANDBOX_PROTOTYPE` CMake option.
- Enabled the option in the `hardened-transport` preset and included the probe
  in the preset's CTest filter.
- Added an Ubuntu CI job that configures, builds, and tests the hardened
  transport preset.
- The probe forks a fresh child process per transfer and the parent enforces a
  two-second watchdog.
- The child drops root to uid/gid 65534 when needed and verifies it is non-root
  with no effective capabilities.
- The child sets `no_new_privs`, disables core dumps, applies memory, CPU,
  process-count, file-size, core, and descriptor limits, and attempts network
  namespace isolation.
- Before decoding, the child installs a seccomp-BPF allowlist that permits only
  basic exit, signal, memory, time, identity, futex, read, write, and close
  syscalls needed by the corrected-packet decoder path.
- After an exact-byte decode, the deliberately compromised child attempts to
  read a parent-created secret file, write that file, create a socket, fork,
  exec `/bin/true`, and regain uid 0. Each escape attempt must fail with
  `EPERM`.

## Boundary Note

The prototype is Linux x86_64 only. Network namespace setup is best effort
because unprivileged environments can return `EPERM`; network socket creation
is still denied by the seccomp allowlist. This is a process sandbox prototype,
not the final OIP-to-SCP IPC contract.

## Validation

Environment:

- WSL Ubuntu
- CMake 3.28.3
- Clang 18.1.3
- Linux 6.6.87.2 Microsoft WSL2 kernel

Commands:

```text
py -c "import json, pathlib; json.loads(pathlib.Path('CMakePresets.json').read_text()); print('CMakePresets.json OK')"
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && cmake --preset hardened-transport"
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && cmake --build --preset hardened-transport"
wsl bash -lc "cd '/mnt/c/Users/rory/Documents/libcimbar audit' && ctest --preset hardened-transport"
```

Results:

- `CMakePresets.json OK`
- `cmake --preset hardened-transport` configured successfully with
  `LIBCIMBAR_BUILD_LINUX_SANDBOX_PROTOTYPE=ON`.
- `cmake --build --preset hardened-transport` built
  `cimbar_hardened_transport`, `hardened_transport_profile_test`, and
  `hardened_transport_sandbox_probe`.
- Hardened artifact verification passed for the archive and linked profile
  test.
- `ctest --preset hardened-transport` passed all three tests:
  `hardened_transport_profile_test`,
  `hardened_transport_verifier_rejects_forbidden`, and
  `hardened_transport_sandbox_probe`.

## Remaining Risk

- The sandbox probe exercises corrected fountain packets, not raw camera frames
  or the future IPC parser.
- The seccomp allowlist is a prototype. The final service should regenerate it
  from the production child process under representative compiler, libc, and
  sanitizer-free release settings.
- The final WP-12 IPC design still needs fixed message types, transfer
  generation, parser fuzzing, and separate Secure Core authentication of the
  completed opaque object.
