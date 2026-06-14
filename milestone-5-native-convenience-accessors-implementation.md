# Milestone 5: Native Convenience Accessors

## Scope completed

This milestone adds typed object-member accessors to the native API:

```cpp
Result<std::string_view> get_string(std::string_view key) const;
Result<bool> get_bool(std::string_view key) const;
Result<std::int64_t> get_int64(std::string_view key) const;
Result<std::uint64_t> get_uint64(std::string_view key) const;
Result<double> get_number(std::string_view key) const;
Result<Object> get_object(std::string_view key) const;
Result<Array> get_array(std::string_view key) const;
```

These functions are native conveniences rather than cJSON compatibility
helpers, so they are available through the normal `Object` API and
`slabjson.hpp`.

## Error behavior

Every accessor returns `Result<T>` with consistent errors:

* `InvalidHandle` when the `Object` is stale or otherwise invalid
* `NotFound` when no member has the requested key
* `TypeMismatch` when the member exists but has the wrong JSON or numeric type

The accessors do not allocate or mutate the slab.

## Numeric behavior

`get_int64()` accepts only values stored as `NumberKind::SignedInteger`.

`get_uint64()` accepts only values stored as
`NumberKind::UnsignedInteger`.

`get_number()` accepts every number kind and converts it to `double`, matching
`Value::as_number()`. Large exact integers may be rounded by this conversion.

This keeps exact access explicit and avoids an ambiguous `get_int()` API.

## Lookup behavior

Accessors use the same lookup policy as `Object::find()`. When duplicate keys
exist, the first matching member is returned.

No `operator[]` was added because it could not distinguish a missing key, an
invalid handle, and an invalid container without introducing surprising
semantics.

## Compatibility boundary

The implementation plan now separates:

* Milestone 5: native typed accessors
* Milestone 6: opt-in cJSON compatibility through `cjson_compat.hpp`

The compatibility header will not be included by the default umbrella header.

## Verification

Object tests cover:

* Successful string, boolean, signed integer, unsigned integer, floating-point,
  object, and array access
* Numeric conversion through `get_number()`
* Exact signed and unsigned kind enforcement
* Missing members
* Wrong types for every accessor
* First-match duplicate-key behavior
* Invalid handles after `Slab::reset()`

All seven test executables pass in:

* Debug and release CMake builds
* GCC 13.3 with `-Wall -Wextra -Wpedantic -Werror`
* Builds with exceptions and RTTI disabled
* AddressSanitizer and UndefinedBehaviorSanitizer
