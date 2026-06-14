#pragma once

#include <cstdint>
#include <string_view>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

class Slab;

inline constexpr std::uint16_t kMaxParserDepth = 32;

struct ParseOptions {
    std::uint16_t max_depth{kMaxParserDepth};
    bool allow_trailing_whitespace{true};
};

[[nodiscard]] Result<Value> parse(
    Slab& slab,
    std::string_view input,
    ParseOptions options = {}) noexcept;

} // namespace slabjson
