#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <slabjson/containers.hpp>
#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

namespace detail {
class Serializer;
}

class Slab {
public:
    explicit Slab(std::span<std::byte> storage) noexcept;

    Slab(const Slab&) = delete;
    Slab& operator=(const Slab&) = delete;
    Slab(Slab&&) = delete;
    Slab& operator=(Slab&&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    void reset() noexcept;

    [[nodiscard]] std::size_t used_bytes() const noexcept;
    [[nodiscard]] std::size_t capacity_bytes() const noexcept;
    [[nodiscard]] std::size_t remaining_bytes() const noexcept;

    [[nodiscard]] Result<Value> make_null() noexcept;
    [[nodiscard]] Result<Value> make_bool(bool value) noexcept;
    [[nodiscard]] Result<Value> make_number(double value) noexcept;
    [[nodiscard]] Result<Value> make_string(std::string_view value) noexcept;
    [[nodiscard]] Result<Object> make_object() noexcept;
    [[nodiscard]] Result<Array> make_array() noexcept;

protected:
    Slab() noexcept = default;
    void initialize(std::span<std::byte> storage) noexcept;

private:
    using NodeId = std::uint16_t;

    static constexpr NodeId kInvalidNodeId = UINT16_MAX;
    static constexpr std::size_t kMaxStorageBytes = UINT16_MAX;

    struct StringRef {
        std::uint16_t offset{kInvalidNodeId};
        std::uint16_t length{0};
    };

    union Payload {
        bool bool_value;
        double number_value;
        StringRef string_value;

        constexpr Payload() noexcept
            : number_value(0.0)
        {
        }
    };

    struct Node {
        ValueType type{ValueType::Null};
        NodeId parent{kInvalidNodeId};
        NodeId first_child{kInvalidNodeId};
        NodeId last_child{kInvalidNodeId};
        NodeId next_sibling{kInvalidNodeId};
        StringRef key{};
        Payload payload{};
    };

    struct Checkpoint {
        std::size_t string_bytes_used;
        NodeId node_count;
    };

    [[nodiscard]] Result<NodeId> allocate_node(ValueType type) noexcept;
    [[nodiscard]] Result<StringRef> store_string(std::string_view value) noexcept;
    [[nodiscard]] bool can_allocate_node() const noexcept;
    [[nodiscard]] Result<void> validate_attachment(
        NodeId parent_id,
        std::uint32_t generation,
        Value child,
        ValueType parent_type) const noexcept;
    void append_child_unchecked(NodeId parent_id, NodeId child_id) noexcept;
    [[nodiscard]] Checkpoint checkpoint() const noexcept;
    void rollback(Checkpoint checkpoint) noexcept;
    [[nodiscard]] const Node* node_for(NodeId id, std::uint32_t generation) const noexcept;
    [[nodiscard]] Node* node_for(NodeId id, std::uint32_t generation) noexcept;
    [[nodiscard]] std::string_view view_string(StringRef ref) const noexcept;
    [[nodiscard]] Value make_value(NodeId id) noexcept;
    [[nodiscard]] Object make_object_handle(NodeId id) noexcept;
    [[nodiscard]] Array make_array_handle(NodeId id) noexcept;

    std::span<std::byte> storage_{};
    std::size_t string_bytes_used_{0};
    NodeId node_count_{0};
    std::uint32_t generation_{1};
    bool valid_{false};

    friend class Array;
    friend class detail::Serializer;
    friend class Object;
    friend class Value;
};

} // namespace slabjson
