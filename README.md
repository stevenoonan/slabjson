# SlabJson

[![CI](https://github.com/stevenoonan/slabjson/actions/workflows/ci.yml/badge.svg)](https://github.com/stevenoonan/slabjson/actions/workflows/ci.yml)
[![CodeQL](https://github.com/stevenoonan/slabjson/actions/workflows/codeql.yml/badge.svg)](https://github.com/stevenoonan/slabjson/actions/workflows/codeql.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**SlabJson** is a C++20 JSON library that does not use heap allocation. This makes it ideal for memory-constrained systems, embedded firmware, and other environments where predictable memory behavior matters.

Instead of allocating a JSON tree with `malloc()`/`free()` or `new`/`delete`, SlabJson stores the entire DOM inside a caller-provided fixed-size memory slab. Values are represented by lightweight handles, and all storage is reclaimed at once by resetting the slab.

This also makes SlabJson very fast. In the included benchmark suite, SlabJson is typically around 2 - 4× faster than cJSON.

## Why SlabJson?

SlabJson is intended for projects that need JSON but do not want the failure modes of a heap-backed DOM parser on small targets.

It is especially suitable for:

* MCU firmware
* RTOS applications
* IoT device protocols
* fixed-buffer message handling
* deterministic test fixtures
* cJSON migration paths
* systems compiled with exceptions and RTTI disabled

## Features

* C++20 baseline
* No heap allocation required by the core library
* No exceptions required
* No RTTI required
* Caller-provided fixed-size slab storage
* `StaticSlab<N>` convenience type for owned fixed storage
* 16-bit node IDs and string offsets
* Maximum slab capacity of 65,535 bytes
* Compact 24-byte internal node representation
* Strings stored from the opposite end of the slab
* Object, array, string, bool, null, signed integer, unsigned integer, and floating-point values
* Exact `int64_t`, `uint64_t`, and `double` number storage
* UTF-8 validation
* JSON escaping and unescaping
* Transactional parse and duplicate operations
* Transactional serialization API
* Single-pass partial serialization API
* Compact and pretty serialization
* cJSON-style compatibility helpers
* Allocation-free host test suite and JSONTestSuite conformance runner

## Quick start

```cpp
#include <array>
#include <cstddef>
#include <span>
#include <slabjson/slabjson.hpp>

int main()
{
    slabjson::StaticSlab<4096> slab;

    auto root_result = slab.make_object();
    if (!root_result) {
        return 1;
    }

    auto root = root_result.value();

    root.add("device_id", "widget-123");
    root.add("battery_mv", 4120);
    root.add("connected", true);

    auto tags_result = root.add_array("tags");
    if (!tags_result) {
        return 1;
    }

    auto tags = tags_result.value();
    tags.add("widget");
    tags.add("production");

    if (!root.status()) {
        return 1;
    }

    std::array<char, 512> output{};
    auto written = slabjson::serialize(root, std::span<char>{output});
    if (!written) {
        return 1;
    }

    // output[0..written.value()) contains compact JSON.
    return 0;
}
```

`Object::add*` and `Array::add*` record the first addition error in the slab.
Later additions through any handle for that slab are no-ops and return the same
error, so a construction sequence can use one final `status()` check. An
individual addition result can still be checked immediately. Call
`clear_error()` to recover explicitly, or `reset()` to clear both the DOM and
the recorded error.

Example output:

```json
{"device_id":"widget-123","battery_mv":4120,"connected":true,"tags":["widget","production"]}
```

## Parsing

```cpp
#include <array>
#include <cstddef>
#include <string_view>
#include <slabjson/slabjson.hpp>

void handle_payload(std::string_view payload)
{
    slabjson::StaticSlab<4096> slab;

    auto parsed = slabjson::parse(slab, payload);
    if (!parsed) {
        auto error = parsed.error();
        // error.code describes the failure.
        // error.offset is set for parse errors when applicable.
        return;
    }

    auto root = parsed.value().as_object();
    if (!root) {
        return;
    }

    auto device_id = root->get_string("device_id");
    if (!device_id) {
        return;
    }

    auto battery_mv = root->get_int64("battery_mv");
    if (!battery_mv) {
        return;
    }

    // Use device_id.value() and battery_mv.value().
}
```

## Serialization contracts

SlabJson provides two serialization styles.

### Transactional serialization

```cpp
auto written = slabjson::serialize(value, output);
```

`serialize()` is transactional. It validates and sizes the complete output before writing. If the output buffer is too small, the buffer is not modified.

Pretty transactional serialization is also available:

```cpp
auto written = slabjson::serialize_pretty(value, output, 2);
```

### Partial serialization

```cpp
auto written = slabjson::serialize_partial(value, output);
```

`serialize_partial()` performs one checked traversal. It is faster and uses a simpler contract: if the output buffer is too small, the buffer may contain partial output.

Pretty partial serialization is also available:

```cpp
auto written = slabjson::serialize_pretty_partial(value, output, 2);
```

Use transactional serialization when the caller needs all-or-nothing output. Use partial serialization when the caller already controls buffer sizing or can tolerate partial output on failure.

## Fixed storage model

A slab can either use caller-provided memory:

```cpp
std::array<std::byte, 4096> storage{};
slabjson::Slab slab{std::span<std::byte>{storage}};
```

or owned static storage:

```cpp
slabjson::StaticSlab<4096> slab;
```

The slab owns the lifetime of every value handle created from it.

```cpp
slab.reset();
```

`reset()` reclaims all storage at once and invalidates all existing `Value`, `Object`, and `Array` handles from that slab.

You can inspect slab usage:

```cpp
auto used = slab.used_bytes();
auto capacity = slab.capacity_bytes();
auto remaining = slab.remaining_bytes();
```

## Object and array access

Objects support typed convenience accessors:

```cpp
auto name = object.get_string("name");
auto enabled = object.get_bool("enabled");
auto count = object.get_int64("count");
auto child = object.get_object("child");
auto items = object.get_array("items");
```

Objects and arrays are iterable:

```cpp
for (auto member : object) {
    std::string_view key = member.key;
    slabjson::Value value = member.value;
}

for (auto value : array) {
    // value is a slabjson::Value handle.
}
```

## Error handling

SlabJson uses `Result<T>` instead of exceptions.

```cpp
auto result = slabjson::parse(slab, payload);
if (!result) {
    slabjson::Error error = result.error();
    return;
}

auto value = result.value();
```

Errors are compact and allocation-free:

```cpp
struct Error {
    ErrorCode code;
    std::size_t offset;
};
```

`offset` is primarily used for parse and output errors.

## cJSON compatibility helpers

SlabJson includes a cJSON-style namespace for migration-oriented code:

```cpp
#include <array>
#include <slabjson/cjson_compat.hpp>
#include <slabjson/static_slab.hpp>

int main()
{
    slabjson::StaticSlab<4096> slab;

    auto root_result = slabjson::cjson::create_object(slab);
    auto name_result = slabjson::cjson::create_string(slab, "widget-123");
    if (!root_result || !name_result) {
        return 1;
    }

    auto root = root_result.value();
    auto name = name_result.value();
    if (!slabjson::cjson::add_item_to_object(root, "device_id", name)) {
        return 1;
    }

    std::array<char, 512> output{};
    auto written = slabjson::cjson::print_unformatted(root, output);
    if (!written) {
        return 1;
    }

    return 0;
}
```

Important difference from cJSON: SlabJson allocation is reclaimed at the slab level. Use:

```cpp
slabjson::cjson::delete_all(slab);
```

or:

```cpp
slab.reset();
```

to invalidate and reclaim all values in the slab.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Build and run tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSLABJSON_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The repository also provides `debug`, `release`, `sanitizer`, and
`no-exceptions` configure/build/test presets. For example:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Tests default to `ON` for a top-level SlabJson build and `OFF` when SlabJson is
included by another CMake project.

## Consuming with CMake

SlabJson is always a static library. Link the namespaced target in every
consumption mode:

```cmake
target_link_libraries(my_app PRIVATE slabjson::slabjson)
```

### As a subdirectory

```cmake
add_subdirectory(path/to/slabjson)
target_link_libraries(my_app PRIVATE slabjson::slabjson)
```

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(slabjson
    GIT_REPOSITORY https://github.com/stevenoonan/slabjson.git
    GIT_TAG v0.9.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(slabjson)
target_link_libraries(my_app PRIVATE slabjson::slabjson)
```

### As an installed package

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/install/prefix
```

```cmake
find_package(slabjson 0.9 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE slabjson::slabjson)
```

Build with link-time optimization:

```bash
cmake -S . -B build-lto \
  -DCMAKE_BUILD_TYPE=Release \
  -DSLABJSON_ENABLE_LTO=ON \
  -DSLABJSON_BUILD_TESTS=ON

cmake --build build-lto
ctest --test-dir build-lto --output-on-failure
```

Build benchmarks:

```bash
cmake -S . -B build-bench \
  -DCMAKE_BUILD_TYPE=Release \
  -DSLABJSON_BUILD_BENCHMARKS=ON \
  -DSLABJSON_ENABLE_LTO=ON

cmake --build build-bench --target slabjson_benchmark_report
```

Show the timing report:

```bash
python3 benchmarks/show_timing_report.py
```

## CMake options

| Option                       | Default | Description                                       |
| ---------------------------- | ------: | ------------------------------------------------- |
| `SLABJSON_BUILD_TESTS`       | Top-level only | Build unit and consumer tests                |
| `SLABJSON_BUILD_BENCHMARKS`  |   `OFF` | Build host benchmark suite                        |
| `SLABJSON_ENABLE_EXCEPTIONS` |   `OFF` | Allow exceptions; core code does not require them |
| `SLABJSON_ENABLE_LTO`        |   `OFF` | Enable CMake IPO/LTO when supported               |

## Performance notes

The included benchmark suite compares SlabJson against cJSON using generated subsets of Twitter, CITM, and Canada-style JSON datasets.

In a Release build with LTO using GCC 13.3 on an AMD Ryzen 9 7900X, with five repetitions per test, SlabJson was faster in all 81 paired comparisons. Its geometric-mean speedup was **2.51x**, equivalent to using about **60% less CPU time**. Summing CPU time across all cases gives a 3.73x speedup, although that figure is more heavily influenced by the larger Canada-style workloads.

| Operation                   | Geometric-mean speedup | CPU-time reduction |
| --------------------------- | ---------------------: | -----------------: |
| Parse lifecycle             |                  1.96x |                49% |
| Transactional serialization |                  2.43x |                59% |
| Partial serialization       |                  3.85x |                74% |
| Compact round trip          |                  2.35x |                57% |
| Recursive duplication       |                  2.12x |                53% |

Results varied with corpus shape:

| Corpus | Geometric-mean speedup |
| ------ | ---------------------: |
| Twitter |                  1.69x |
| CITM    |                  1.94x |
| Canada  |                  4.81x |

Individual results ranged from 1.15x faster for parsing the small, pretty Twitter fixture to 11.53x faster for partial serialization of the largest compact Canada fixture. The standalone SlabJson serialized-size measurements are not included in these comparisons because they have no paired cJSON benchmark.

Benchmark results will vary by compiler, standard library, CPU, optimization level, and corpus shape. CPU scaling and ASLR were enabled during this run, so small run-to-run variations are expected.

## Compatibility and support

SlabJson 0.9 is a pre-1.0 release. Public headers and the
`slabjson::slabjson` CMake target may still change before 1.0. Patch releases
within 0.9.x maintain source and package compatibility; a new 0.x minor release
may contain breaking changes.

The library requires C++20 and CMake 3.20. CI records the host compilers tested
on Linux, Windows, and macOS; specific compiler versions are not contractual
minimums. Embedded projects should validate their target toolchain and limits.

Before using it in production firmware, consider adding project-specific tests for:

* maximum expected JSON size
* worst-case nesting depth
* escaped and non-ASCII strings
* integer boundary values
* output capacity failures
* parse failure rollback behavior
* target-specific compiler flags

## Project layout

```text
include/slabjson/       Public headers
src/                    Core implementation
tests/                  Unit tests
benchmarks/             Host benchmark and conformance tools
```

Use the umbrella include for most application code:

```cpp
#include <slabjson/slabjson.hpp>
```

## License

SlabJson is available under the [MIT License](LICENSE).
