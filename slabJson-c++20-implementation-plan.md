# SlabJson C++20 Implementation Plan

## Goal

Build a C++20 JSON library for memory-constrained targets such as MCUs. The library should eventually be capable of replacing cJSON-style usage, but with deterministic memory behavior, no manual `malloc`/`free` lifecycle, no owning raw pointers in the public API, and no heap requirement in the core implementation.

The working project/library name is:

```cpp
slabjson
```

Core public types should use PascalCase. Member functions and free functions should use snake_case.

Example style:

```cpp
slabjson::StaticSlab<4096> slab;

auto root_result = slab.make_object();
if (!root_result) {
    return;
}

auto root = root_result.value();
root.add("device_id", "widget-123");
root.add("battery_mv", 4120);
```

## Baseline requirements

Use C++20 as the baseline.

The core library must not require:

* Heap allocation
* Exceptions
* RTTI
* `iostream`
* Manual caller-managed free/delete of JSON nodes
* Owning raw pointers in public APIs

Allowed:

* `std::span`
* `std::string_view`
* `std::array`
* `std::byte`
* `std::optional` if useful
* Internal raw pointers only when non-owning and tightly encapsulated
* Fixed-capacity internal pools
* Error-returning APIs

Avoid depending on C++23 or C++26.

## Primary design model

The library is built around a fixed-capacity memory owner/view called a `Slab`.

The slab owns or refers to a contiguous region of memory. JSON values, object members, array links, copied strings, and parser scratch storage are allocated from this slab. Allocation is monotonic or pool-based inside the slab, but never uses `new`, `delete`, `malloc`, or `free` in the core MCU path.

There should be two initial slab types:

```cpp
namespace slabjson {

class Slab;

template <size_t Bytes>
class StaticSlab;

} // namespace slabjson
```

`StaticSlab<N>` owns an internal `std::array<std::byte, N>`.

`Slab` uses caller-provided memory:

```cpp
std::array<std::byte, 4096> storage;
slabjson::Slab slab{std::span<std::byte>{storage}};
```

Later, an optional host-only dynamic slab can be added, but do not implement it in the first pass.

## Public type model

Initial public types:

```cpp
namespace slabjson {

class Slab;

template <size_t Bytes>
class StaticSlab;

class Value;
class Object;
class Array;

struct Error;
enum class ErrorCode;

template <typename T>
class Result;

} // namespace slabjson
```

`Value`, `Object`, and `Array` are lightweight handles. They should not own memory directly. They should contain something equivalent to:

```cpp
Slab* slab_;
ValueId id_;
```

where `ValueId` is an internal index into a node table or node region.

Do not expose raw node pointers publicly.

The user should never delete a `Value`, `Object`, or `Array`. A value lives as long as the slab lives or until `slab.reset()` is called.

## Public API sketch

### Slab construction

```cpp
std::array<std::byte, 4096> storage;
slabjson::Slab slab{std::span<std::byte>{storage}};

slabjson::StaticSlab<4096> static_slab;
```

### Slab lifecycle

```cpp
slab.reset();

size_t used = slab.used_bytes();
size_t capacity = slab.capacity_bytes();
size_t remaining = slab.remaining_bytes();
```

### Value creation

Prefer `Result<T>` for any operation that can fail due to capacity.

```cpp
Result<Value> make_null();
Result<Value> make_bool(bool value);
Result<Value> make_number(double value);
Result<Value> make_string(std::string_view value);
Result<Object> make_object();
Result<Array> make_array();
```

These should be member functions on `Slab`:

```cpp
auto object_result = slab.make_object();
```

### Object API

