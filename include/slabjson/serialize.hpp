#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

[[nodiscard]] Result<std::size_t> serialized_size(Value value) noexcept;
[[nodiscard]] Result<std::size_t> serialized_size_pretty(
    Value value,
    std::uint8_t indent_spaces = 2) noexcept;
[[nodiscard]] Result<std::size_t> serialize(
    Value value,
    std::span<char> output) noexcept;
[[nodiscard]] Result<std::size_t> serialize_pretty(
    Value value,
    std::span<char> output,
    std::uint8_t indent_spaces = 2) noexcept;

} // namespace slabjson
