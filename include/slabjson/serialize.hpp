#pragma once

#include <cstddef>
#include <span>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

[[nodiscard]] Result<std::size_t> serialized_size(Value value) noexcept;
[[nodiscard]] Result<std::size_t> serialize(
    Value value,
    std::span<char> output) noexcept;

} // namespace slabjson