```cpp
class Object {
public:
    Result<void> add(std::string_view key, Value value);
    Result<void> add(std::string_view key, std::string_view value);
    Result<void> add(std::string_view key, const char* value);
    Result<void> add(std::string_view key, bool value);
    Result<void> add(std::string_view key, int value);
    Result<void> add(std::string_view key, int64_t value);
    Result<void> add(std::string_view key, double value);
    Result<void> add_null(std::string_view key);

    Result<Object> add_object(std::string_view key);
    Result<Array> add_array(std::string_view key);

    std::optional<Value> find(std::string_view key) const;
    bool contains(std::string_view key) const;

    Result<std::string_view> get_string(std::string_view key) const;
    Result<bool> get_bool(std::string_view key) const;
    Result<int64_t> get_int64(std::string_view key) const;
    Result<uint64_t> get_uint64(std::string_view key) const;
    Result<double> get_number(std::string_view key) const;
    Result<Object> get_object(std::string_view key) const;
    Result<Array> get_array(std::string_view key) const;

    Result<void> remove(std::string_view key);

    size_t size() const;
    bool empty() const;
};
```

For duplicate keys, choose one behavior and document it. For the initial implementation, allow duplicate keys internally but make `find()` return the first matching key. Later, an option can be added to replace existing keys.

The typed `get_*()` accessors also use the first matching key. They return
`InvalidHandle` for a stale object, `NotFound` for an absent key, and
`TypeMismatch` when the value has the wrong type. `get_int64()` and
`get_uint64()` require the matching exact integer kind. `get_number()` converts
any numeric kind to `double` and may round large exact integers.

### Array API

```cpp
class Array {
public:
    Result<void> add(Value value);
    Result<void> add(std::string_view value);
    Result<void> add(const char* value);
    Result<void> add(bool value);
    Result<void> add(int value);
    Result<void> add(int64_t value);
    Result<void> add(double value);
    Result<void> add_null();

    Result<Object> add_object();
    Result<Array> add_array();

    std::optional<Value> at(size_t index) const;

    size_t size() const;
    bool empty() const;
};
```

### Value API

```cpp
enum class ValueType : uint8_t {
    Invalid,
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

enum class NumberKind : uint8_t {
    SignedInteger,
    UnsignedInteger,
    FloatingPoint
};

class Value {
public:
    ValueType type() const;

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    std::optional<bool> as_bool() const;
    std::optional<NumberKind> number_kind() const;
    std::optional<int64_t> as_int64() const;
    std::optional<uint64_t> as_uint64() const;
    std::optional<double> as_number() const;
    std::optional<std::string_view> as_string() const;
    std::optional<Array> as_array() const;
    std::optional<Object> as_object() const;
};
```

Store signed integers, unsigned integers, and floating-point values distinctly.
`as_number()` converts any numeric kind to `double` and may round large exact
integers. Use the typed integer accessors when precision matters.

Objects and arrays expose zero-allocation C++20 forward iterators. Object
iteration yields a key and `Value`; array iteration yields `Value`.

## Error handling

Implement a small `Result<T>` type. Do not use exceptions.

For `Result<void>`, either specialize `Result<void>` or create a simple `Status` type. Prefer `Result<void>` if simple enough.

Error enum:

```cpp
enum class ErrorCode : uint8_t {
    Ok,
    InvalidArgument,
    InvalidHandle,
    TypeMismatch,
    NotFound,
    AlreadyAttached,
    CrossSlab,
    CycleDetected,
    InvalidUtf8,
    NonFiniteNumber,

    OutOfMemory,
    NodeCapacityExceeded,
    StringCapacityExceeded,
    MemberCapacityExceeded,
    ParserDepthExceeded,
    OutputCapacityExceeded,

    ParseUnexpectedEnd,
    ParseUnexpectedToken,
    ParseInvalidString,
    ParseInvalidNumber,
    ParseInvalidEscape,
    ParseInvalidUnicodeEscape,
    ParseTrailingCharacters,

    InternalError
};
```

Error struct:

```cpp
struct Error {
    ErrorCode code;
    size_t offset; // for parse/serialize errors where applicable
};
```

`offset` can be zero for non-parse errors.

Keep errors compact. Do not require dynamic strings in `Error`.

## Internal representation

