# Milestone 2 Implementation

## Scope completed

Milestone 2 adds manual object and array construction:

* `Object` and `Array` lightweight handles
* `Slab::make_object()` and `Slab::make_array()`
* Object and array conversion through `Value`
* Singly linked child storage
* Primitive convenience overloads
* Nested object and array creation
* Object key lookup and containment checks
* Array index lookup
* Logical object member removal
* CMake and unit test targets for objects and arrays

Parsing and serialization remain deferred to later milestones.

## Public API

Objects can be built directly:

```cpp
slabjson::StaticSlab<4096> slab;

auto root_result = slab.make_object();
if (!root_result) {
    return;
}

auto root = root_result.value();
root.add("device_id", "hub-123");
root.add("battery_mv", 4120);
root.add("connected", true);
root.add_null("last_error");

auto metadata = root.add_object("metadata");
auto tags = root.add_array("tags");
```

Arrays support the corresponding append operations:

```cpp
auto array_result = slab.make_array();
if (!array_result) {
    return;
}

auto array = array_result.value();
array.add("first");
array.add(2);
array.add(true);
array.add_null();
array.add_object();
array.add_array();
```

Integer overloads were initially stored as `double`. The native API hardening
milestone supersedes this behavior with exact signed and unsigned storage.

## Handles and conversions

`Object` and `Array` contain a slab pointer, node ID, and slab generation. They
do not own memory and become invalid after `Slab::reset()`.

Both handles provide:

* `valid()`
* `size()`
* `empty()`
* `value()`
* Implicit conversion to `Value`

`Value::as_object()` and `Value::as_array()` return `std::optional` handles when
the value has the requested type and remains valid.

## Child representation

Object members and array elements use the node fields introduced in Milestone 1:

```text
parent
first_child
last_child
next_sibling
```

Appending is constant time through `last_child`. Object lookup, object size,
array indexing, and array size are linear in the number of children.

Object keys are copied into slab string storage. Array children have no key.

## Object behavior

`Object::find()` returns the first member whose key matches. Duplicate keys are
allowed, as specified by the implementation plan.

`Object::remove()` removes the first matching member from the linked list. The
node and its stored data are not reclaimed until `Slab::reset()`. The detached
value remains valid and may be attached to another container in the same slab.

Removing a missing key returns `ErrorCode::InvalidArgument`.

## Attachment rules

Adding an existing `Value` requires:

* A valid value
* The same slab and generation as the destination
* A value that does not already have a parent
* An attachment that does not create a self or ancestor cycle

Violations return `InvalidHandle` or `InvalidArgument`. These rules keep the
node structure a tree and prevent one node from appearing in multiple child
lists.

## Transactional convenience operations

Convenience operations such as:

```cpp
object.add("key", "value");
array.add(42);
object.add_array("items");
```

use an internal slab checkpoint. If value allocation succeeds but key storage
or attachment fails, node and string counters roll back to their prior values.
Failed convenience operations therefore do not consume slab capacity.

Adding an existing value stores the object key before linking the child. A key
capacity failure leaves both the value and destination unchanged.

String copying now uses overlap-safe movement so a string view that already
refers to slab storage can be copied into another slab string allocation.

## Files added

```text
include/slabjson/
  array.hpp
  containers.hpp
  object.hpp
src/
  array.cpp
  object.cpp
tests/
  test_array.cpp
  test_object.cpp
```

The existing slab, value, umbrella header, CMake files, and Milestone 1 tests
were updated for container support.

## Verification

The tests cover:

* Empty object and array construction
* Primitive object members and array elements
* `Value` container type checks and conversions
* Object lookup, containment, duplicate keys, and missing keys
* Array indexing and out-of-range lookup
* Nested objects and arrays
* Logical removal and re-attachment
* Same-slab and single-parent enforcement
* Self-cycle and ancestor-cycle rejection
* Cross-slab rejection
* Null C-string rejection
* Stale handles after reset
* Transactional capacity failure

All four test executables pass with GCC 13.3 under C++20, strict warnings,
disabled exceptions, and disabled RTTI. They also pass with AddressSanitizer
and UndefinedBehaviorSanitizer.

The project also configures and builds successfully with CMake 3.28.3. CTest
runs all four test targets successfully with no failures.
