#include <slabjson/serialize.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>

#include <slabjson/detail/utf8.hpp>
#include <slabjson/slab.hpp>

namespace slabjson {
namespace detail {

class CountingWriter {
public:
    [[nodiscard]] bool append(char character) noexcept
    {
        (void)character;
        if (position_ == std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] bool append(std::string_view text) noexcept
    {
        if (text.size() > std::numeric_limits<std::size_t>::max() - position_) {
            return false;
        }
        position_ += text.size();
        return true;
    }

    [[nodiscard]] bool append_repeated(
        char character,
        std::size_t count) noexcept
    {
        (void)character;
        if (count > std::numeric_limits<std::size_t>::max() - position_) {
            return false;
        }
        position_ += count;
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return position_;
    }

private:
    std::size_t position_{0};
};

class CheckedWriter {
public:
    explicit CheckedWriter(std::span<char> output) noexcept
        : output_(output)
    {
    }

    [[nodiscard]] bool append(char character) noexcept
    {
        if (position_ >= output_.size()) {
            return false;
        }
        output_[position_++] = character;
        return true;
    }

    [[nodiscard]] bool append(std::string_view text) noexcept
    {
        if (text.size() > output_.size() - position_) {
            return false;
        }
        if (!text.empty()) {
            std::memcpy(output_.data() + position_, text.data(), text.size());
            position_ += text.size();
        }
        return true;
    }

    [[nodiscard]] bool append_repeated(
        char character,
        std::size_t count) noexcept
    {
        if (count > output_.size() - position_) {
            return false;
        }
        if (count != 0) {
            std::memset(output_.data() + position_, character, count);
            position_ += count;
        }
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return position_;
    }

private:
    std::span<char> output_{};
    std::size_t position_{0};
};

class UncheckedWriter {
public:
    explicit UncheckedWriter(std::span<char> output) noexcept
        : output_(output.data())
    {
    }

    [[nodiscard]] bool append(char character) noexcept
    {
        output_[position_++] = character;
        return true;
    }

    [[nodiscard]] bool append(std::string_view text) noexcept
    {
        if (!text.empty()) {
            std::memcpy(output_ + position_, text.data(), text.size());
            position_ += text.size();
        }
        return true;
    }

    [[nodiscard]] bool append_repeated(
        char character,
        std::size_t count) noexcept
    {
        if (count != 0) {
            std::memset(output_ + position_, character, count);
            position_ += count;
        }
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return position_;
    }

private:
    char* output_;
    std::size_t position_{0};
};

class Serializer {
public:
    explicit Serializer(
        Value value,
        bool pretty = false,
        std::uint8_t indent_spaces = 2) noexcept
        : slab_(value.slab_)
        , root_id_(value.id_)
        , generation_(value.generation_)
        , pretty_(pretty)
        , indent_spaces_(indent_spaces)
    {
    }

    [[nodiscard]] Result<std::size_t> measure() const noexcept
    {
        CountingWriter writer;
        return write<true, true>(writer);
    }

    [[nodiscard]] Result<std::size_t> write_partial_to(
        std::span<char> output) const noexcept
    {
        if (overlaps_slab(output)) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        CheckedWriter writer{output};
        return write<true, true>(writer);
    }

    [[nodiscard]] Result<std::size_t> write_prevalidated_to(
        std::span<char> output) const noexcept
    {
        if (overlaps_slab(output)) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        UncheckedWriter writer{output};
        return write<false, false>(writer);
    }

private:
    using NodeId = Slab::NodeId;