Implement JSON values as indexed nodes. Avoid node pointers in public handles.

Suggested internal ID types:

```cpp
using NodeId = uint16_t;
using StringId = uint16_t;

constexpr NodeId kInvalidNodeId = 0xFFFF;
constexpr StringId kInvalidStringId = 0xFFFF;
```

For large slabs later, allow a compile-time option to use 32-bit indices.

Internal node shape:

```cpp
enum class NodeType : uint8_t {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct StringRef {
    uint16_t offset;
    uint16_t length;
};

struct Node {
    NodeType type;
    NumberKind number_kind;

    NodeId parent;
    NodeId first_child;
    NodeId last_child;
    NodeId next_sibling;

    StringRef key; // only meaningful when this node is a child of an object

    union Payload {
        bool bool_value;
        int64_t signed_integer;
        uint64_t unsigned_integer;
        double floating_point;
        StringRef string_value;

        Payload() {}
    } payload;
};
```

Initial simplification: children of both arrays and objects can be represented as a singly linked list using `first_child`, `last_child`, and `next_sibling`.

For an object child, the child node’s `key` field stores the object key.

For an array child, `key` is invalid.

This is not the fastest possible object lookup, but it is simple, compact, and cJSON-like. It is acceptable for initial MCU use.

## Slab memory layout

The simplest initial implementation should not try to dynamically partition arbitrary bytes into complex variable-sized regions.

Use fixed internal sub-pools inside `StaticSlab<N>` and `Slab` if that is simpler, or implement a small bump allocator over the user-provided span.

Recommended first implementation:

* Allocate `Node` objects from the front of the byte span.
* Allocate string bytes from the back of the byte span.
* They grow toward each other.
* Capacity failure occurs when the two regions collide.

Conceptually:

```text
+------------------------------+
| Node Node Node --->          |
|                              |
|          <--- string bytes   |
+------------------------------+
```

This allows a single caller-provided buffer to hold both nodes and copied strings.

The slab tracks:

```cpp
std::span<std::byte> storage_;
size_t node_bytes_used_;
size_t string_bytes_used_from_end_;
NodeId node_count_;
```

Creating a node places it at:

```cpp
storage_.data() + node_count_ * sizeof(Node)
```

Strings are copied from the end downward.

Important alignment requirement:

* Node allocation must respect `alignof(Node)`.
* String allocation only needs byte alignment.
* Ensure the node region is aligned at the start.
* If caller-provided memory is insufficiently aligned, either adjust the starting address or return an initialization failure flag.

Because `Slab` construction cannot return `Result`, provide:

```cpp
bool valid() const;
```

If alignment correction leaves no usable capacity, `valid()` returns false and all allocation attempts fail with `ErrorCode::InvalidArgument` or `OutOfMemory`.

Alternative: require caller-provided span to be sufficiently aligned and document it. But for robustness, adjusting internally is better.

## String storage policy

The first implementation should copy all strings into the slab.

This includes:

* Object keys
* String values
* Parsed JSON strings

This makes lifetime simple. The input JSON buffer does not need to outlive the parsed tree.

Later, add a zero-copy parsing option for unescaped strings.

String storage API internally:

```cpp
Result<StringRef> store_string(std::string_view value);
std::string_view view_string(StringRef ref) const;
```

Strings do not need to be NUL-terminated internally. Store length explicitly.

For compatibility helpers that need C strings later, provide optional NUL-terminated storage or temporary output into caller-provided buffers.

## Parser

Implement a recursive descent parser or iterative parser with explicit depth tracking.

For first pass, recursive descent is acceptable if maximum depth is checked and kept small. However, for MCU robustness, prefer an explicit parser depth limit.

Public parse function:

```cpp
Result<Value> parse(Slab& slab, std::string_view input);
```

Optional parse options:

```cpp
struct ParseOptions {
    uint16_t max_depth = 32;
    bool allow_trailing_whitespace = true;
};
```

Parser grammar:

