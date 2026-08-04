#include <slabjson/slab.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include <slabjson/detail/utf8.hpp>

namespace slabjson {

Slab::Slab(std::span<std::byte> storage) noexcept
{
    initialize(storage);
}

void Slab::initialize(std::span<std::byte> storage) noexcept
{
    storage_ = {};
    string_bytes_used_ = 0;
    node_count_ = 0;
    error_ = {};
    valid_ = false;

    if (storage.empty()) {
        return;
    }

    void* aligned_start = storage.data();
    std::size_t available = storage.size();
    if (std::align(alignof(Node), sizeof(Node), aligned_start, available) == nullptr) {
        return;
    }

    available = std::min(available, kMaxStorageBytes);
    if (available < sizeof(Node)) {
        return;
    }

    storage_ = std::span<std::byte>{
        static_cast<std::byte*>(aligned_start),
        available,
    };
    valid_ = true;
}

bool Slab::valid() const noexcept
{
    return valid_;
}

Result<void> Slab::status() const noexcept
{
    if (error_.code != ErrorCode::Ok) {
        return error_;
    }
    return {};
}

void Slab::clear_error() noexcept
{
    error_ = {};
}

void Slab::reset() noexcept
{
    string_bytes_used_ = 0;
    node_count_ = 0;
    clear_error();
    ++generation_;
    if (generation_ == 0) {
        generation_ = 1;
    }
}

std::size_t Slab::used_bytes() const noexcept
{
    return static_cast<std::size_t>(node_count_) * sizeof(Node) + string_bytes_used_;
}

std::size_t Slab::capacity_bytes() const noexcept
{
    return storage_.size();
}

std::size_t Slab::remaining_bytes() const noexcept
{
    return capacity_bytes() - used_bytes();
}

Result<Value> Slab::make_null() noexcept
{
    auto node_result = allocate_node(ValueType::Null);
    if (!node_result) {
        return node_result.error();
    }
    return make_value(node_result.value());
}

Result<Value> Slab::make_bool(bool value) noexcept
{
    auto node_result = allocate_node(ValueType::Bool);
    if (!node_result) {
        return node_result.error();
    }

    const NodeId id = node_result.value();
    node_for(id, generation_)->payload.bool_value = value;
    return make_value(id);
}

Result<Value> Slab::make_number(double value) noexcept
{
    if (!std::isfinite(value)) {
        return Error{ErrorCode::NonFiniteNumber, 0};
    }

    auto node_result = allocate_node(ValueType::Number);
    if (!node_result) {
        return node_result.error();
    }

    const NodeId id = node_result.value();
    Node* node = node_for(id, generation_);
    set_node_number_kind(*node, NumberKind::FloatingPoint);
    node->payload.floating_point = value;
    return make_value(id);
}

Result<Value> Slab::make_number(std::int64_t value) noexcept
{
    auto node_result = allocate_node(ValueType::Number);
    if (!node_result) {
        return node_result.error();
    }

    const NodeId id = node_result.value();
    Node* node = node_for(id, generation_);
    set_node_number_kind(*node, NumberKind::SignedInteger);
    node->payload.signed_integer = value;
    return make_value(id);
}

Result<Value> Slab::make_number(std::uint64_t value) noexcept
{
    auto node_result = allocate_node(ValueType::Number);
    if (!node_result) {
        return node_result.error();
    }

    const NodeId id = node_result.value();
    Node* node = node_for(id, generation_);
    set_node_number_kind(*node, NumberKind::UnsignedInteger);
    node->payload.unsigned_integer = value;
    return make_value(id);
}

Result<Value> Slab::make_string(std::string_view value) noexcept
{
    if (!valid_) {
        return Error{ErrorCode::InvalidArgument, 0};
    }
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        return Error{ErrorCode::StringCapacityExceeded, 0};
    }
    if (auto invalid = detail::invalid_utf8_offset(value)) {
        return Error{ErrorCode::InvalidUtf8, *invalid};
    }
    if (!can_allocate_node()) {
        return Error{ErrorCode::OutOfMemory, 0};
    }

