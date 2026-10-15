#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include <slabjson/result.hpp>

namespace slabjson::detail {

// Validate one sequence and report offsets relative to the entire input.
[[nodiscard]] inline Result<std::size_t> utf8_sequence_length(
    std::string_view value,
    std::size_t offset) noexcept
{
    if (offset >= value.size()) {
        return Error{ErrorCode::InvalidUtf8, offset};
    }
    const auto first = static_cast<unsigned char>(value[offset]);
    if (first < 0x80) {
        return std::size_t{1};
    }

    std::size_t length = 0;
    if (first >= 0xc2 && first <= 0xdf) {
        length = 2;
    } else if (first >= 0xe0 && first <= 0xef) {
        length = 3;
    } else if (first >= 0xf0 && first <= 0xf4) {
        length = 4;
    } else {
        return Error{ErrorCode::InvalidUtf8, offset};
    }

    if (length > value.size() - offset) {
        return Error{ErrorCode::InvalidUtf8, offset};
    }
    for (std::size_t index = 1; index < length; ++index) {
        const auto continuation =
            static_cast<unsigned char>(value[offset + index]);
        if (continuation < 0x80 || continuation > 0xbf) {
            return Error{ErrorCode::InvalidUtf8, offset + index};
        }
    }

    const auto second = static_cast<unsigned char>(value[offset + 1]);
    if ((first == 0xe0 && second < 0xa0)
        || (first == 0xed && second > 0x9f)
        || (first == 0xf0 && second < 0x90)
        || (first == 0xf4 && second > 0x8f)) {
        return Error{ErrorCode::InvalidUtf8, offset};
    }
    return length;
}

[[nodiscard]] inline std::optional<std::size_t> invalid_utf8_offset(
    std::string_view value) noexcept
{
    std::size_t offset = 0;
    while (offset < value.size()) {
        // Keep the common ASCII path cheap for strings and serialization.
        if (static_cast<unsigned char>(value[offset]) < 0x80) {
            ++offset;
            continue;
        }
        auto sequence = utf8_sequence_length(value, offset);
        if (!sequence) {
            return sequence.error().offset;
        }
        offset += sequence.value();
    }
    return std::nullopt;
}

} // namespace slabjson::detail
