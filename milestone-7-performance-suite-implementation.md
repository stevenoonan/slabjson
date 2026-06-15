# Milestone 7: cJSON Performance and Conformance Suite

## Implemented

- Added the opt-in `SLABJSON_BUILD_BENCHMARKS` CMake option. Normal builds do
  not fetch or build benchmark dependencies.
- Pinned cJSON 1.7.19, Google Benchmark 1.9.5, JSONTestSuite commit
  `1ef36fa`, and nativejson-benchmark commit `478d572`.
- Added deterministic Python generation of compact and newline-formatted
  Twitter, CITM, and Canada profiles in the build directory.
- Added `slabjson_bench` with these benchmark families for both libraries:
  `ParseLifecycle`, `SerializeTransactional`, `SerializePartial`,
  `RoundTripCompact`, and `DuplicateRecursive`. `SerializedSize` separately
  measures SlabJson's non-writing preflight traversal.
- Added setup-time validation requiring full input consumption, successful
  reparsing of serialized output, and case-sensitive cJSON semantic equality.
- Added `slabjson_conformance`, covering all JSONTestSuite cases, generated
  corpus validation, output preflight behavior, embedded NUL handling, and
  capacity-limited canonical documents.
- Added `slabjson_memory_report`, which records SlabJson capacity use and cJSON
  current, peak, total, and allocation-count statistics without affecting the
  timing executable.
- Added one-iteration CTest smoke tests and the
  `slabjson_benchmark_report` target. No performance thresholds are enforced.

The generated profiles range from approximately 650 bytes to 36 KiB and fit
the 65,535-byte slab. The original Twitter, CITM, and Canada documents are
recorded as unsupported-capacity cases.

## Build And Run

```bash
cmake -S . -B build-bench \
  -DCMAKE_BUILD_TYPE=Release \
  -DSLABJSON_BUILD_TESTS=OFF \
  -DSLABJSON_BUILD_BENCHMARKS=ON

cmake --build build-bench -j
ctest --test-dir build-bench -R slabjson.benchmark
cmake --build build-bench --target slabjson_benchmark_report
```

The report target writes:

- `build-bench/benchmark-results/timings.json`
- `build-bench/benchmark-results/conformance.txt`
- `build-bench/benchmark-results/memory.csv`

Show a paired timing summary with:

```bash
python3 benchmarks/show_timing_report.py
```

The delta is relative to cJSON. A negative value means SlabJson is faster; a
positive value means SlabJson is slower. `SerializedSize` has no cJSON column
because cJSON does not expose an equivalent non-writing size operation.

The cJSON rows in both serialization families are the same
`cJSON_PrintPreallocated()` single-pass baseline. The two families distinguish
SlabJson's transactional and partial-write API contracts.

In the first five-repetition Release report after this split, partial
serialization was faster than cJSON for all 18 compact and formatted corpus
profiles, ranging from approximately 21% to 91% faster on the development
machine. Results remain machine-specific and informational.

Generated corpora and reports remain under the build directory and are not
committed.

## Conformance Policy

JSONTestSuite contains 95 required-valid, 188 required-invalid, and 35
implementation-defined cases. Required-valid and required-invalid SlabJson
regressions fail CTest. cJSON outcomes and implementation-defined outcomes are
reported for comparison but do not fail the suite.

Benchmark timing is intended for same-machine comparisons. Sanitizer builds
are useful for validating the tools, but their timing results are not
performance data.
