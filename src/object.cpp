#include <slabjson/object.hpp>

#include <slabjson/slab.hpp>

namespace slabjson {

namespace {

Result<Value> find_required(
    const Object& object,
    std::string_view key) noexcept
{
    if (!object.valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }

    auto value = object.find(key);
    if (!value) {
        return Error{ErrorCode::NotFound, 0};
    }
    return *value;
}

} // namespace

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

Result<void> Object::status() const noexcept
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

void Object::clear_error() noexcept
{
    if (slab_ != nullptr) {
        slab_->clear_error();
    }
}

Error Object::record_error(Error error) noexcept
{
    return slab_ == nullptr ? error : slab_->record_error(error);
}

Result<void> Object::add(std::string_view key, Value child) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }

    auto validation =
        slab_->validate_attachment(id_, generation_, child, ValueType::Object);
    if (!validation) {
        return record_error(validation.error());
    }

    auto key_result = slab_->store_string_with_flags(key);
    if (!key_result) {
        return record_error(key_result.error());
    }

    auto* child_node = slab_->node_for(child.id_, child.generation_);
    child_node->key = key_result.value().ref;
    child_node->key_flags = key_result.value().flags;
    slab_->append_child_unchecked(id_, child.id_);
    return {};
}

Result<void> Object::add(
    std::string_view key,
    std::string_view string_value) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, const char* string_value) noexcept
{
    auto current = status();
    if (!current) {
        return record_error(current.error());
    }
    if (string_value == nullptr) {
        return record_error(Error{ErrorCode::InvalidArgument, 0});
    }
    return add(key, std::string_view{string_value});
}

Result<void> Object::add(std::string_view key, bool bool_value) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, std::int64_t number_value) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, std::uint64_t number_value) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add(std::string_view key, double number_value) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<void> Object::add_null(std::string_view key) noexcept
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

    auto add_result = add(key, value_result.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return {};
}

Result<Object> Object::add_object(std::string_view key) noexcept
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
    auto add_result = add(key, object.value());
    if (!add_result) {
        slab_->rollback(checkpoint);
        return add_result.error();
    }
    return object;
}

Result<Array> Object::add_array(std::string_view key) noexcept
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

Result<std::string_view> Object::get_string(
    std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_string();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<bool> Object::get_bool(std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_bool();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<std::int64_t> Object::get_int64(
    std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_int64();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<std::uint64_t> Object::get_uint64(
    std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_uint64();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<double> Object::get_number(std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_number();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<Object> Object::get_object(std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_object();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
}

Result<Array> Object::get_array(std::string_view key) const noexcept
{
    auto value = find_required(*this, key);
    if (!value) {
        return value.error();
    }

    auto converted = value.value().as_array();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return *converted;
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
            child->key_flags = 0;
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