    const std::size_t node_bytes_after =
        (static_cast<std::size_t>(node_count_) + 1) * sizeof(Node);
    const std::size_t free_after_node =
        storage_.size() - string_bytes_used_ - node_bytes_after;
    if (value.size() > free_after_node) {
        return Error{ErrorCode::StringCapacityExceeded, 0};
    }

    auto node_result = allocate_node(ValueType::String);
    if (!node_result) {
        return node_result.error();
    }
    auto string_result = copy_string_trusted(value);
    if (!string_result) {
        return Error{ErrorCode::InternalError, 0};
    }

    const NodeId id = node_result.value();
    Node* node = node_for(id, generation_);
    node->payload.string_value = string_result.value();
    node->value_flags = string_flags(value);
    return make_value(id);
}

Result<Object> Slab::make_object() noexcept
{
    auto node_result = allocate_node(ValueType::Object);
    if (!node_result) {
        return node_result.error();
    }
    return make_object_handle(node_result.value());
}

Result<Array> Slab::make_array() noexcept
{
    auto node_result = allocate_node(ValueType::Array);
    if (!node_result) {
        return node_result.error();
    }
    return make_array_handle(node_result.value());
}

Result<Slab::NodeId> Slab::allocate_node(ValueType type) noexcept
{
    auto allocation = allocate_node_with_pointer(type);
    if (!allocation) {
        return allocation.error();
    }
    return allocation.value().id;
}

Result<Slab::NodeAllocation> Slab::allocate_node_with_pointer(
    ValueType type) noexcept
{
    if (!valid_) {
        return Error{ErrorCode::InvalidArgument, 0};
    }
    if (!can_allocate_node()) {
        return Error{ErrorCode::OutOfMemory, 0};
    }

    const NodeId id = node_count_;
    auto* location = storage_.data() + static_cast<std::size_t>(id) * sizeof(Node);
    auto* node = ::new (static_cast<void*>(location)) Node{};
    node->type = type;
    ++node_count_;
    return NodeAllocation{id, node};
}

Result<Slab::StoredString> Slab::store_string_with_flags(
    std::string_view value) noexcept
{
    if (auto invalid = detail::invalid_utf8_offset(value)) {
        return Error{ErrorCode::InvalidUtf8, *invalid};
    }

    auto copied = copy_string_trusted(value);
    if (!copied) {
        return copied.error();
    }
    return StoredString{copied.value(), string_flags(value)};
}

Result<Slab::StringRef> Slab::copy_string_trusted(
    std::string_view value) noexcept
{
    auto allocation = allocate_string(value.size());
    if (!allocation) {
        return allocation.error();
    }
    const StringRef ref = allocation.value();
    if (!value.empty()) {
        std::memmove(storage_.data() + ref.offset, value.data(), value.size());
    }
    return ref;
}

Result<Slab::StringRef> Slab::allocate_string(std::size_t length) noexcept
{
    if (!valid_) {
        return Error{ErrorCode::InvalidArgument, 0};
    }
    if (length > std::numeric_limits<std::uint16_t>::max()) {
        return Error{ErrorCode::StringCapacityExceeded, 0};
    }

    const std::size_t node_bytes =
        static_cast<std::size_t>(node_count_) * sizeof(Node);
    const std::size_t free_bytes =
        storage_.size() - string_bytes_used_ - node_bytes;
    if (length > free_bytes) {
        return Error{ErrorCode::StringCapacityExceeded, 0};
    }

    const std::size_t offset =
        storage_.size() - string_bytes_used_ - length;
    string_bytes_used_ += length;
    return StringRef{
        static_cast<std::uint16_t>(offset),
        static_cast<std::uint16_t>(length),
    };
}

bool Slab::can_allocate_node() const noexcept
{
    if (!valid_) {
        return false;
    }

    const std::size_t node_bytes_after =
        (static_cast<std::size_t>(node_count_) + 1) * sizeof(Node);
    return node_bytes_after <= storage_.size() - string_bytes_used_;
}

std::uint8_t Slab::string_flags(std::string_view value) noexcept
{
    static constexpr std::uint8_t kNeedsJsonEscape = 0x01;

    for (char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte == '"' || byte == '\\' || byte < 0x20) {
            return kNeedsJsonEscape;
        }
    }
    return 0;
}

