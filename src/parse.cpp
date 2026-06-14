#include <slabjson/parse.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <slabjson/slab.hpp>

namespace slabjson {
namespace detail {

class Parser {
public:
    Parser(
        Slab& slab,
        std::string_view input,
        ParseOptions options) noexcept
        : slab_(slab)
        , input_(input)
        , options_(options)
    {
    }

    [[nodiscard]] Result<Value> run() noexcept
    {
        if (!slab_.valid()) {
            return Error{ErrorCode::InvalidArgument, 0};
        }
        if (overlaps_slab()) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        const Slab::Checkpoint start = slab_.checkpoint();
        skip_whitespace();
        if (position_ == input_.size()) {
            return Error{ErrorCode::ParseUnexpectedEnd, position_};
        }

        auto value_result = parse_value(0);
        if (!value_result) {
            slab_.rollback(start);
            return value_result.error();
        }

        if (options_.allow_trailing_whitespace) {
            skip_whitespace();
        }
        if (position_ != input_.size()) {
            slab_.rollback(start);
            return Error{ErrorCode::ParseTrailingCharacters, position_};
        }
        return value_result.value();
    }

private:
    struct StringScan {
        std::size_t end;
        std::size_t decoded_length;
    };

    [[nodiscard]] Result<Value> parse_value(std::uint16_t depth) noexcept
    {
        skip_whitespace();
        if (position_ == input_.size()) {
            return Error{ErrorCode::ParseUnexpectedEnd, position_};
        }

        switch (input_[position_]) {
        case 'n':
            return parse_null();
        case 't':
            return parse_bool("true", true);
        case 'f':
            return parse_bool("false", false);
        case '"':
            return parse_string_value();
        case '[':
            return parse_array(depth);
        case '{':
            return parse_object(depth);
        default:
            if (input_[position_] == '-'
                || is_digit(input_[position_])) {
                return parse_number();
            }
            return Error{ErrorCode::ParseUnexpectedToken, position_};
        }
    }

    [[nodiscard]] Result<Value> parse_null() noexcept
    {
        const std::size_t start = position_;
        auto literal = consume_literal("null");
        if (!literal) {
            return literal.error();
        }
        auto value = slab_.make_null();
        return with_offset(value, start);
    }

    [[nodiscard]] Result<Value> parse_bool(
        std::string_view literal,
        bool bool_value) noexcept
    {
        const std::size_t start = position_;
        auto consumed = consume_literal(literal);
        if (!consumed) {
            return consumed.error();
        }
        auto value = slab_.make_bool(bool_value);
        return with_offset(value, start);
    }

