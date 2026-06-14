# Milestone 6: Optional cJSON Compatibility Layer

## Scope completed

Milestone 6 adds opt-in cJSON source-migration helpers:

```cpp
#include <slabjson/cjson_compat.hpp>
```

The header is intentionally not included by `slabjson.hpp`.

The API lives in:

```cpp
namespace slabjson::cjson
```

and uses the existing non-owning handle as its uniform item type:

```cpp
using Item = Value;
```

## Creation and attachment

The layer provides:

* `create_null()`
* `create_true()` and `create_false()`
* `create_bool()`
* Exact signed, unsigned, and floating-point `create_number()` overloads
* `create_string()`
* `create_array()`
* `create_object()`
* `add_item_to_array()`
* `add_item_to_object()`

Every creation call receives an explicit `Slab&`. Attachment retains the native
same-slab, single-parent, and cycle checks.

## Lookup and inspection

The compatibility API provides:

* `get_array_item()`
* `get_array_size()`
* `get_object_item()`
* `get_object_item_case_sensitive()`
* `has_object_item()`
* JSON type predicates
* `get_string_value()`
* `get_number_value()`

Like cJSON, the default object lookup is ASCII case-insensitive. The native
`Object::find()` remains case-sensitive.

Lookup functions return `std::optional<Item>` to model cJSON's nullable lookup
result. Mutating operations continue to return `Result<T>` so specific failures
are retained.

## Parsing and printing

`cjson::parse()` forwards to the native transactional parser and requires an
explicit destination slab.

Printing is caller-buffered:

```cpp
print_unformatted(item, output);
print_pretty(item, output, 2);
print_preallocated(item, output, formatted, 2);
```

Compatibility printing requires one extra output byte, appends a NUL
terminator, and returns the JSON byte count excluding that terminator. An
undersized buffer is not modified.

Native pretty serialization was added through:

```cpp
serialized_size_pretty()
serialize_pretty()
```

It uses iterative parent/sibling traversal, performs no heap allocation, and
does not consume stack space proportional to JSON depth.

## Duplication

`duplicate(destination, item, recurse)` supports shallow and recursive copying
into an explicit destination slab.

Duplication:

* Preserves exact signed, unsigned, and floating-point number kinds
* Copies all strings and object keys
* Preserves insertion order and duplicate keys
* Uses iterative traversal rather than recursion
* Rolls the destination slab back completely on failure
* Supports copying within the same slab

A shallow object or array duplicate contains no children.

## Detach and delete

The layer provides:

* `detach_item_from_array()`
* `delete_item_from_array()`
* `detach_item_from_object()`
* `detach_item_from_object_case_sensitive()`
* `delete_item_from_object()`

Detached values remain valid and may be attached to another container in the
same slab. Delete helpers logically unlink and discard the returned handle;
they do not reclaim node or string capacity.

`delete_all(slab)` maps the cJSON whole-tree lifecycle to `Slab::reset()`. It
reclaims the slab as a unit and invalidates all handles into that slab.

## Deliberate compatibility limits

This is a source-migration layer, not a cJSON ABI clone. It does not expose:

* A mutable public node structure
* Owning raw pointers
* Allocator hooks
* Heap-allocated print strings
* Raw JSON nodes
* Reference nodes or shared children
* Direct mutation of value fields

These exclusions preserve deterministic memory use and the native tree
invariants.

## Verification

Compatibility tests cover:

* Every creation category
* Object and array attachment
* Case-insensitive and case-sensitive lookup
* Type predicates and value extraction
* Compact and pretty NUL-terminated printing
* Untouched undersized output buffers
* Parsing
* Recursive and shallow duplication
* Exact `uint64_t` duplication
* Transactional duplication rollback on out-of-memory
* Object and array detach, delete, and reattachment
* Missing-index and wrong-type errors
* Slab reset and stale handles
* Deep stackless duplication
* Deep stackless pretty printing
* Same-slab duplication

All eight test executables pass with:

* Debug and release CMake builds
* CTest
* GCC 13.3 with `-Wall -Wextra -Wpedantic -Werror`
* Exceptions and RTTI disabled
* AddressSanitizer
* UndefinedBehaviorSanitizer

The compatibility header also compiles as a standalone include.
