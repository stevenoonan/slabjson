#include <slabjson/value.hpp>

#include <slabjson/containers.hpp>
#include <slabjson/slab.hpp>

namespace slabjson {

Value::Value(Slab* slab, NodeId id, std::uint32_t generation) noexcept
    : slab_(slab)
    , id_(id)
    , generation_(generation)
{
}

bool Value::valid() const noexcept
{
    return slab_ != nullptr && slab_->node_for(id_, generation_) != nullptr;
}

ValueType Value::type() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node == nullptr ? ValueType::Invalid : node->type;
}

bool Value::is_null() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Null;
}

bool Value::is_bool() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Bool;
}

bool Value::is_number() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Number;
}

bool Value::is_string() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::String;
}

bool Value::is_array() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Array;
}

bool Value::is_object() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    return node != nullptr && node->type == ValueType::Object;
}

std::optional<bool> Value::as_bool() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::Bool) {
        return std::nullopt;
    }
    return node->payload.bool_value;
}

std::optional<double> Value::as_number() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::Number) {
        return std::nullopt;
    }
    switch (node->number_kind) {
    case NumberKind::SignedInteger:
        return static_cast<double>(node->payload.signed_integer);
    case NumberKind::UnsignedInteger:
        return static_cast<double>(node->payload.unsigned_integer);
    case NumberKind::FloatingPoint:
        return node->payload.floating_point;
    }
    return std::nullopt;
}

std::optional<NumberKind> Value::number_kind() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::Number) {
        return std::nullopt;
    }
    return node->number_kind;
}

std::optional<std::int64_t> Value::as_int64() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr
        || node->type != ValueType::Number
        || node->number_kind != NumberKind::SignedInteger) {
        return std::nullopt;
    }
    return node->payload.signed_integer;
}

std::optional<std::uint64_t> Value::as_uint64() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr
        || node->type != ValueType::Number
        || node->number_kind != NumberKind::UnsignedInteger) {
        return std::nullopt;
    }
    return node->payload.unsigned_integer;
}

std::optional<std::string_view> Value::as_string() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::String) {
        return std::nullopt;
    }
    return slab_->view_string(node->payload.string_value);
}

std::optional<Array> Value::as_array() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::Array) {
        return std::nullopt;
    }
    return Array{slab_, id_, generation_};
}

std::optional<Object> Value::as_object() const noexcept
{
    const auto* node = slab_ == nullptr
        ? nullptr
        : slab_->node_for(id_, generation_);
    if (node == nullptr || node->type != ValueType::Object) {
        return std::nullopt;
    }
    return Object{slab_, id_, generation_};
}

} // namespace slabjson
