#pragma once

#include <slabjson/config.hpp>

#if SLABJSON_ENABLE_CJSON_COMPAT

#include <slabjson/numbers.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include <slabjson/parse.hpp>
#include <slabjson/result.hpp>
#include <slabjson/slab.hpp>
#include <slabjson/value.hpp>

namespace slabjson::cjson {

using Item = Value;

[[nodiscard]] Result<Item> create_null(Slab& slab) noexcept;
[[nodiscard]] Result<Item> create_true(Slab& slab) noexcept;
[[nodiscard]] Result<Item> create_false(Slab& slab) noexcept;
[[nodiscard]] Result<Item> create_bool(Slab& slab, bool value) noexcept;
[[nodiscard]] Result<Item> create_number(
    Slab& slab,
    int_t value) noexcept;
[[nodiscard]] Result<Item> create_number(
    Slab& slab,
    uint_t value) noexcept;
#if SLABJSON_HAS_FLOAT
[[nodiscard]] Result<Item> create_number(Slab& slab, float_t value) noexcept;
#endif

template <std::signed_integral T>
    requires (!std::same_as<std::remove_cv_t<T>, bool>
        && !std::same_as<std::remove_cv_t<T>, int_t>
            && sizeof(T) <= sizeof(int_t))
[[nodiscard]] Result<Item> create_number(Slab& slab, T value) noexcept
{
    return create_number(slab, static_cast<int_t>(value));
}

template <std::unsigned_integral T>
    requires (!std::same_as<std::remove_cv_t<T>, bool>
        && !std::same_as<std::remove_cv_t<T>, uint_t>
            && sizeof(T) <= sizeof(uint_t))
[[nodiscard]] Result<Item> create_number(Slab& slab, T value) noexcept
{
    return create_number(slab, static_cast<uint_t>(value));
}

template <detail::UnsupportedNumber T>
Result<Item> create_number(Slab&, T) noexcept = delete;

[[nodiscard]] Result<Item> create_string(
    Slab& slab,
    std::string_view value) noexcept;
[[nodiscard]] Result<Item> create_array(Slab& slab) noexcept;
[[nodiscard]] Result<Item> create_object(Slab& slab) noexcept;

[[nodiscard]] Result<void> add_item_to_array(
    Item array,
    Item item) noexcept;
[[nodiscard]] Result<void> add_item_to_object(
    Item object,
    std::string_view key,
    Item item) noexcept;

[[nodiscard]] std::optional<Item> get_array_item(
    Item array,
    std::size_t index) noexcept;
[[nodiscard]] std::size_t get_array_size(Item item) noexcept;
[[nodiscard]] std::optional<Item> get_object_item(
    Item object,
    std::string_view key) noexcept;
[[nodiscard]] std::optional<Item> get_object_item_case_sensitive(
    Item object,
    std::string_view key) noexcept;
[[nodiscard]] bool has_object_item(
    Item object,
    std::string_view key) noexcept;

[[nodiscard]] bool is_invalid(Item item) noexcept;
[[nodiscard]] bool is_false(Item item) noexcept;
[[nodiscard]] bool is_true(Item item) noexcept;
[[nodiscard]] bool is_bool(Item item) noexcept;
[[nodiscard]] bool is_null(Item item) noexcept;
[[nodiscard]] bool is_number(Item item) noexcept;
[[nodiscard]] bool is_string(Item item) noexcept;
[[nodiscard]] bool is_array(Item item) noexcept;
[[nodiscard]] bool is_object(Item item) noexcept;

[[nodiscard]] std::optional<std::string_view> get_string_value(
    Item item) noexcept;
#if SLABJSON_HAS_FLOAT
[[nodiscard]] std::optional<float_t> get_number_value(Item item) noexcept;
#endif

#if SLABJSON_ENABLE_PARSER
[[nodiscard]] Result<Item> parse(
    Slab& slab,
    std::string_view input,
    ParseOptions options = {}) noexcept;
#endif

// Printing appends a NUL terminator and returns the JSON byte count without it.
#if SLABJSON_ENABLE_SERIALIZER
[[nodiscard]] Result<std::size_t> print_unformatted(
    Item item,
    std::span<char> output) noexcept;
#endif
#if SLABJSON_ENABLE_SERIALIZER && SLABJSON_ENABLE_PRETTY
[[nodiscard]] Result<std::size_t> print_pretty(
    Item item,
    std::span<char> output,
    std::uint8_t indent_spaces = 2) noexcept;
#endif
#if SLABJSON_ENABLE_SERIALIZER
[[nodiscard]] Result<std::size_t> print_preallocated(
    Item item,
    std::span<char> output,
    bool formatted,
    std::uint8_t indent_spaces = 2) noexcept;
#endif

[[nodiscard]] Result<Item> duplicate(
    Slab& destination,
    Item item,
    bool recurse = true) noexcept;

[[nodiscard]] Result<Item> detach_item_from_array(
    Item array,
    std::size_t index) noexcept;
[[nodiscard]] Result<void> delete_item_from_array(
    Item array,
    std::size_t index) noexcept;
[[nodiscard]] Result<Item> detach_item_from_object(
    Item object,
    std::string_view key) noexcept;
[[nodiscard]] Result<Item> detach_item_from_object_case_sensitive(
    Item object,
    std::string_view key) noexcept;
[[nodiscard]] Result<void> delete_item_from_object(
    Item object,
    std::string_view key) noexcept;

// Slab allocation is reclaimed as a unit; this invalidates every slab handle.
void delete_all(Slab& slab) noexcept;

} // namespace slabjson::cjson

#endif
