# Milestone 4.5: Native API Hardening

## Scope completed

This milestone hardens the native API before cJSON migration helpers:

* Exact signed integer, unsigned integer, and floating-point storage
* Unambiguous integral overloads
* Typed numeric inspection
* UTF-8 invariants for manually supplied strings and keys
* Zero-allocation object and array forward iterators
* Targeted mutation and validation errors
* Unambiguous stale-handle types
* Copy and move assignment for `Result<T>`
* A published parser depth hard cap

## Exact numbers

Number nodes now carry a `NumberKind` and one of:

```cpp
std::int64_t
std::uint64_t
double
```

All standard signed and unsigned integral types are accepted without overload
ambiguity. `bool` remains a boolean rather than an integer.

Integer JSON tokens are parsed exactly. Nonnegative values use `int64_t` when
they fit and `uint64_t` otherwise. Fraction and exponent syntax uses `double`.
Serialization uses the matching `std::to_chars` overload, preserving values
such as `9007199254740993` and `UINT64_MAX`.

`Value` adds:

```cpp
number_kind()
as_int64()
as_uint64()
```

`as_number()` remains available and converts to `double`, which may round large
exact integers.

NaN and infinity are rejected at creation with `NonFiniteNumber`, before slab
capacity is consumed.

## String validity

`Slab::make_string()` and object-key insertion validate UTF-8 before allocation.
Malformed input returns `InvalidUtf8` with the first invalid byte in
`Error::offset`.

Embedded NUL and other control bytes remain valid string data and are escaped by
the serializer. The serializer also validates stored strings defensively before
emitting bytes.

Parser string behavior remains parse-specific: malformed raw UTF-8 returns
`ParseInvalidString`.

## Iteration

`Array` and `Object` expose C++20 forward iterators:

```cpp
for (slabjson::Value value : array) {
    // ...
}

for (slabjson::ObjectMember member : object) {
    // member.key
    // member.value
}
```

Iteration follows insertion order and includes duplicate object keys.
Iterators are lightweight slab/node handles and allocate no memory.

Structural mutation invalidates iterators for the mutated container.
`Slab::reset()` invalidates all handles and iterators.

## Errors and validity

The following targeted codes replace ambiguous `InvalidArgument` results:

* `NotFound`
* `AlreadyAttached`
* `CrossSlab`
* `CycleDetected`
* `InvalidUtf8`
* `NonFiniteNumber`

`ValueType::Invalid` is returned by `type()` for stale handles. Type predicates
and conversions continue to return false or empty optionals.

## Result assignment

`Result<T>` now supports copy and move assignment across value/value,
error/error, value/error, error/value, and self-assignment states. The
implementation remains allocation-free.

## Parser depth

`kMaxParserDepth` is publicly defined as 32. `ParseOptions` defaults to this
value. A larger requested depth returns `InvalidArgument` before parsing or
allocating.

Recursive descent remains bounded by this hard cap.

## Verification

The hardening tests cover:

* Every standard signed and unsigned integral width
* `INT64_MIN`, `INT64_MAX`, `UINT64_MAX`, and integers above `2^53`
* Exact parse and serialize round trips
* Integer overflow
* Non-finite floating-point rejection and rollback
* Invalid UTF-8 values and keys with exact offsets
* Valid UTF-8 and embedded NUL serialization
* C++20 forward-iterator concepts
* Array and object insertion-order traversal
* Duplicate keys and nested values
* Every new targeted error code
* Stale handle and iterator behavior after reset
* Every `Result<T>` assignment state transition
* Parser hard-cap rejection and rollback

All seven test executables pass with:

* CMake debug and release builds
* CTest
* GCC 13.3 with C++20, strict warnings, exceptions disabled, and RTTI disabled
* AddressSanitizer
* UndefinedBehaviorSanitizer
