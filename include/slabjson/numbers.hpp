#pragma once

#include <concepts>
#include <cstdint>
#include <slabjson/config.hpp>

namespace slabjson {

#if SLABJSON_FULL_NUMBERS
using int_t = std::int64_t;
using uint_t = std::uint64_t;
#else
using int_t = std::int32_t;
using uint_t = std::uint32_t;
#endif
#if SLABJSON_HAS_FLOAT
#if SLABJSON_FULL_NUMBERS
using float_t = double;
#else
using float_t = float;
#endif
#endif

namespace detail {
// Deleted overloads prevent implicit narrowing, including floating point to bool.
template <typename T>
concept UnsupportedNumber =
    (std::integral<T> && sizeof(T) > sizeof(uint_t))
    || (std::floating_point<T>
#if SLABJSON_HAS_FLOAT
        && sizeof(T) > sizeof(float_t)
#endif
    );
}

} // namespace slabjson
