# Milestone 3 Implementation

## Scope completed

Milestone 3 adds compact JSON serialization:

* `serialize(Value, std::span<char>)`
* `serialized_size(Value)`
* Primitive serialization
* Object and array serialization
* Arbitrarily nested tree traversal within slab capacity
* JSON string and object-key escaping
* Locale-independent number formatting
* Explicit output-capacity errors
* Serializer CMake and unit test target

Pretty serialization remains deferred.

## Public API

The serializer is available through either:

```cpp
#include <slabjson/serialize.hpp>
```

or the umbrella header:

```cpp
#include <slabjson/slabjson.hpp>
```

Example:

```cpp
slabjson::StaticSlab<4096> slab;

auto root_result = slab.make_object();
if (!root_result) {
    return;
}

auto root = root_result.value();
root.add("device_id", "hub-123");
root.add("battery_mv", 4120);

std::array<char, 128> output{};
auto result = slabjson::serialize(root, output);
if (!result) {
    return;
}

std::string_view json{output.data(), result.value()};
```

The returned size excludes a NUL terminator. The serializer does not append one.

`Object` and `Array` work directly because both convert to `Value`.

## Output sizing and failure behavior

`serialized_size()` computes the exact compact JSON size without producing
output:

```cpp
auto required = slabjson::serialized_size(root);
```

`serialize()` performs the same size pass before writing. If the output span is
too small, it returns `ErrorCode::OutputCapacityExceeded` and does not modify
the output buffer.

An exactly sized output span succeeds.

Output storage must not overlap the slab's usable storage region. Overlap
returns `ErrorCode::InvalidArgument`, preventing serialization from overwriting
nodes or strings that are still being read.

## Traversal

Serialization uses an iterative depth-first traversal over the existing:

```text
parent
first_child
next_sibling
```

links. It does not recurse and does not allocate a traversal stack. Deeply
nested documents therefore do not consume call-stack space proportional to
their depth.

Objects preserve insertion order, including duplicate keys. Logically removed
members are not serialized. Serializing an attached child value emits only that
value's subtree, not its siblings or parent.

## String escaping

String values and object keys are quoted and escaped identically.

The serializer emits short escapes for:

* `"`
* `\`
* backspace
* form feed
* newline
* carriage return
* tab

Other bytes below `0x20` are emitted as lowercase `\u00xx` escapes.

The forward slash is emitted without escaping. Non-control bytes, including
UTF-8 bytes, are copied unchanged.

## Number formatting

Finite `double` values are formatted with `std::to_chars` using general format.
This is locale-independent and produces a compact round-trippable
representation.

JSON has no representation for NaN or positive or negative infinity.
Attempting to size or serialize a non-finite number returns
`ErrorCode::InvalidArgument`.

## Error behavior

The serializer returns:

* `InvalidHandle` for stale or otherwise invalid root values
* `InvalidArgument` for non-finite numbers or overlapping output storage
* `OutputCapacityExceeded` when the output span is too small
* `InternalError` if inconsistent internal tree links are encountered

No heap allocation or exceptions are used.

## Files added

```text
include/slabjson/
  serialize.hpp
src/
  serialize.cpp
tests/
  test_serialize.cpp
```

The umbrella header, root CMake target, and test CMake target list were updated.

## Verification

Serializer tests cover:

* Null, boolean, number, and string primitives
* Empty and populated objects and arrays
* Nested object and array structures
* Object insertion order
* Duplicate keys
* Removed members
* Serialization of an attached subtree
* Required string and key escapes
* Embedded NUL and other control bytes
* UTF-8 byte passthrough
* Exact-size output spans
* Untouched undersized output buffers
* Empty output spans
* Non-finite number rejection
* Overlapping slab/output rejection
* Stale handle rejection
* Deep stackless traversal

All five test executables pass with:

* CMake debug and release builds
* CTest
* GCC 13.3 with C++20, strict warnings, exceptions disabled, and RTTI disabled
* AddressSanitizer
* UndefinedBehaviorSanitizer
