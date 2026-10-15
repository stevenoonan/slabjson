# Contributing to SlabJson

Thank you for improving SlabJson. Changes should preserve its fixed-capacity,
allocation-free core and its C++20 portability.

## Development setup

Configure, build, and test a Debug checkout with:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Before opening a pull request, also run the Release and no-exceptions presets:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset no-exceptions
cmake --build --preset no-exceptions
ctest --preset no-exceptions
```

The sanitizer preset requires a compiler that supports AddressSanitizer and
UndefinedBehaviorSanitizer.

The normal test suite checks core operations with C++ allocation counters;
Linux also wraps direct `malloc`, `calloc`, and `realloc` calls. Positive
controls verify that allocation detection works before exercising the core.
Allocations hidden inside shared runtime libraries are outside the C wrappers.

Run JSONTestSuite without building benchmarks:

```bash
cmake -S . -B build-conformance -DSLABJSON_BUILD_TESTS=OFF \
  -DSLABJSON_BUILD_CONFORMANCE=ON
cmake --build build-conformance
ctest --test-dir build-conformance --output-on-failure
```

This fetches the pinned JSONTestSuite data. For an offline checkout, set
`SLABJSON_JSON_TEST_SUITE_DIR` to its existing `test_parsing` directory.

Run the Clang fuzz target with a writable copy of the seed corpus:

```bash
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSLABJSON_BUILD_TESTS=OFF \
  -DSLABJSON_BUILD_FUZZERS=ON
cmake --build build-fuzz
cp -R fuzz/corpus build-fuzz/corpus
build-fuzz/fuzz/slabjson_fuzz build-fuzz/corpus -max_total_time=30 -max_len=8192
```

CI runs standalone conformance and a bounded 30-second sanitizer fuzz pass on
each pull request. The fuzz target checks parse and duplicate rollback,
serialization sizes, compact/pretty round trips, partial output, and unchanged
undersized transactional output. Fuzzing requires Clang with libFuzzer support.

## Pull requests

- Keep each change focused and include tests for observable behavior.
- Do not introduce heap allocation, exceptions, or RTTI into the core library.
- Preserve transactional failure behavior where the API promises it.
- Update `README.md` and `CHANGELOG.md` for user-visible changes.
- Treat public headers and the `slabjson::slabjson` CMake target as a deliberate
  pre-1.0 API. Document breaking changes and reserve them for a new 0.x minor
  release.
- Ensure all CI checks pass before requesting review.

By contributing, you agree that your contribution is licensed under the MIT
License in this repository.
