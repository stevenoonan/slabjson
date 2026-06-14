# Milestone 4 Implementation

## Scope completed

Milestone 4 adds JSON parsing into a caller-owned slab:

* `parse(Slab&, std::string_view, ParseOptions)`
* Objects and arrays
* Strings and object keys
* Numbers
* `true`, `false`, and `null`
* JSON whitespace handling
* Escape and Unicode decoding
* Configurable container-depth limits
* Parse error offsets
* Transactional rollback on every parse failure
* Parser CMake and unit test target

## Public API

The parser is available through:

```cpp
#include <slabjson/parse.hpp>
```

or:

```cpp
#include <slabjson/slabjson.hpp>
```

Example:

```cpp
slabjson::StaticSlab<4096> slab;

auto result = slabjson::parse(
    slab,
    R"({"device_id":"hub-123","connected":true})");

if (!result) {
    const slabjson::Error error = result.error();
    // error.code identifies the failure.
    // error.offset identifies the input byte offset.
    return;
}

auto root = result.value().as_object();
```

Options:

```cpp
struct ParseOptions {
    std::uint16_t max_depth = 32;
    bool allow_trailing_whitespace = true;
};
```

Leading JSON whitespace is always accepted. With
`allow_trailing_whitespace == true`, JSON whitespace after the root value is
also accepted. When false, any byte after the root value, including whitespace,
returns `ParseTrailingCharacters`.

## Grammar

The parser accepts:

```text
value  = object | array | string | number | "true" | "false" | "null"
object = "{" [ member *( "," member ) ] "}"
member = string ":" value
array  = "[" [ value *( "," value ) ] "]"
```

Trailing commas, unquoted object keys, missing values, missing separators, and
extra bytes after the root value are rejected.

Duplicate object keys are retained in input order. Existing object behavior
applies, so `find()` returns the first matching key.

## Number parsing

Number syntax supports:

* Optional leading minus
* Zero or a nonzero integer part
* Optional fraction with at least one digit
* Optional exponent with an optional sign and at least one digit

Leading zeros, incomplete fractions, and incomplete exponents return
`ParseInvalidNumber`.

Numbers are converted directly to `double` with `std::from_chars`. This avoids
locale-dependent parsing and does not require a temporary NUL-terminated
buffer. Values outside the finite `double` range are rejected.

## String and Unicode handling

Strings and object keys support:

* `\"`
* `\\`
* `\/`
* `\b`
* `\f`
* `\n`
* `\r`
* `\t`
* `\uXXXX`

Unicode escapes are encoded into UTF-8. Valid UTF-16 surrogate pairs are
combined and encoded as one Unicode code point. Lone high surrogates, lone low
surrogates, malformed pairs, and invalid hexadecimal digits return
`ParseInvalidUnicodeEscape`.

Unescaped non-ASCII input is validated as UTF-8 before being copied. Invalid,
overlong, surrogate, truncated, or out-of-range UTF-8 sequences return
`ParseInvalidString`. Unescaped control bytes below `0x20` are also rejected.

String decoding uses two passes:

1. Validate the source and calculate the exact decoded byte length.
2. Reserve that many bytes from the slab and decode directly into them.

No heap allocation or variable-size stack buffer is needed. Parsed strings are
owned by the slab, so the input buffer does not need to remain alive after a
successful parse.

## Depth limits

`max_depth` limits nested object and array containers. A root container has
depth one:

* `max_depth == 1` accepts `[]` and `{}`.
* `max_depth == 1` rejects `[[]]`.
* Primitive roots are accepted even when `max_depth == 0`.

Exceeding the limit returns `ParserDepthExceeded` at the opening `[` or `{`.

The current implementation uses recursive descent. The explicit default limit
of 32 bounds parser call-stack use. Applications should not set a depth limit
larger than their target stack can safely support.

## Transactional failure behavior

Before parsing, the parser records the slab's node and string allocation
counters. Any failure restores both counters.

This means:

* Failed parses consume no slab capacity.
* Values that existed before the parse remain valid.
* A failed parse does not require `slab.reset()`.
* A later parse can reuse all memory reserved during the failed attempt.

Memory bytes written during a failed parse are not cleared, but they are outside
the slab's active node and string regions after rollback.

## Error offsets

`Error::offset` is a zero-based byte offset into the original input.

Examples:

* Invalid escape: offset of the invalid escape character.
* Invalid Unicode: offset of the malformed escape or hexadecimal digit.
* Invalid number: offset of the invalid number component.
* Unexpected end: `input.size()`.
* Depth failure: offset of the rejected opening delimiter.
* Trailing characters: offset of the first unconsumed byte.

Capacity errors use the offset of the value or string whose allocation failed.

## Input storage restriction

The input range must not overlap the slab's usable storage range. Overlap
returns `InvalidArgument`.

This prevents front-growing nodes or back-growing decoded strings from
overwriting unread parser input. Non-overlapping input is copied into the slab
as normal.

## Capacity errors

Parser allocation failures preserve the underlying slab error:

* `OutOfMemory` when another node does not fit
* `NodeCapacityExceeded` when the node ID range is exhausted
* `StringCapacityExceeded` when decoded string bytes do not fit

All capacity failures roll back the complete parse.

## Files added

```text
include/slabjson/
  parse.hpp
src/
  parse.cpp
tests/
  test_parse.cpp
```

The slab internals gained a private exact-length string reservation helper. The
umbrella header and CMake targets were updated for parser support.

## Verification

Parser tests cover:

* Every JSON value type
* Empty and populated objects and arrays
* Nested structures
* Whitespace
* Integer, fraction, and exponent numbers
* Duplicate object keys
* Every supported short escape
* BMP Unicode escapes
* Surrogate pairs
* Raw UTF-8
* Copied input lifetime
* Parse and serialize round trips
* Malformed structures and trailing commas
* Unterminated strings
* Invalid escapes and Unicode
* Invalid and out-of-range numbers
* Invalid UTF-8
* Exact error offsets
* Trailing-whitespace options
* Depth boundaries
* Node and string capacity failures
* Rollback with preexisting slab values
* Overlapping input rejection

All six test executables pass with:

* CMake debug and release builds
* CTest
* GCC 13.3 with C++20, strict warnings, exceptions disabled, and RTTI disabled
* AddressSanitizer
* UndefinedBehaviorSanitizer
