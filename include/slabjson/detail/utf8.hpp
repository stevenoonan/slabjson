#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace slabjson::detail {

[[nodiscard]] inline std::optional<std::size_t> invalid_utf8_offset(
    std::string_view value) noexcept
{
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first < 0x80) {
            ++offset;
            continue;
        }

        std::size_t length = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
        } else {
            return offset;
        }

        if (length > value.size() - offset) {
            return offset;
        }
        for (std::size_t index = 1; index < length; ++index) {
            const auto continuation =
                static_cast<unsigned char>(value[offset + index]);
            if (continuation < 0x80 || continuation > 0xbf) {
                return offset + index;
            }
        }

        const auto second =
            static_cast<unsigned char>(value[offset + 1]);
        if ((first == 0xe0 && second < 0xa0)
            || (first == 0xed && second > 0x9f)
            || (first == 0xf0 && second < 0x90)
            || (first == 0xf4 && second > 0x8f)) {
            return offset;
        }
        offset += length;
    }
    return std::nullopt;
}

} // namespace slabjson::detail
