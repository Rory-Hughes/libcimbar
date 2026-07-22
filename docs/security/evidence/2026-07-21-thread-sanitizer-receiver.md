# ThreadSanitizer Receiver-Concurrency Review

## Scope

- Date: 2026-07-21
- Branch: `hardening-scaffold`
- Reviewed commit before working-tree remediation: `b028e9d18d662ac07bdd7f030887843dd06a80e0`
- Platform: Ubuntu 24.04 under WSL2, x86-64
- Toolchain: Clang 18.1.3 and CMake 3.28.3
- Instrumentation: `-fsanitize=thread -fno-omit-frame-pointer`
- Runtime options: `halt_on_error=1:second_deadlock_stack=1`

The release-relevant scope was the fountain receiver's concurrent submission
wrapper and process-global Wirehair initialization. The legacy `cimbard_*`
receive API was reviewed for ownership but is not a supported concurrent API
and is not linked into `cimbar_hardened_transport`.

## Baseline Build Blocker

The existing TSan preset initially failed before linking because vendored
libcorrect unconditionally appended `-fsanitize=address` to every Debug C
build. Clang correctly rejected the resulting combination:

```text
clang: error: invalid argument '-fsanitize=address' not allowed with '-fsanitize=thread'
```

The vendored build no longer selects a sanitizer. The top-level ASan/UBSan and
TSan presets now control instrumentation consistently for all targets.

## Confirmed Concurrency Defects

### Decoder state read without ownership

`concurrent_fountain_decoder_sink::good`, `chunk_size`, `num_streams`, and
`num_done` read the wrapped decoder while a producer could mutate its
containers. Each accessor now takes the same mutex that owns decoder mutation.

### Stranded final queue item and non-exception-safe lock

The former drainer used `try_lock`, drained until one empty observation, then
unlocked manually. A producer could enqueue after that observation, fail its
own `try_lock`, and return with no future drainer. A decoder allocation or
completion callback exception could also permanently retain the manually held
mutex.

Every successful producer now blocks until it becomes a drainer, and
`std::lock_guard` releases ownership on all exits. Null input and queue
allocation failure are no longer silently accepted.

### Per-translation-unit Wirehair initialization

`FountainInit::init` was a `static` header function. Each translation unit
therefore had a separate function-local static and could concurrently call
Wirehair's unsynchronized process-global initializer. Making the function
`inline` gives the local static one program-wide identity. A dedicated test
calls it concurrently from two translation units before any codec is created.

## Compatibility API Disposition

The `cimbard_*` receiver uses process-global decoder, image, reassembly,
decompression, reporting, and timing state, while `cimbar::Config` is
thread-local. Its raw reassembled-buffer getter also outlives any function-level
lock. A mutex-only patch would therefore provide a misleading contract.

The public header now requires serialized use by one worker/thread, including
reset and reconfiguration. This API remains compatibility-only. The verified
hardened transport target uses instance-owned byte-only state and does not link
the C receiver, decompressor, filesystem output, or raw-buffer getter.

## TSan Verification

Focused build and direct gates:

```text
cmake --preset tsan
cmake --build --preset tsan --parallel 2
fountain_init_concurrency_test
fountain_test "[concurrency]"

All tests passed (15 assertions in 2 concurrent-sink test cases)
No ThreadSanitizer report
```

The sink regression runs four producers against distinct encoded frames while
an observer repeatedly reads live decoder state and snapshots. It verifies one
exact completion, no failed submission, no retained active stream, and one
completion record.

The complete fountain test set also passed with TSan:

```text
ctest --test-dir out/build/tsan \
  -R '^fountain_(test|init_concurrency_test)$' --output-on-failure

100% tests passed, 0 tests failed out of 2
```

## Cross-Sanitizer and Artifact Regression

```text
ASan/UBSan full suite: 100% passed, 0 failed out of 12
Hardened release profile: 100% passed, 0 failed out of 2
Hardened archive verifier: passed
Hardened linked-test verifier: passed
```

The GitHub sanitizer workflow now has a separate TSan job that builds only the
two receiver-concurrency targets and fails on the first race report.

## Residual Risk

- `cimbard_*` is not safe for concurrent calls and must remain one-worker-only.
- `HardenedFountainTransport` instances are not internally synchronized; one
  instance must have one owner, or its caller must serialize access.
- External code can bypass `FountainInit::init` and call the raw vendored
  `wirehair_init` function concurrently; the hardened artifact does not do so.
- TSan validates observed executions, not all schedules. The lock/ownership
  review and deterministic regression are required alongside the clean run.
