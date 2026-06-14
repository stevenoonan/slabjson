#include <slabjson/serialize.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include <slabjson/slab.hpp>

namespace slabjson {
namespace detail {

class Writer {
public:
    explicit Writer(std::span<char> output) noexcept
        : output_(output)
        , count_only_(false)
    {
    }

    Writer() noexcept
        : count_only_(true)
    {
    }

    [[nodiscard]] bool append(char character) noexcept
    {
        if (position_ == std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        if (!count_only_) {
            if (position_ >= output_.size()) {
                return false;
            }
            output_[position_] = character;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] bool append(std::string_view text) noexcept
    {
        if (text.size() > std::numeric_limits<std::size_t>::max() - position_) {
            return false;
        }
        if (!count_only_ && text.size() > output_.size() - position_) {
            return false;
        }

        if (!count_only_) {
            for (char character : text) {
                output_[position_++] = character;
            }
        } else {
            position_ += text.size();
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
    bool count_only_;
};

class Serializer {
public:
    explicit Serializer(Value value) noexcept
        : slab_(value.slab_)
        , root_id_(value.id_)
        , generation_(value.generation_)
    {
    }

    [[nodiscard]] Result<std::size_t> measure() const noexcept
    {
        Writer writer;
        return write(writer);
    }

    [[nodiscard]] Result<std::size_t> write_to(
        std::span<char> output) const noexcept
    {
        if (overlaps_slab(output)) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        Writer writer{output};
        return write(writer);
    }

private:
    using NodeId = Slab::NodeId;

    [[nodiscard]] Result<std::size_t> write(Writer& writer) const noexcept
    {
        if (slab_ == nullptr
            || slab_->node_for(root_id_, generation_) == nullptr) {
            return Error{ErrorCode::InvalidHandle, 0};
        }

        NodeId current_id = root_id_;
        while (true) {
            const Slab::Node* current =
                slab_->node_for(current_id, generation_);
            if (current == nullptr) {
                return Error{ErrorCode::InternalError, 0};
            }

            switch (current->type) {
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
                    write_number(writer, current->payload.number_value);
                if (!number_result) {
                    return number_result.error();
                }
                break;
            }
            case ValueType::String:
                if (!write_string(
                        writer,
                        slab_->view_string(current->payload.string_value))) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                break;
            case ValueType::Array:
            case ValueType::Object: {
                const bool is_object = current->type == ValueType::Object;
                if (!writer.append(is_object ? '{' : '[')) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }

                if (current->first_child != Slab::kInvalidNodeId) {
                    const Slab::Node* child =
                        slab_->node_for(current->first_child, generation_);
                    if (child == nullptr || child->parent != current_id) {
                        return Error{ErrorCode::InternalError, 0};
                    }
                    if (is_object && !write_member_prefix(writer, *child)) {
                        return Error{ErrorCode::OutputCapacityExceeded, 0};
                    }
                    current_id = current->first_child;
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

                current = slab_->node_for(current_id, generation_);
                if (current == nullptr
                    || current->parent == Slab::kInvalidNodeId) {
                    return Error{ErrorCode::InternalError, 0};
                }

                const NodeId parent_id = current->parent;
                const Slab::Node* parent =
                    slab_->node_for(parent_id, generation_);
                if (parent == nullptr
                    || (parent->type != ValueType::Array
                        && parent->type != ValueType::Object)) {
                    return Error{ErrorCode::InternalError, 0};
                }

                if (current->next_sibling != Slab::kInvalidNodeId) {
                    const Slab::Node* sibling =
                        slab_->node_for(current->next_sibling, generation_);
                    if (sibling == nullptr || sibling->parent != parent_id) {
                        return Error{ErrorCode::InternalError, 0};
                    }
                    if (!writer.append(',')) {
                        return Error{ErrorCode::OutputCapacityExceeded, 0};
                    }
                    if (parent->type == ValueType::Object
                        && !write_member_prefix(writer, *sibling)) {
                        return Error{ErrorCode::OutputCapacityExceeded, 0};
                    }
                    current_id = current->next_sibling;
                    break;
                }

                if (!writer.append(
                        parent->type == ValueType::Object ? '}' : ']')) {
                    return Error{ErrorCode::OutputCapacityExceeded, 0};
                }
                current_id = parent_id;
            }
        }
    }

    [[nodiscard]] Result<void> write_number(
        Writer& writer,
        double value) const noexcept
    {
        if (!std::isfinite(value)) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        char buffer[64];
        auto result = std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            value,
            std::chars_format::general);
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

    [[nodiscard]] bool write_string(
        Writer& writer,
        std::string_view value) const noexcept
    {
        static constexpr char kHex[] = "0123456789abcdef";

        if (!writer.append('"')) {
            return false;
        }
        for (unsigned char character : value) {
            switch (character) {
            case '"':
                if (!writer.append("\\\"")) {
                    return false;
                }
                break;
            case '\\':
                if (!writer.append("\\\\")) {
                    return false;
                }
                break;
            case '\b':
                if (!writer.append("\\b")) {
                    return false;
                }
                break;
            case '\f':
                if (!writer.append("\\f")) {
                    return false;
                }
                break;
            case '\n':
                if (!writer.append("\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!writer.append("\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!writer.append("\\t")) {
                    return false;
                }
                break;
            default:
                if (character < 0x20) {
                    const char escape[] = {
                        '\\',
                        'u',
                        '0',
                        '0',
                        kHex[(character >> 4) & 0x0f],
                        kHex[character & 0x0f],
                    };
                    if (!writer.append(
                            std::string_view{escape, sizeof(escape)})) {
                        return false;
                    }
                } else if (!writer.append(static_cast<char>(character))) {
                    return false;
                }
                break;
            }
        }
        return writer.append('"');
    }

    [[nodiscard]] bool write_member_prefix(
        Writer& writer,
        const Slab::Node& child) const noexcept
    {
        return write_string(writer, slab_->view_string(child.key))
            && writer.append(':');
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
};

} // namespace detail

Result<std::size_t> serialized_size(Value value) noexcept
{
    return detail::Serializer{value}.measure();
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
    return serializer.write_to(output);
}

} // namespace slabjson