```text
value  = object | array | string | number | "true" | "false" | "null"
object = "{" [ member *( "," member ) ] "}"
member = string ":" value
array  = "[" [ value *( "," value ) ] "]"
```

Required parsing features:

* Whitespace skipping
* Objects
* Arrays
* Strings
* Escapes:

  * `\"`
  * `\\`
  * `\/`
  * `\b`
  * `\f`
  * `\n`
  * `\r`
  * `\t`
  * `\uXXXX`
* Numbers:

  * optional minus
  * integer
  * optional fraction
  * optional exponent
* Literals:

  * `true`
  * `false`
  * `null`

Unicode handling for first implementation:

* Validate `\uXXXX` syntax.
* For code points in the ASCII range, emit ASCII.
* For non-ASCII code points, encode as UTF-8.
* Correctly handle surrogate pairs if feasible.
* If surrogate pairs are too much for first pass, return `ParseInvalidUnicodeEscape` for surrogate ranges and document this temporary limitation.

Do not silently output invalid UTF-8.

Parser should leave the slab unchanged on failure if practical. If rollback is hard, document that failed parses may consume slab memory and require `slab.reset()`. For first pass, it is acceptable to require reset after failed parse.

## Serializer

Public API:

```cpp
Result<size_t> serialize(Value value, std::span<char> output);
Result<size_t> serialize_partial(Value value, std::span<char> output);
Result<size_t> serialize_pretty(Value value, std::span<char> output, uint8_t indent_spaces = 2);
Result<size_t> serialize_pretty_partial(Value value, std::span<char> output, uint8_t indent_spaces = 2);
```

The return value is the number of bytes written, excluding any NUL terminator.

Do not require NUL termination.

`serialize()` and `serialize_pretty()` preserve the output on capacity failure.
The partial variants perform one pass and may leave a partial JSON document in
the output span on failure.

Optionally add:

```cpp
Result<size_t> serialized_size(Value value);
```

Serializer requirements:

* Output valid JSON
* Escape strings correctly
* Serialize objects and arrays recursively or iteratively
* Return `OutputCapacityExceeded` if output buffer is too small
* Do not partially report success

String escaping must handle:

* `"`
* `\`
* control characters below `0x20`
* `\b`
* `\f`
* `\n`
* `\r`
* `\t`

Validate strings as UTF-8 when they enter the slab. For valid non-control UTF-8
bytes, output them as-is.

Number serialization:

* Use `std::to_chars` for locale-independent formatting.
* Serialize integer payloads exactly.
* Serialize finite `double` payloads using general format.
* Reject NaN and infinity when values are created.

## Mutation and deletion

The initial implementation should support append/add and lookup.

Deletion is useful for cJSON replacement, but compact memory reclamation is hard in a slab. Implement deletion as logical unlinking:

```cpp
Result<void> Object::remove(std::string_view key);
```

Removed nodes are unlinked from parent child lists. Their memory is not reclaimed until `slab.reset()`.

This is acceptable and should be documented.

Later, add a free-list for nodes if needed.

## cJSON replacement strategy

Do not start by implementing a literal cJSON ABI clone.

First build a clean C++ API.

Then add a compatibility header:

```cpp
#include <slabjson/cjson_compat.hpp>
```

The compatibility layer should map common cJSON-style operations to SlabJson operations.

The compatibility API lives in `slabjson::cjson` and uses `Value` as its
uniform `Item` handle:

```cpp
namespace slabjson::cjson {

using Item = Value;

Result<Item> create_object(Slab& slab);
Result<Item> create_array(Slab& slab);
Result<Item> create_string(Slab& slab, std::string_view value);
Result<Item> create_number(Slab& slab, double value);

Result<void> add_item_to_object(
    Item object,
    std::string_view key,
    Item item);
Result<void> add_item_to_array(Item array, Item item);

std::optional<Item> get_object_item(
    Item object,
    std::string_view key);
std::optional<Item> get_array_item(Item array, size_t index);

Result<size_t> print_preallocated(
    Item item,
    std::span<char> output,
    bool formatted);

Result<Item> duplicate(
    Slab& destination,
    Item item,
    bool recurse = true);

} // namespace slabjson::cjson
```

Compatibility object lookup follows cJSON's ASCII case-insensitive default and
also provides an explicit case-sensitive variant. Printing requires room for a
trailing NUL, writes it on success, and returns the JSON byte count excluding
that terminator.

Target cJSON-like functionality to cover eventually:

* Create object
* Create array
* Create string
* Create number
* Create bool
* Create null
* Add item to object
* Add item to array
* Get object item
* Get array item
* Get array size
* Parse
* Print unformatted
* Print pretty
* Delete/reset lifecycle
* Duplicate/clone
* Detach/remove item

Important difference:

* Do not expose a heap-allocated `char*` print result in the core API.
* Printing writes into caller-provided output buffers.

Compatibility layer can provide helper functions that mimic cJSON behavior only if the user explicitly supplies a slab/output buffer.

The layer is source-migration support, not an ABI clone. It does not provide a
public mutable node structure, allocator hooks, raw child/next pointers,
reference nodes, raw JSON nodes, or heap-allocated print results.

## Header and source layout

Use this initial structure:

```text
include/
  slabjson/
    slab.hpp
    static_slab.hpp
    value.hpp
    object.hpp
    array.hpp
    result.hpp
    error.hpp
    parse.hpp
    serialize.hpp
    cjson_compat.hpp
    slabjson.hpp