    [[nodiscard]] Result<Value> parse_number() noexcept
    {
        const std::size_t start = position_;

        if (input_[position_] == '-') {
            ++position_;
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseInvalidNumber, position_};
            }
        }

        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size()
                && is_digit(input_[position_])) {
                return Error{ErrorCode::ParseInvalidNumber, position_};
            }
        } else if (input_[position_] >= '1'
            && input_[position_] <= '9') {
            while (position_ < input_.size()
                && is_digit(input_[position_])) {
                ++position_;
            }
        } else {
            return Error{ErrorCode::ParseInvalidNumber, position_};
        }

        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            if (position_ == input_.size()
                || !is_digit(input_[position_])) {
                return Error{ErrorCode::ParseInvalidNumber, position_};
            }
            while (position_ < input_.size()
                && is_digit(input_[position_])) {
                ++position_;
            }
        }

        if (position_ < input_.size()
            && (input_[position_] == 'e'
                || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size()
                && (input_[position_] == '+'
                    || input_[position_] == '-')) {
                ++position_;
            }
            if (position_ == input_.size()
                || !is_digit(input_[position_])) {
                return Error{ErrorCode::ParseInvalidNumber, position_};
            }
            while (position_ < input_.size()
                && is_digit(input_[position_])) {
                ++position_;
            }
        }

        double number = 0.0;
        const char* first = input_.data() + start;
        const char* last = input_.data() + position_;
        const auto conversion = std::from_chars(
            first,
            last,
            number,
            std::chars_format::general);
        if (conversion.ec != std::errc{}
            || conversion.ptr != last
            || !std::isfinite(number)) {
            return Error{ErrorCode::ParseInvalidNumber, start};
        }

        auto value = slab_.make_number(number);
        return with_offset(value, start);
    }

    [[nodiscard]] Result<Value> parse_string_value() noexcept
    {
        const std::size_t start = position_;
        auto node_result = slab_.allocate_node(ValueType::String);
        if (!node_result) {
            return Error{node_result.error().code, start};
        }
        const Slab::NodeId id = node_result.value();

        auto string_result = parse_string();
        if (!string_result) {
            return string_result.error();
        }
        slab_.node_for(id, slab_.generation_)->payload.string_value =
            string_result.value();
        return slab_.make_value(id);
    }

    [[nodiscard]] Result<Value> parse_array(std::uint16_t depth) noexcept
    {
        const std::size_t start = position_;
        if (depth >= options_.max_depth) {
            return Error{ErrorCode::ParserDepthExceeded, start};
        }
        ++position_;

        auto array_result = slab_.make_array();
        if (!array_result) {
            return Error{array_result.error().code, start};
        }
        Array array = array_result.value();
        const Value array_value = array.value();

        skip_whitespace();
        if (position_ == input_.size()) {
            return Error{ErrorCode::ParseUnexpectedEnd, position_};
        }
        if (input_[position_] == ']') {
            ++position_;
            return array.value();
        }

        while (true) {
            auto child_result = parse_value(
                static_cast<std::uint16_t>(depth + 1));
            if (!child_result) {
                return child_result.error();
            }
            slab_.append_child_unchecked(
                array_value.id_,
                child_result.value().id_);

            skip_whitespace();
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseUnexpectedEnd, position_};
            }
            if (input_[position_] == ']') {
                ++position_;
                return array.value();
            }
            if (input_[position_] != ',') {
                return Error{ErrorCode::ParseUnexpectedToken, position_};
            }
            ++position_;
            skip_whitespace();
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseUnexpectedEnd, position_};
            }
        }
    }

    [[nodiscard]] Result<Value> parse_object(std::uint16_t depth) noexcept
    {
        const std::size_t start = position_;
        if (depth >= options_.max_depth) {
            return Error{ErrorCode::ParserDepthExceeded, start};
        }
        ++position_;

        auto object_result = slab_.make_object();
        if (!object_result) {
            return Error{object_result.error().code, start};
        }
        Object object = object_result.value();
        const Value object_value = object.value();

        skip_whitespace();
        if (position_ == input_.size()) {
            return Error{ErrorCode::ParseUnexpectedEnd, position_};
        }
        if (input_[position_] == '}') {
            ++position_;
            return object.value();
        }

        while (true) {
            if (input_[position_] != '"') {
                return Error{ErrorCode::ParseUnexpectedToken, position_};
            }
            auto key_result = parse_string();
            if (!key_result) {
                return key_result.error();
            }

            skip_whitespace();
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseUnexpectedEnd, position_};
            }
            if (input_[position_] != ':') {
                return Error{ErrorCode::ParseUnexpectedToken, position_};
            }
            ++position_;

            auto child_result = parse_value(
                static_cast<std::uint16_t>(depth + 1));
            if (!child_result) {
                return child_result.error();
            }
            Slab::Node* child =
                slab_.node_for(child_result.value().id_, slab_.generation_);
            child->key = key_result.value();
            slab_.append_child_unchecked(
                object_value.id_,
                child_result.value().id_);

            skip_whitespace();
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseUnexpectedEnd, position_};
            }
            if (input_[position_] == '}') {
                ++position_;
                return object.value();
            }
            if (input_[position_] != ',') {
                return Error{ErrorCode::ParseUnexpectedToken, position_};
            }
            ++position_;
            skip_whitespace();
            if (position_ == input_.size()) {
                return Error{ErrorCode::ParseUnexpectedEnd, position_};
            }
        }
    }

    [[nodiscard]] Result<Slab::StringRef> parse_string() noexcept
    {
        const std::size_t start = position_;
        auto scan_result = scan_string(start);
        if (!scan_result) {
            return scan_result.error();
        }
        const StringScan scan = scan_result.value();

        auto allocation =
            slab_.allocate_string(scan.decoded_length);
        if (!allocation) {
            return Error{allocation.error().code, start};
        }
        const Slab::StringRef ref = allocation.value();
        decode_string(start, scan.end, ref);
        position_ = scan.end;
        return ref;
    }

    [[nodiscard]] Result<StringScan> scan_string(
        std::size_t start) const noexcept
    {
        std::size_t cursor = start + 1;
        std::size_t decoded_length = 0;

        while (cursor < input_.size()) {
            const auto byte =
                static_cast<unsigned char>(input_[cursor]);
            if (byte == '"') {
                return StringScan{cursor + 1, decoded_length};
            }
            if (byte < 0x20) {
                return Error{ErrorCode::ParseInvalidString, cursor};
            }
            if (byte == '\\') {
                ++cursor;
                if (cursor == input_.size()) {
                    return Error{
                        ErrorCode::ParseUnexpectedEnd,
                        cursor,
                    };
                }

                switch (input_[cursor]) {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    ++decoded_length;
                    ++cursor;
                    break;
                case 'u': {
                    const std::size_t escape_offset = cursor - 1;
                    auto code_unit_result = parse_hex4(cursor + 1);
                    if (!code_unit_result) {
                        return code_unit_result.error();
                    }
                    std::uint32_t code_point = code_unit_result.value();
                    cursor += 5;

                    if (code_point >= 0xd800
                        && code_point <= 0xdbff) {
                        if (cursor + 6 > input_.size()
                            || input_[cursor] != '\\'
                            || input_[cursor + 1] != 'u') {
                            return Error{
                                ErrorCode::ParseInvalidUnicodeEscape,
                                escape_offset,
                            };
                        }
                        auto low_result = parse_hex4(cursor + 2);
                        if (!low_result) {
                            return low_result.error();
                        }
                        const std::uint32_t low = low_result.value();
                        if (low < 0xdc00 || low > 0xdfff) {
                            return Error{
                                ErrorCode::ParseInvalidUnicodeEscape,
                                cursor,
                            };
                        }
                        code_point = 0x10000
                            + ((code_point - 0xd800) << 10)
                            + (low - 0xdc00);
                        cursor += 6;
                    } else if (
                        code_point >= 0xdc00
                        && code_point <= 0xdfff) {
                        return Error{
                            ErrorCode::ParseInvalidUnicodeEscape,
                            escape_offset,
                        };
                    }

                    decoded_length += utf8_length(code_point);
                    break;
                }
                default:
                    return Error{
                        ErrorCode::ParseInvalidEscape,
                        cursor,
                    };
                }
            } else if (byte < 0x80) {
                ++decoded_length;
                ++cursor;
            } else {
                auto length_result = validate_utf8(cursor);
                if (!length_result) {
                    return length_result.error();
                }
                decoded_length += length_result.value();
                cursor += length_result.value();
            }

            if (decoded_length
                > std::numeric_limits<std::uint16_t>::max()) {
                return Error{
                    ErrorCode::StringCapacityExceeded,
                    start,
                };
            }
        }

        return Error{ErrorCode::ParseUnexpectedEnd, input_.size()};
    }

    void decode_string(
        std::size_t start,
        std::size_t end,
        Slab::StringRef ref) noexcept
    {
        std::byte* destination = slab_.storage_.data() + ref.offset;
        std::size_t output = 0;
        std::size_t cursor = start + 1;
        const std::size_t content_end = end - 1;

        while (cursor < content_end) {
            const auto byte =
                static_cast<unsigned char>(input_[cursor]);
            if (byte == '\\') {
                const char escape = input_[cursor + 1];
                cursor += 2;
                switch (escape) {
                case '"':
                    destination[output++] = std::byte{'"'};
                    break;
                case '\\':
                    destination[output++] = std::byte{'\\'};
                    break;
                case '/':
                    destination[output++] = std::byte{'/'};
                    break;
                case 'b':
                    destination[output++] = std::byte{'\b'};
                    break;
                case 'f':
                    destination[output++] = std::byte{'\f'};
                    break;
                case 'n':
                    destination[output++] = std::byte{'\n'};
                    break;
                case 'r':
                    destination[output++] = std::byte{'\r'};
                    break;
                case 't':
                    destination[output++] = std::byte{'\t'};
                    break;
                case 'u': {
                    std::uint32_t code_point =
                        parse_hex4_unchecked(cursor);
                    cursor += 4;
                    if (code_point >= 0xd800
                        && code_point <= 0xdbff) {
                        cursor += 2;
                        const std::uint32_t low =
                            parse_hex4_unchecked(cursor);
                        cursor += 4;
                        code_point = 0x10000
                            + ((code_point - 0xd800) << 10)
                            + (low - 0xdc00);
                    }
                    output += encode_utf8(
                        code_point,
                        destination + output);
                    break;
                }
                }
            } else if (byte < 0x80) {
                destination[output++] = std::byte{byte};
                ++cursor;
            } else {
                const std::size_t length =
                    validate_utf8(cursor).value();
                for (std::size_t index = 0; index < length; ++index) {
                    destination[output++] = std::byte{
                        static_cast<unsigned char>(
                            input_[cursor + index]),
                    };
                }
                cursor += length;
            }
        }
    }

    [[nodiscard]] Result<std::uint32_t> parse_hex4(
        std::size_t offset) const noexcept
    {
        if (offset + 4 > input_.size()) {
            return Error{
                ErrorCode::ParseInvalidUnicodeEscape,
                offset,
            };
        }

        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const int digit = hex_value(input_[offset + index]);
            if (digit < 0) {
                return Error{
                    ErrorCode::ParseInvalidUnicodeEscape,
                    offset + index,
                };
            }
            value = (value << 4) | static_cast<std::uint32_t>(digit);
        }
        return value;
    }

    [[nodiscard]] std::uint32_t parse_hex4_unchecked(
        std::size_t offset) const noexcept
    {
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            value = (value << 4)
                | static_cast<std::uint32_t>(
                    hex_value(input_[offset + index]));
        }
        return value;
    }

    [[nodiscard]] Result<std::size_t> validate_utf8(
        std::size_t offset) const noexcept
    {
        const auto first =
            static_cast<unsigned char>(input_[offset]);
        std::size_t length = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
        } else {
            return Error{ErrorCode::ParseInvalidString, offset};
        }

        if (offset + length > input_.size()) {
            return Error{ErrorCode::ParseInvalidString, offset};
        }
        for (std::size_t index = 1; index < length; ++index) {
            const auto continuation =
                static_cast<unsigned char>(input_[offset + index]);
            if (continuation < 0x80 || continuation > 0xbf) {
                return Error{
                    ErrorCode::ParseInvalidString,
                    offset + index,
                };
            }
        }

        const auto second =
            static_cast<unsigned char>(input_[offset + 1]);
        if ((first == 0xe0 && second < 0xa0)
            || (first == 0xed && second > 0x9f)
            || (first == 0xf0 && second < 0x90)
            || (first == 0xf4 && second > 0x8f)) {
            return Error{ErrorCode::ParseInvalidString, offset};
        }
        return length;
    }

    [[nodiscard]] Result<void> consume_literal(
        std::string_view literal) noexcept
    {
        for (std::size_t index = 0; index < literal.size(); ++index) {
            if (position_ + index >= input_.size()) {
                return Error{
                    ErrorCode::ParseUnexpectedEnd,
                    input_.size(),
                };
            }
            if (input_[position_ + index] != literal[index]) {
                return Error{
                    ErrorCode::ParseUnexpectedToken,
                    position_ + index,
                };
            }
        }
        position_ += literal.size();
        return {};
    }

    void skip_whitespace() noexcept
    {
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character != ' '
                && character != '\t'
                && character != '\n'
                && character != '\r') {
                return;
            }
            ++position_;
        }
    }

    [[nodiscard]] bool overlaps_slab() const noexcept
    {
        if (input_.empty() || slab_.storage_.empty()) {
            return false;
        }

        const auto input_start =
            reinterpret_cast<std::uintptr_t>(input_.data());
        const auto slab_start =
            reinterpret_cast<std::uintptr_t>(slab_.storage_.data());
        if (input_start <= slab_start) {
            return slab_start - input_start < input_.size();
        }
        return input_start - slab_start < slab_.storage_.size();
    }

    template <typename T>
    [[nodiscard]] static Result<T> with_offset(
        Result<T> result,
        std::size_t offset) noexcept
    {
        if (!result) {
            return Error{result.error().code, offset};
        }
        return std::move(result).value();
    }

    [[nodiscard]] static bool is_digit(char character) noexcept
    {
        return character >= '0' && character <= '9';
    }

    [[nodiscard]] static int hex_value(char character) noexcept
    {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        return -1;
    }

    [[nodiscard]] static std::size_t utf8_length(
        std::uint32_t code_point) noexcept
    {
        if (code_point <= 0x7f) {
            return 1;
        }
        if (code_point <= 0x7ff) {
            return 2;
        }
        if (code_point <= 0xffff) {
            return 3;
        }
        return 4;
    }

    [[nodiscard]] static std::size_t encode_utf8(
        std::uint32_t code_point,
        std::byte* output) noexcept
    {
        if (code_point <= 0x7f) {
            output[0] = std::byte{
                static_cast<unsigned char>(code_point),
            };
            return 1;
        }
        if (code_point <= 0x7ff) {
            output[0] = std::byte{
                static_cast<unsigned char>(
                    0xc0 | (code_point >> 6)),
            };
            output[1] = std::byte{
                static_cast<unsigned char>(
                    0x80 | (code_point & 0x3f)),
            };
            return 2;
        }
        if (code_point <= 0xffff) {
            output[0] = std::byte{
                static_cast<unsigned char>(
                    0xe0 | (code_point >> 12)),
            };
            output[1] = std::byte{
                static_cast<unsigned char>(
                    0x80 | ((code_point >> 6) & 0x3f)),
            };
            output[2] = std::byte{
                static_cast<unsigned char>(
                    0x80 | (code_point & 0x3f)),
            };
            return 3;
        }

        output[0] = std::byte{
            static_cast<unsigned char>(
                0xf0 | (code_point >> 18)),
        };
        output[1] = std::byte{
            static_cast<unsigned char>(
                0x80 | ((code_point >> 12) & 0x3f)),
        };
        output[2] = std::byte{
            static_cast<unsigned char>(
                0x80 | ((code_point >> 6) & 0x3f)),
        };
        output[3] = std::byte{
            static_cast<unsigned char>(
                0x80 | (code_point & 0x3f)),
        };
        return 4;
    }

    Slab& slab_;
    std::string_view input_;
    ParseOptions options_;
    std::size_t position_{0};
};

} // namespace detail

Result<Value> parse(
    Slab& slab,
    std::string_view input,
    ParseOptions options) noexcept
{
    return detail::Parser{slab, input, options}.run();
}

} // namespace slabjson
