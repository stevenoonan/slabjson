#pragma once

#include <cstdint>
#include <string_view>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

class Slab;

struct ParseOptions {
    std::uint16_t max_depth{32};
    bool allow_trailing_whitespace{true};
};

[[nodiscard]] Result<Value> parse(
    Slab& slab,
    std::string_view input,
    ParseOptions options = {}) noexcept;

} // namespace slabjson
