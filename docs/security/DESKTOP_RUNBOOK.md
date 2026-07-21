# Desktop Audit Bootstrap Runbook

Use this when returning to a development workstation. It preserves the frozen baseline and keeps generated files out of the source tree.

## 1. Clone and verify

```bash
git clone --recursive https://github.com/Rory-Hughes/libcimbar.git
cd libcimbar
git fetch --all --tags --prune
git rev-parse audit-baseline-681e18e
git rev-parse hardening-scaffold
```

The frozen baseline must resolve to:

```text
681e18eb61a059f4a796bc6ef097d24b45c430eb
```

Create a local working branch from the remote scaffold:

```bash
git switch --track origin/hardening-scaffold
```

Do not commit to `audit-baseline-681e18e`.

## 2. Record the environment

Create an evidence directory outside the source tree:

```bash
mkdir -p ../libcimbar-audit-evidence/environment
```

Record:

```bash
{
  date -u
  uname -a
  git rev-parse HEAD
  git submodule status --recursive
  cmake --version
  clang --version
  gcc --version
  ninja --version
  pkg-config --modversion opencv4 || true
} | tee ../libcimbar-audit-evidence/environment/toolchain.txt
```

On Debian or Ubuntu, install initial dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  cmake \
  ninja-build \
  cppcheck \
  clang-tidy \
  libopencv-dev \
  libglfw3-dev \
  libgles2-mesa-dev
```

On Windows with the local MinGW/vcpkg toolchain, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-vcpkg.ps1
```

The Windows setup builds the non-GUI audit baseline with `LIBCIMBAR_BUILD_GUI_TOOLS=OFF`.
The live GLFW/OpenGL send and receive tools need a separate OpenGL ES function loader or implementation on Windows.

## 3. Reproduce the frozen baseline

Use a detached worktree so the baseline cannot be accidentally modified:

```bash
git worktree add --detach ../libcimbar-baseline audit-baseline-681e18e
cmake -S ../libcimbar-baseline -B ../build-libcimbar-baseline -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build ../build-libcimbar-baseline
ctest --test-dir ../build-libcimbar-baseline --output-on-failure \
  | tee ../libcimbar-audit-evidence/baseline-ctest.txt
```

Run the upstream Python usage tests from their expected directory after the build:

```bash
(
  cd ../libcimbar-baseline/test/py
  python3 -m unittest
) | tee ../libcimbar-audit-evidence/baseline-python-tests.txt
```

Record built binaries and hashes:

```bash
find ../build-libcimbar-baseline -type f -perm -111 -print0 \
  | sort -z \
  | xargs -0 sha256sum \
  | tee ../libcimbar-audit-evidence/baseline-binary-sha256.txt
```

## 4. Run ASan and UBSan

From the `hardening-scaffold` worktree:

```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan \
  | tee ../libcimbar-audit-evidence/asan-ubsan-ctest.txt
```

Preserve any sanitizer output before changing code.

## 5. Build and run the first fuzzer

```bash
cmake -S fuzz -B out/fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build out/fuzz
mkdir -p out/fuzz-artifacts fuzz/corpus/fountain_metadata
./out/fuzz/fuzz_fountain_metadata \
  fuzz/corpus/fountain_metadata \
  -max_len=256 \
  -timeout=5 \
  -rss_limit_mb=2048 \
  -artifact_prefix=out/fuzz-artifacts/
```

Begin with a short run, then allow a longer session after confirming stability. Do not commit a sensitive crash artifact before coordinating disclosure.

## 6. Run initial static analysis

```bash
cppcheck --enable=warning,style,performance,portability \
  --inline-suppr \
  --std=c++17 \
  src 2>&1 \
  | tee ../libcimbar-audit-evidence/cppcheck.txt
```

After generating `compile_commands.json` through a preset build:

```bash
run-clang-tidy -p out/build/asan-ubsan 2>&1 \
  | tee ../libcimbar-audit-evidence/clang-tidy.txt
```

Do not bulk-fix warnings before recording and triaging the baseline.

## 7. Generate a preliminary dependency inventory

Record system-linked libraries:

```bash
find out/build/asan-ubsan -type f -perm -111 -print0 | while IFS= read -r -d '' file; do
  echo "### $file"
  ldd "$file" || true
done | tee ../libcimbar-audit-evidence/runtime-dependencies.txt
```

Inventory vendored directories and submodules:

```bash
find src/third_party_lib -maxdepth 2 -type f \
  \( -name LICENSE -o -name LICENSE.txt -o -name COPYING -o -name README.md \) \
  -print | sort \
  | tee ../libcimbar-audit-evidence/vendored-license-files.txt

git submodule status --recursive \
  | tee ../libcimbar-audit-evidence/submodules.txt
```

An SPDX or CycloneDX SBOM should be added after the exact build dependency set is understood.

## 8. First review targets

Begin manual review in this order:

1. `src/lib/fountain/FountainMetadata.h`
2. `src/lib/fountain/fountain_decoder_sink.h`
3. `src/lib/fountain/fountain_decoder_stream.h`
4. `src/lib/fountain/FountainDecoder.h`
5. zstd wrapper and header/filename handling
6. image extractor and geometry code
7. receiver concurrency and shutdown paths

Record observations using `FINDING_TEMPLATE.md`. Label unconfirmed concerns as audit leads rather than vulnerabilities.

## 9. First working branches

Suggested branches:

```text
audit/dependency-inventory
audit/static-analysis-baseline
test/fuzz-fountain-state
hardening/bounded-transfer-policy
hardening/remove-filesystem-profile
hardening/remove-zstd-message-wallet
```

Keep changes small enough that each security property can be tested and reviewed independently.

## 10. Evidence handling

- Keep public, non-sensitive test output in repository issues or pull requests when useful.
- Keep potential exploit details and crash artifacts private until disclosure is coordinated.
- Record exact hashes for every private reproducer.
- Never use production keys, private messages, or real wallet material in testing.
