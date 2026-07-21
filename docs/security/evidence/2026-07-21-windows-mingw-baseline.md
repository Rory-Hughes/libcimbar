# Windows MinGW Baseline Evidence

## Scope

- Baseline commit: 681e18eb61a059f4a796bc6ef097d24b45c430eb
- Worktree: C:\Users\rory\Documents\libcimbar-audit-baseline
- Submodule: samples at 9bbafc5ea0ad27e2a43ec9eee387dd60714a6c7f
  (heads/v0.6)
- Date: 2026-07-21
- CMake: 3.29.2
- Ninja: 1.12.0
- Compiler: MinGW-W64 GCC 13.2.0
- Dependency manager/triplet: vcpkg x64-mingw-dynamic
- Direct package observations: opencv4 4.11.0#4, glfw3 3.4#1, and
  opengl-registry 2024-02-10#1

The baseline worktree contained no source modifications after the build and
test commands. Generated build, install, and test artifacts are ignored.

## Baseline Build Results

The unmodified baseline was configured with:

    cmake -S . -B out/build/windows-mingw-vcpkg -G Ninja
      -DCMAKE_BUILD_TYPE=RelWithDebInfo
      -DCMAKE_TOOLCHAIN_FILE=C:\Users\rory\vcpkg\scripts\buildsystems\vcpkg.cmake
      -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

The first full build failed while compiling cimbar_js because the baseline
CMake files do not add the installed OpenGL Registry include directory:

    fatal error: GLES3/gl3.h: No such file or directory

The header exists at the vcpkg triplet include directory. A second clean
configuration added only this compiler include path:

    -DCMAKE_CXX_FLAGS=-IC:/Users/rory/vcpkg/installed/x64-mingw-dynamic/include

That configuration progressed past cimbar_js, then the full build failed in
compression_test and fountain_test. The common failure is that File and
write_on_store accept std::string while baseline tests pass
std::filesystem::path. This is a source compatibility issue exposed by GCC
13.2.0, not a test assertion failure.

The following target build succeeded under the include-path-adjusted
configuration:

    cmake --build out/build/windows-mingw-vcpkg --target cimbar --parallel 4

The all-target install command installed dist/bin/cimbar.exe but exited
nonzero when it reached an executable that had not been built
(cimbar_extract.exe). Full installation is therefore not reproducible until
the full build succeeds.

## Test Results

The following independently built CTest targets passed:

    ctest --test-dir out/build/windows-mingw-vcpkg
      -R ^(bit_file_test|chromatic_adaptation_test|cimb_translator_test)$
      --output-on-failure

Result: 3 of 3 passed in 1.89 seconds.

The CTest manifest defines nine tests. The other six cannot be run because
their executables were not built after the full build stopped.

The Python command-line usage suite initially failed because it expects the
installed dist/bin/cimbar program. After building the cimbar target and
prepending the reference vcpkg bin directory to PATH, it passed:

    py -m unittest discover -s test/py -p test_*.py -v

Result: 2 of 2 passed in 1.514 seconds.

## Binary Evidence

Installed binary:

    dist/bin/cimbar.exe

SHA-256:

    9B3AEE50077F3D217C50225030A4ABC995F4F63375B24E198632214BE3EF2482

PE import inspection reported libgcc_s_seh-1.dll, libstdc++-6.dll, Windows
UCRT and KERNEL32 DLLs, plus these OpenCV DLLs:

    libopencv_calib3d4.dll
    libopencv_core4.dll
    libopencv_flann4.dll
    libopencv_imgcodecs4.dll
    libopencv_imgproc4.dll

## Remaining Baseline Gaps

1. Repair or configure the baseline OpenGL Registry discovery without relying
   on a manually injected compiler include path.
2. Resolve the std::filesystem::path and std::string incompatibility so the
   full target set and all nine CTest executables build.
3. Make installation conditional on built targets or install only the selected
   product targets.
4. Re-run this evidence on a second environment and collect peak memory and
   performance after the full test suite succeeds.