    template<bool ValidateUtf8, bool ValidateGraph, typename Writer>
    [[nodiscard]] Result<std::size_t> write(Writer& writer) const noexcept
    {
        if (slab_ == nullptr) {
            return Error{ErrorCode::InvalidHandle, 0};
        }
        if constexpr (ValidateGraph) {
            if (slab_->node_for(root_id_, generation_) == nullptr) {
                return Error{ErrorCode::InvalidHandle, 0};
            }
        }

        NodeId current_id = root_id_;
        std::size_t depth = 0;
        while (true) {
            const Slab::Node* current =
                ValidateGraph
                    ? slab_->node_for(current_id, generation_)
                    : slab_->node_at_unchecked(current_id);
            if constexpr (ValidateGraph) {
                if (current == nullptr) {
                    return Error{ErrorCode::InternalError, 0};
                }
            }

            switch (current->type) {
            case ValueType::Invalid:
                return Error{ErrorCode::InvalidHandle, 0};
            case ValueType::Null:
                if (!writer.append("null")) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                break;
            case ValueType::Bool:
                if (!writer.append(
                        current->payload.bool_value ? "true" : "false")) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                break;
            case ValueType::Number: {
                auto number_result =
                    write_number(writer, *current);
                if (!number_result) {
                    return number_result.error();
                }
                break;
            }
            case ValueType::String: {
                auto string_result = write_string(
                    current->value_flags,
                    ValidateUtf8,
                    writer,
                    slab_->view_string(current->payload.string_value));
                if (!string_result) {
                    return string_result.error();
                }
                break;
            }
            case ValueType::Array:
            case ValueType::Object: {
                const bool is_object = current->type == ValueType::Object;
                if (!writer.append(is_object ? '{' : '[')) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }

                if (current->first_child != Slab::kInvalidNodeId) {
                    const Slab::Node* child =
                        ValidateGraph
                            ? slab_->node_for(current->first_child, generation_)
                            : slab_->node_at_unchecked(current->first_child);
                    if constexpr (ValidateGraph) {
                        if (child == nullptr || child->parent != current_id) {
                            return Error{ErrorCode::InternalError, 0};
                        }
                    }
                    if (pretty_) {
                        if (!writer.append('\n')
                            || !write_indent(writer, depth + 1)) {
                            return Error{
                                ErrorCode::OutputCapacityExceeded,
                                0,
                            };
                        }
                    }
                    if (is_object) {
                        auto prefix_result =
                            write_member_prefix<ValidateUtf8>(
                                writer,
                                *child);
                        if (!prefix_result) {
                            return prefix_result.error();
                        }
                    }
                    current_id = current->first_child;
                    ++depth;
                    continue;
                }

                if (!writer.append(is_object ? '}' : ']')) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                break;
            }
            }

            while (true) {
                if (current_id == root_id_) {
                    return writer.size();
                }

                current = ValidateGraph
                    ? slab_->node_for(current_id, generation_)
                    : slab_->node_at_unchecked(current_id);
                if constexpr (ValidateGraph) {
                    if (current == nullptr
                        || current->parent == Slab::kInvalidNodeId) {
                        return Error{ErrorCode::InternalError, 0};
                    }
                }

                const NodeId parent_id = current->parent;
                const Slab::Node* parent =
                    ValidateGraph
                        ? slab_->node_for(parent_id, generation_)
                        : slab_->node_at_unchecked(parent_id);
                if constexpr (ValidateGraph) {
                    if (parent == nullptr
                        || (parent->type != ValueType::Array
                            && parent->type != ValueType::Object)) {
                        return Error{ErrorCode::InternalError, 0};
                    }
                }

                if (current->next_sibling != Slab::kInvalidNodeId) {
                    const Slab::Node* sibling =
                        ValidateGraph
                            ? slab_->node_for(current->next_sibling, generation_)
                            : slab_->node_at_unchecked(current->next_sibling);
                    if constexpr (ValidateGraph) {
                        if (sibling == nullptr || sibling->parent != parent_id) {
                            return Error{ErrorCode::InternalError, 0};
                        }
                    }
                    if (!writer.append(',')) {
                        return Error{ErrorCode::OutputCapacityExceeded, 0};
                    }
                    if (pretty_) {
                        if (!writer.append('\n')
                            || !write_indent(writer, depth)) {
                            return Error{
                                ErrorCode::OutputCapacityExceeded,
                                0,
                            };
                        }
                    }
                    if (parent->type == ValueType::Object) {
                        auto prefix_result =
                            write_member_prefix<ValidateUtf8>(
                                writer,
                                *sibling);
                        if (!prefix_result) {
                            return prefix_result.error();
                        }
                    }
                    current_id = current->next_sibling;
                    break;
                }

                if (pretty_) {
                    if (depth == 0
                        || !writer.append('\n')
                        || !write_indent(writer, depth - 1)) {
                        return Error{
                            depth == 0
                                ? ErrorCode::InternalError
                                : ErrorCode::OutputCapacityExceeded,
                            0,
                        };
                    }
                }
                if (!writer.append(
                        parent->type == ValueType::Object ? '}' : ']')) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                current_id = parent_id;
                --depth;
            }
        }
    }

    template<typename Writer>
    [[nodiscard]] Result<void> write_number(
        Writer& writer,
        const Slab::Node& node) const noexcept
    {
        char buffer[64];
        std::to_chars_result result{};
        switch (Slab::node_number_kind(node)) {
        case NumberKind::SignedInteger:
            result = std::to_chars(
                buffer,
                buffer + sizeof(buffer),
                node.payload.signed_integer);
            break;
        case NumberKind::UnsignedInteger:
            result = std::to_chars(
                buffer,
                buffer + sizeof(buffer),
                node.payload.unsigned_integer);
            break;
        case NumberKind::FloatingPoint:
            if (!std::isfinite(node.payload.floating_point)) {
                return Error{ErrorCode::NonFiniteNumber, 0};
            }
            result = std::to_chars(
                buffer,
                buffer + sizeof(buffer),
                node.payload.floating_point,
                std::chars_format::general);
            break;
        }
        if (result.ec != std::errc{}) {
            return Error{ErrorCode::InternalError, 0};
        }
        if (!writer.append(
                std::string_view{
                    buffer,
                    static_cast<std::size_t>(result.ptr - buffer),
                })) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }
        return {};
    }

