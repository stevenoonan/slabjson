# Milestone 1 Implementation

## Scope completed

Milestone 1 implements the core fixed-capacity slab and primitive JSON values:

* `ErrorCode` and `Error`
* `Result<T>` and `Result<void>`
* `Slab` over caller-provided memory
* `StaticSlab<Bytes>` with owned inline storage
* Lightweight `Value` handles
* Null, boolean, number, and copied string allocation
* CMake library and test targets
* Unit tests for slab behavior and primitive values

Object, array, parser, and serializer support remain intentionally deferred to
later milestones.

## Public API

The convenience header is:

```cpp
#include <slabjson/slabjson.hpp>
```

Primitive values can be created as follows:

```cpp
slabjson::StaticSlab<1024> slab;

auto null_value = slab.make_null();
auto bool_value = slab.make_bool(true);
auto number_value = slab.make_number(12.5);
auto string_value = slab.make_string("device-1");
```

Every allocation returns `Result<Value>`. A failed allocation contains a compact
`Error` with an `ErrorCode` and offset. No exception is thrown.

`Value` exposes:

* `valid()`
* `type()`
* `is_null()`, `is_bool()`, `is_number()`, and `is_string()`
* Reserved `is_array()` and `is_object()` predicates for later milestones
* `as_bool()`, `as_number()`, and `as_string()`

Conversions return `std::optional` and return an empty optional on a type
mismatch or invalid handle.

## Memory model

Nodes are placement-constructed from the front of the slab. Copied string bytes
are stored from the back. The two regions grow toward each other.

Strings are stored by length and are not NUL-terminated. `as_string()` returns a
`std::string_view` into slab-owned memory, so the source string does not need to
remain alive.

Caller-provided storage is advanced internally when needed to satisfy node
alignment. `capacity_bytes()` reports the usable capacity after that adjustment.
`StaticSlab` aligns its owned storage to `std::max_align_t`.

The initial representation uses 16-bit node IDs and string offsets. Therefore,
a slab uses at most 65,535 bytes even when a larger external span is supplied.
A wider-index configuration is deferred to a future milestone.

## Lifetime and reset behavior

`Value` is a non-owning handle containing a slab pointer, node ID, and slab
generation. Calling `Slab::reset()`:

* Resets used bytes to zero
* Makes all previous handles invalid
* Allows the full usable capacity to be reused

The generation prevents a stale handle from accidentally referring to a new
node that reuses the same node ID after reset.

Node and string memory is monotonic between resets. Individual primitive values
cannot be freed.

## Capacity errors

The implemented allocation paths report:

* `InvalidArgument` when the slab has no usable aligned storage
* `OutOfMemory` when another node does not fit
* `NodeCapacityExceeded` when the 16-bit node ID space is exhausted
* `StringCapacityExceeded` when a string is too large or collides with nodes

String creation preflights both node and string capacity. A failed string
allocation does not consume slab memory.

## Files added

```text
CMakeLists.txt
include/slabjson/
  error.hpp
  result.hpp
  slab.hpp
  slabjson.hpp
  static_slab.hpp
  value.hpp
src/
  slab.cpp
  value.cpp
tests/
  CMakeLists.txt
  test_slab.cpp
  test_value.cpp
```

## Verification

The tests cover:

* Static and caller-provided slab construction
* Alignment adjustment
* Used, remaining, and capacity byte accounting
* Reset and capacity reuse
* Invalid and exhausted slab failures
* Primitive creation, type predicates, and conversions
* Type mismatch behavior
* String copying and empty strings
* Stale handle invalidation after reset
* Transactional string capacity failure

Both test executables pass with GCC 13.3 using C++20, strict warnings, disabled
exceptions, and disabled RTTI. They also pass with AddressSanitizer and
UndefinedBehaviorSanitizer.

The CMake project configures and builds successfully with CMake 3.28.3. The
Milestone 1 tests also pass through CTest as part of the complete test suite.