src/
  slab.cpp
  value.cpp
  object.cpp
  array.cpp
  parse.cpp
  serialize.cpp
  cjson_compat.cpp

tests/
  test_slab.cpp
  test_value.cpp
  test_object.cpp
  test_array.cpp
  test_parse.cpp
  test_serialize.cpp
```

`slabjson.hpp` should be the convenience umbrella include:

```cpp
#pragma once

#include <slabjson/slab.hpp>
#include <slabjson/static_slab.hpp>
#include <slabjson/value.hpp>
#include <slabjson/object.hpp>
#include <slabjson/array.hpp>
#include <slabjson/parse.hpp>
#include <slabjson/serialize.hpp>
```

## Coding style

* Types/classes/enums: PascalCase
* Member functions/free functions: snake_case
* Variables: snake_case
* Private members: leading underscore 
* Constants: `kName`
* Namespace: `slabjson`
* Files: snake_case

Examples:

```cpp
class StaticSlab;
class ParseOptions;
enum class ErrorCode;

Result<Value> make_string(std::string_view value);

size_t used_bytes_;
```

Avoid names reserved by the implementation. Do not use double underscores. Do not use leading underscore followed by uppercase.

## Build system

Provide CMake support.

Minimum CMake:

```cmake
cmake_minimum_required(VERSION 3.20)

project(slabjson LANGUAGES CXX)

add_library(slabjson
    src/slab.cpp
    src/value.cpp
    src/object.cpp
    src/array.cpp
    src/parse.cpp
    src/serialize.cpp
)

target_include_directories(slabjson PUBLIC include)