    template<typename Writer>
    [[nodiscard]] Result<void> write_string(
        std::uint8_t flags,
        bool validate_utf8,
        Writer& writer,
        std::string_view value) const noexcept
    {
        static constexpr char kHex[] = "0123456789abcdef";

        if (validate_utf8) {
            if (auto invalid = invalid_utf8_offset(value)) {
                return Error{ErrorCode::InvalidUtf8, *invalid};
            }
        }
        if (!Slab::string_needs_json_escape(flags)) {
            if (!writer.append('"')
                || !writer.append(value)
                || !writer.append('"')) {
                return Error{ErrorCode::OutputCapacityExceeded, 0};
            }
            return {};
        }
        if (!writer.append('"')) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }

        std::size_t run_start = 0;
        for (std::size_t index = 0; index < value.size(); ++index) {
            const auto character =
                static_cast<unsigned char>(value[index]);
            std::string_view escape;
            char control_escape[6];
            switch (character) {
            case '"':
                escape = "\\\"";
                break;
            case '\\':
                escape = "\\\\";
                break;
            case '\b':
                escape = "\\b";
                break;
            case '\f':
                escape = "\\f";
                break;
            case '\n':
                escape = "\\n";
                break;
            case '\r':
                escape = "\\r";
                break;
            case '\t':
                escape = "\\t";
                break;
            default:
                if (character < 0x20) {
                    const char encoded[] = {
                        '\\',
                        'u',
                        '0',
                        '0',
                        kHex[(character >> 4) & 0x0f],
                        kHex[character & 0x0f],
                    };
                    std::memcpy(
                        control_escape,
                        encoded,
                        sizeof(control_escape));
                    escape = std::string_view{
                        control_escape,
                        sizeof(control_escape),
                    };
                }
                break;
            }

            if (escape.empty()) {
                continue;
            }
            if (!writer.append(value.substr(run_start, index - run_start))
                || !writer.append(escape)) {
                return Error{ErrorCode::OutputCapacityExceeded, 0};
            }
            run_start = index + 1;
        }
        if (!writer.append(value.substr(run_start))) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }
        if (!writer.append('"')) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }
        return {};
    }

    template<bool ValidateUtf8, typename Writer>
    [[nodiscard]] Result<void> write_member_prefix(
        Writer& writer,
        const Slab::Node& child) const noexcept
    {
        auto key_result = write_string(
            child.key_flags,
            ValidateUtf8,
            writer,
            slab_->view_string(child.key));
        if (!key_result) {
            return key_result.error();
        }
        if (!writer.append(':')) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }
        if (pretty_ && !writer.append(' ')) {
            return Error{ErrorCode::OutputCapacityExceeded, 0};
        }
        return {};
    }

    template<typename Writer>
    [[nodiscard]] bool write_indent(
        Writer& writer,
        std::size_t depth) const noexcept
    {
        if (indent_spaces_ != 0
            && depth > std::numeric_limits<std::size_t>::max()
                    / indent_spaces_) {
            return false;
        }
        const std::size_t count =
            depth * static_cast<std::size_t>(indent_spaces_);
        return writer.append_repeated(' ', count);
    }

    [[nodiscard]] bool overlaps_slab(std::span<char> output) const noexcept
    {
        if (slab_ == nullptr || output.empty() || slab_->storage_.empty()) {
            return false;
        }

        const auto output_start =
            reinterpret_cast<std::uintptr_t>(output.data());
        const auto slab_start =
            reinterpret_cast<std::uintptr_t>(slab_->storage_.data());
        if (output_start <= slab_start) {
            return slab_start - output_start < output.size();
        }
        return output_start - slab_start < slab_->storage_.size();
    }

    Slab* slab_;
    NodeId root_id_;
    std::uint32_t generation_;
    bool pretty_;
    std::uint8_t indent_spaces_;
};

} // namespace detail

Result<std::size_t> serialized_size(Value value) noexcept
{
    return detail::Serializer{value}.measure();
}

Result<std::size_t> serialized_size_pretty(
    Value value,
    std::uint8_t indent_spaces) noexcept
{
    return detail::Serializer{value, true, indent_spaces}.measure();
}

Result<std::size_t> serialize(Value value, std::span<char> output) noexcept
{
    detail::Serializer serializer{value};
    auto size_result = serializer.measure();
    if (!size_result) {
        return size_result.error();
    }
    if (size_result.value() > output.size()) {
        return Error{ErrorCode::OutputCapacityExceeded, 0};
    }
    return serializer.write_prevalidated_to(output);
}

Result<std::size_t> serialize_partial(
    Value value,
    std::span<char> output) noexcept
{
    return detail::Serializer{value}.write_partial_to(output);
}

Result<std::size_t> serialize_pretty(
    Value value,
    std::span<char> output,
    std::uint8_t indent_spaces) noexcept
{
    detail::Serializer serializer{value, true, indent_spaces};
    auto size_result = serializer.measure();
    if (!size_result) {
        return size_result.error();
    }
    if (size_result.value() > output.size()) {
        return Error{ErrorCode::OutputCapacityExceeded, 0};
    }
    return serializer.write_prevalidated_to(output);
}

Result<std::size_t> serialize_pretty_partial(
    Value value,
    std::span<char> output,
    std::uint8_t indent_spaces) noexcept
{
    return detail::Serializer{
        value,
        true,
        indent_spaces,
    }.write_partial_to(output);
}

} // namespace slabjson
