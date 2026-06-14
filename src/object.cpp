#include <slabjson/object.hpp>

#include <slabjson/slab.hpp>

namespace slabjson {

Object::Object(Slab* slab, NodeId id, std::uint32_t generation) noexcept
    : slab_(slab)
    , id_(id)
    , generation_(generation)
{
}

bool Object::valid() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Object;
}

Result<void> Object::add(std::string_view key, Value child) noexcept
{
    if (slab_ == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    auto validation =
        slab_->validate_attachment(id_, generation_, child, ValueType::Object);
    if (!validation) {
        return validation.error();
    }

    auto key_result = slab_->store_string(key);
    if (!key_result) {
        return key_result.error();
    }

    auto* child_node = slab_->node_for(child.id_, child.generation_);
    child_node->key = key_result.value();
    slab_->append_child_unchecked(id_, child.id_);
    return {};
}

Result<void> Object::add(
    std::string_view key,
    std::string_view string_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_string(string_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, const char* string_value) noexcept
{
    if (string_value == nullptr) {
        return Error{ErrorCode::InvalidArgument, 0};
    }
    return add(key, std::string_view{string_value});
}

Result<void> Object::add(std::string_view key, bool bool_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_bool(bool_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, std::int64_t number_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, std::uint64_t number_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, double number_value) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_number(number_value);
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add_null(std::string_view key) noexcept
{
    if (!valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    const auto checkpoint = slab_->checkpoint();
    auto value_result = slab_->make_null();
    if (!value_result) {
        return value_result.error();
    }

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<Object> Object::add_object(std::string_view key) noexcept
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
    auto add_result = add(key, object.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return object;
}

Result<Array> Object::add_array(std::string_view key) noexcept
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
    auto add_result = add(key, array.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return array;
}

std::optional<Value> Object::find(std::string_view key) const noexcept
{
    const auto* object_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (object_node == nullptr || object_node->type != ValueType::Object) {
        return std::nullopt;
    }

    NodeId child_id = object_node->first_child;
    while (child_id != Slab::kInvalidNodeId) {
        const auto* child = slab_->node_for(child_id, generation_);
        if (child == nullptr) {
            return std::nullopt;
        }
        if (slab_->view_string(child->key) == key) {
            return slab_->make_value(child_id);
        }
        child_id = child->next_sibling;
    }
    return std::nullopt;
}

bool Object::contains(std::string_view key) const noexcept
{
    return find(key).has_value();
}

Result<void> Object::remove(std::string_view key) noexcept
{
    auto* object_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (object_node == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    if (object_node->type != ValueType::Object) {
        return Error{ErrorCode::TypeMismatch, 0};
    }

    NodeId previous_id = Slab::kInvalidNodeId;
    NodeId child_id = object_node->first_child;
    while (child_id != Slab::kInvalidNodeId) {
        auto* child = slab_->node_for(child_id, generation_);
        if (child == nullptr) {
            return Error{ErrorCode::InternalError, 0};
        }

        if (slab_->view_string(child->key) == key) {
            if (previous_id == Slab::kInvalidNodeId) {
                object_node->first_child = child->next_sibling;
            } else {
                slab_->node_for(previous_id, generation_)->next_sibling =
                    child->next_sibling;
            }
            if (object_node->last_child == child_id) {
                object_node->last_child = previous_id;
            }
            child->parent = Slab::kInvalidNodeId;
            child->next_sibling = Slab::kInvalidNodeId;
            child->key = {};
            return {};
        }

        previous_id = child_id;
        child_id = child->next_sibling;
    }

    return Error{ErrorCode::NotFound, 0};
}

std::size_t Object::size() const noexcept
{
    const auto* object_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (object_node == nullptr || object_node->type != ValueType::Object) {
        return 0;
    }

    std::size_t count = 0;
    NodeId child_id = object_node->first_child;
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

bool Object::empty() const noexcept
{
    return size() == 0;
}

Object::iterator Object::begin() const noexcept
{
    const auto* object_node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (object_node == nullptr || object_node->type != ValueType::Object) {
        return end();
    }
    return ObjectIterator{
        slab_,
        object_node->first_child,
        generation_,
    };
}

Object::iterator Object::end() const noexcept
{
    return ObjectIterator{
        slab_,
        Slab::kInvalidNodeId,
        generation_,
    };
}

Value Object::value() const noexcept
{
    return Value{slab_, id_, generation_};
}

Object::operator Value() const noexcept
{
    return value();
}

ObjectIterator::ObjectIterator(
    Slab* slab,
    NodeId id,
    std::uint32_t generation) noexcept
    : slab_(slab)
    , id_(id)
    , generation_(generation)
{
}

ObjectMember ObjectIterator::operator*() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr) {
        return ObjectMember{
            {},
            Value{nullptr, 0, 0},
        };
    }
    return ObjectMember{
        slab_->view_string(node->key),
        Value{slab_, id_, generation_},
    };
}

ObjectIterator& ObjectIterator::operator++() noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    id_ = node == nullptr ? Slab::kInvalidNodeId : node->next_sibling;
    return *this;
}

ObjectIterator ObjectIterator::operator++(int) noexcept
{
    ObjectIterator previous = *this;
    ++(*this);
    return previous;
}

} // namespace slabjson