target_compile_features(slabjson PUBLIC cxx_std_20)
```

Add options:

```cmake
option(SLABJSON_BUILD_TESTS "Build SlabJson tests" ON)
option(SLABJSON_ENABLE_EXCEPTIONS "Allow exceptions" OFF)
```

The first implementation should not use exceptions regardless of the option.

## Testing

Use unit tests. Catch2 is acceptable if already available in the repository.

Test categories:

### Slab tests

* Construct `StaticSlab<1024>`
* Construct `Slab` from external storage
* `used_bytes()`, `capacity_bytes()`, `remaining_bytes()`
* `reset()`
* Fail cleanly when capacity is exceeded

### Value tests

* Create null, bool, number, string
* Check type predicates
* Check conversions
* Type mismatch returns empty optional

### Object tests

* Create object
* Add string, bool, number, null
* Find existing key
* Missing key returns empty
* Nested object
* Nested array
* Remove key

### Array tests

* Create array
* Add primitive values
* Add object
* Add nested array
* Index lookup
* Out-of-range lookup

### Parse tests

Valid:

```json
null
true
false
123
-12.5
"hello"
[]
[1,2,3]
{}
{"a":1}
{"a":1,"b":true,"c":null}
{"nested":{"x":1},"array":[true,false]}
```

Invalid:

```json
{
[1,2,
{"a"}
{"a":}
"unterminated
"\q"
01
1.
```

Capacity:

* Parse a document that exceeds node capacity
* Parse a document that exceeds string capacity
* Verify clear error code

### Serialize tests

* Serialize primitives
* Serialize object
* Serialize array
* Serialize nested structures
* Escape strings correctly
* Output capacity failure

Expected serialization should be compact by default:

```json
{"device_id":"widget-123","battery_mv":4120}
```

Pretty serialization can come later if needed.

## Initial milestone plan

### Milestone 1: Core slab and primitive values

Implement:

* `ErrorCode`
* `Error`
* `Result<T>`
* `Slab`
* `StaticSlab<N>`
* `Value`
* Primitive allocation:

  * null
  * bool
  * number
  * string

Tests should prove primitive creation and access works.

### Milestone 2: Object and array construction

Implement:

* `Object`
* `Array`
* child linked-list storage
* object add/find
* array add/at
* nested object/array creation

Tests should prove manual JSON construction works.

### Milestone 3: Compact serializer

Implement:

* `serialize(Value, std::span<char>)`
* string escaping
* nested object/array serialization
* output capacity errors

Tests should prove constructed JSON serializes correctly.

### Milestone 4: Parser

Implement:

* `parse(Slab&, std::string_view)`
* objects
* arrays
* strings
* numbers
* booleans
* null
* parse error offsets
* depth limit

Tests should prove valid JSON parses and invalid JSON fails cleanly.

### Milestone 4.5: Native API hardening

Implement before compatibility helpers:

* exact signed, unsigned, and floating-point number storage
* constrained integral construction overloads
* UTF-8 validation for manually supplied strings and keys
* object and array forward iterators
* unambiguous invalid handle types
* targeted mutation and validation error codes
* assignable `Result<T>`
* a published parser depth hard cap

Tests should prove exact integer round trips, iterator behavior, UTF-8
invariants, targeted errors, and parser stack bounds.

### Milestone 5: Native convenience accessors

Add typed object-member accessors that are useful independently of cJSON:

* `get_string()`
* `get_bool()`
* `get_int64()`
* `get_uint64()`
* `get_number()`
* `get_object()`
* `get_array()`

Return `Result<T>` so invalid handles, missing keys, and type mismatches remain
distinguishable. Preserve exact integer-kind semantics and first-match behavior
for duplicate keys.

Do not add `operator[]`; its missing-key and wrong-container behavior would be
ambiguous. Do not add `get_int()` because its width and signedness are
ambiguous.

### Milestone 6: Optional cJSON compatibility layer

Add opt-in cJSON migration helpers in:

```cpp
#include <slabjson/cjson_compat.hpp>
```

Keep cJSON naming and lifecycle adaptations out of the native API and the
default `slabjson.hpp` umbrella header. Require explicit slab and output-buffer
arguments where cJSON would otherwise allocate.

Implement:

* Uniform `cjson::Item` handles
* Primitive, object, and array creation
* Object and array attachment
* cJSON-style object and array lookup
* Type predicates and string/number extraction
* Parsing into an explicit slab
* Compact and pretty preallocated printing with NUL termination
* Recursive or shallow duplication into an explicit destination slab
* Object and array detach/delete operations
* Slab-wide delete/reset lifecycle

Duplication and printing must be transactional on capacity failure. Recursive
duplication and pretty serialization must use the slab's parent/sibling links
rather than call-stack recursion.

Logical delete and detach do not reclaim slab memory. `delete_all()` maps the
cJSON tree-deletion lifecycle to `Slab::reset()` and invalidates every handle
owned by that slab.

### Milestone 7: cJSON performance and conformance suite

Add optional host-side tooling behind `SLABJSON_BUILD_BENCHMARKS=ON`. Fetch
pinned cJSON, Google Benchmark, JSONTestSuite, and nativejson-benchmark
revisions only for benchmark builds. Generate deterministic compact and
newline-formatted Twitter, CITM, and Canada subsets that fit
`StaticSlab<65535>`; classify the original corpus documents as
capacity-limited.

Benchmark parse lifecycle, compact and formatted serialization, compact round
trips, and recursive duplication after validating both implementations against
case-sensitive cJSON semantic comparison. Add JSONTestSuite conformance
reporting and a separate allocation/capacity CSV report using cJSON allocation
hooks. Include one-iteration CTest smoke coverage for every benchmark family
and a report target that writes five-repetition Google Benchmark JSON and
memory data. Performance results are informational and have no pass/fail
thresholds.

## Example target API

This should compile when the first major milestones are done:

```cpp
#include <array>
#include <cstddef>
#include <slabjson/slabjson.hpp>

void example()
{
    slabjson::StaticSlab<4096> slab;

    auto root_result = slab.make_object();
    if (!root_result) {
        return;
    }

    auto root = root_result.value();

    root.add("device_id", "widget-123");
    root.add("battery_mv", 4120);
    root.add("connected", true);

    auto tags_result = root.add_array("tags");
    if (tags_result) {
        auto tags = tags_result.value();
        tags.add("widget");
        tags.add("production");
    }

    std::array<char, 512> output{};
    auto written = slabjson::serialize(root, std::span<char>{output});

    if (!written) {
        return;
    }

    // output[0..written.value()) contains compact JSON.
}
```

Parsing example:

```cpp
#include <array>
#include <cstddef>
#include <slabjson/slabjson.hpp>

void parse_example(std::string_view payload)
{
    slabjson::StaticSlab<4096> slab;

    auto parsed = slabjson::parse(slab, payload);
    if (!parsed) {
        auto error = parsed.error();
        // Handle error.code and error.offset.
        return;
    }

    auto root_value = parsed.value();
    auto root = root_value.as_object();
    if (!root) {
        return;
    }

    auto home_id_value = root->find("home_id");
    if (!home_id_value) {
        return;
    }

    auto home_id = home_id_value->as_string();
    if (!home_id) {
        return;
    }

    // Use *home_id.
}
```

## Important implementation notes

1. Keep the core deterministic.

   * No hidden heap allocation.
   * No global allocator hooks.
   * No surprise ownership transfer.

2. Handles must be cheap to copy.

   * `Value`, `Object`, and `Array` should behave like views/handles.
   * They should not own memory.

3. Slab lifetime controls value lifetime.

   * Values become invalid after `slab.reset()` or slab destruction.
   * Document this clearly.

4. Capacity errors are normal.

   * Treat them as expected outcomes.
   * Tests should verify them.

5. Avoid overengineering the first version.

   * Do not implement hash tables for object lookup yet.
   * Do not implement node free-lists yet.
   * Do not implement zero-copy parsing yet.
   * Do not implement C++26 reflection yet.
   * Do not implement dynamic allocation yet.

6. Prioritize correctness and testability over raw speed.

   * The target is embedded reliability first.
   * Performance tuning can happen once the API and representation are stable.

## Definition of done for first usable version

The first usable version is complete when this works:

* User can create a fixed-size slab.
* User can build JSON objects and arrays manually.
* User can parse common JSON payloads into the slab.
* User can serialize JSON into a caller-provided output buffer.
* All common capacity failures return explicit errors.
* No heap allocation is required.
* No manual delete/free is required.
* Unit tests cover primitive values, objects, arrays, parsing, serialization, and capacity failure.
