#include <slabjson/array.hpp>

#include <slabjson/slab.hpp>

namespace slabjson {

Array::Array(Slab* slab, NodeId id, std::uint32_t generation) noexcept
    : slab_(slab)
    , id_(id)
    , generation_(generation)
{
}

bool Array::valid() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Array;
}

Result<void> Array::add(Value child) noexcept
{
    if (slab_ == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    auto validation =
        slab_->validate_attachment(id_, generation_, child, ValueType::Array);
    if (!validation) {
        return validation.error();
    }

    auto* child_node = slab_->node_for(child.id_, child.generation_);
    child_node->key = {};
    slab_->append_child_unchecked(id_, child.id_);
    return {};
}

Result<void> Array::add(std::string_view string_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_string(string_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add(const char* string_value) noexcept
{
    if (string_value == nullptr) {
        return Error{ErrorCode::InvalidArgument, 0};
    }
    return add(std::string_view{string_value});
}

Result<void> Array::add(bool bool_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_bool(bool_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add(int number_value) noexcept
{
    return add(static_cast<double>(number_value));
}

Result<void> Array::add(std::int64_t number_value) noexcept
{
    return add(static_cast<double>(number_value));
}

Result<void> Array::add(double number_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add_null() noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_null();
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<Object> Array::add_object() noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto object_result = slab_->make_object();
    if (!object_result) {
        return object_result.error();
    }

    Object object = object_result.value();
    auto add_result = add(object.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return object;
}

Result<Array> Array::add_array() noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto array_result = slab_->make_array();
    if (!array_result) {
        return array_result.error();
    }

    Array array = array_result.value();
    auto add_result = add(array.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return array;
}

std::optional<Value> Array::at(std::size_t index) const noexcept
{
    const auto* array_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (array_node == nullptr || array_node->type != ValueType::Array) {
        return std::nullopt;
    }

    NodeId child_id = array_node->first_child;
    while (child_id != Slab::kInvalidNodeId && index > 0) {
        const auto* child = slab_->node_for(child_id, generation_);
        if (child == nullptr) {
            return std::nullopt;
        }
        child_id = child->next_sibling;
        --index;
    }

    if (child_id == Slab::kInvalidNodeId) {
        return std::nullopt;
    }
    return slab_->make_value(child_id);
}

std::size_t Array::size() const noexcept
{
    const auto* array_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (array_node == nullptr || array_node->type != ValueType::Array) {
        return 0;
    }

    std::size_t count = 0;
    NodeId child_id = array_node->first_child;
    while (child_id != Slab::kInvalidNodeId) {
        const auto* child = slab_->node_for(child_id, generation_);
        if (child == nullptr) {
            return 0;
        }
        ++count;
        child_id = child->next_sibling;
    }
    return count;
}

bool Array::empty() const noexcept
{
    return size() == 0;
}

Value Array::value() const noexcept
{
    return Value{slab_, id_, generation_};
}

Array::operator Value() const noexcept
{
    return value();
}

} // namespace slabjson