bool Slab::string_needs_json_escape(std::uint8_t flags) noexcept
{
    static constexpr std::uint8_t kNeedsJsonEscape = 0x01;
    return (flags & kNeedsJsonEscape) != 0;
}

NumberKind Slab::node_number_kind(const Node& node) noexcept
{
    return static_cast<NumberKind>(node.aux);
}

void Slab::set_node_number_kind(Node& node, NumberKind kind) noexcept
{
    node.aux = static_cast<std::uint8_t>(kind);
}

Result<void> Slab::validate_attachment(
    NodeId parent_id,
    std::uint32_t generation,
    Value child,
    ValueType parent_type) const noexcept
{
    const Node* parent = node_for(parent_id, generation);
    if (parent == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    if (parent->type != parent_type) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    if (child.slab_ == nullptr
        || child.slab_ != this
        || child.generation_ != generation_) {
        return child.slab_ == nullptr || child.slab_ == this
            ? Error{ErrorCode::InvalidHandle, 0}
            : Error{ErrorCode::CrossSlab, 0};
    }

    const Node* child_node = node_for(child.id_, child.generation_);
    if (child_node == nullptr) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    if (child_node->parent != kInvalidNodeId) {
        return Error{ErrorCode::AlreadyAttached, 0};
    }

    NodeId ancestor = parent_id;
    while (ancestor != kInvalidNodeId) {
        if (ancestor == child.id_) {
            return Error{ErrorCode::CycleDetected, 0};
        }
        const Node* ancestor_node = node_for(ancestor, generation);
        if (ancestor_node == nullptr) {
            return Error{ErrorCode::InternalError, 0};
        }
        ancestor = ancestor_node->parent;
    }

    return {};
}

void Slab::append_child_unchecked(NodeId parent_id, NodeId child_id) noexcept
{
    Node* parent = node_for(parent_id, generation_);
    Node* child = node_for(child_id, generation_);

    child->parent = parent_id;
    child->next_sibling = kInvalidNodeId;
    if (parent->last_child == kInvalidNodeId) {
        parent->first_child = child_id;
    } else {
        node_for(parent->last_child, generation_)->next_sibling = child_id;
    }
    parent->last_child = child_id;
}

Slab::Checkpoint Slab::checkpoint() const noexcept
{
    return Checkpoint{string_bytes_used_, node_count_};
}

void Slab::rollback(Checkpoint checkpoint) noexcept
{
    string_bytes_used_ = checkpoint.string_bytes_used;
    node_count_ = checkpoint.node_count;
}

const Slab::Node* Slab::node_for(NodeId id, std::uint32_t generation) const noexcept
{
    if (!valid_ || generation != generation_ || id >= node_count_) {
        return nullptr;
    }

    const auto* location =
        storage_.data() + static_cast<std::size_t>(id) * sizeof(Node);
    return reinterpret_cast<const Node*>(location);
}

Slab::Node* Slab::node_for(NodeId id, std::uint32_t generation) noexcept
{
    return const_cast<Node*>(
        static_cast<const Slab*>(this)->node_for(id, generation));
}

const Slab::Node* Slab::node_at_unchecked(NodeId id) const noexcept
{
    const auto* location =
        storage_.data() + static_cast<std::size_t>(id) * sizeof(Node);
    return reinterpret_cast<const Node*>(location);
}

Slab::Node* Slab::node_at_unchecked(NodeId id) noexcept
{
    return const_cast<Node*>(
        static_cast<const Slab*>(this)->node_at_unchecked(id));
}

std::string_view Slab::view_string(StringRef ref) const noexcept
{
    if (ref.offset > storage_.size()
        || ref.length > storage_.size() - ref.offset) {
        return {};
    }

    return std::string_view{
        reinterpret_cast<const char*>(storage_.data() + ref.offset),
        ref.length,
    };
}

Value Slab::make_value(NodeId id) noexcept
{
    return Value{this, id, generation_};
}

Object Slab::make_object_handle(NodeId id) noexcept
{
    return Object{this, id, generation_};
}

Array Slab::make_array_handle(NodeId id) noexcept
{
    return Array{this, id, generation_};
}

Error Slab::record_error(Error error) noexcept
{
    if (error_.code == ErrorCode::Ok) {
        error_ = error;
    }
    return error_;
}

} // namespace slabjson
