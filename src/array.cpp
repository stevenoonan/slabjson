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

Result<void> Array::status() const noexcept
{
    if (slab_ == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    auto slab_status = slab_->status();
    if (!slab_status) {
        return slab_status.error();
    }
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    return {};
}

void Array::clear_error() noexcept
{
    if (slab_ != nullptr) {
        slab_->clear_error();
    }
}

Error Array::record_error(Error error) noexcept
{
    return slab_ == nullptr ? error : slab_->record_error(error);
}

Result<void> Array::add(Value child) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    auto validation =
        slab_->validate_attachment(id_, generation_, child, ValueType::Array);
    if (!validation) {
        return record_error(validation.error());
    }

    auto* child_node = slab_->node_for(child.id_, child.generation_);
    child_node->key = {};
    child_node->key_flags = 0;
    slab_->append_child_unchecked(id_, child.id_);
    return {};
}

Result<void> Array::add(std::string_view string_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_string(string_value);
    if (!value_result) {
        return record_error(value_result.error());
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
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }
    if (string_value == nullptr) {
        return record_error(Error{ErrorCode::InvalidArgument, 0});
    }
    return add(std::string_view{string_value});
}

Result<void> Array::add(bool bool_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_bool(bool_value);
    if (!value_result) {
        return record_error(value_result.error());
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add(std::int64_t number_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return record_error(value_result.error());
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add(std::uint64_t number_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return record_error(value_result.error());
    }

    auto add_result = add(value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Array::add(double number_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return record_error(value_result.error());
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
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_null();
    if (!value_result) {
        return record_error(value_result.error());
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
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto object_result = slab_->make_object();
    if (!object_result) {
        return record_error(object_result.error());
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
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    const auto checkpoint = slab_->checkpoint();
    auto array_result = slab_->make_array();
    if (!array_result) {
        return record_error(array_result.error());
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

Array::iterator Array::begin() const noexcept
{
    const auto* array_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (array_node == nullptr || array_node->type != ValueType::Array) {
        return end();
    }
    return ArrayIterator{
        slab_,
        array_node->first_child,
        generation_,
    };
}

Array::iterator Array::end() const noexcept
{
    return ArrayIterator{
        slab_,
        Slab::kInvalidNodeId,
        generation_,
    };
}

Value Array::value() const noexcept
{
    return Value{slab_, id_, generation_};
}

Array::operator Value() const noexcept
{
    return value();
}

ArrayIterator::ArrayIterator(
    Slab* slab,
    NodeId id,
    std::uint32_t generation) noexcept
    : slab_(slab)
    , id_(id)
    , generation_(generation)
{
}

Value ArrayIterator::operator*() const noexcept
{
    if (slab_ == nullptr
        || slab_->node_for(id_, generation_) == nullptr) {
        return Value{nullptr, 0, 0};
    }
    return Value{slab_, id_, generation_};
}

ArrayIterator& ArrayIterator::operator++() noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    id_ = node == nullptr ? Slab::kInvalidNodeId : node->next_sibling;
    return *this;
}

ArrayIterator ArrayIterator::operator++(int) noexcept
{
    ArrayIterator previous = *this;
    ++(*this);
    return previous;
}

} // namespace slabjson
