#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace slabjson {

class Array;
class Object;
class Slab;

enum class ValueType : std::uint8_t {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] ValueType type() const noexcept;

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;

    [[nodiscard]] std::optional<bool> as_bool() const noexcept;
    [[nodiscard]] std::optional<double> as_number() const noexcept;
    [[nodiscard]] std::optional<std::string_view> as_string() const noexcept;
    [[nodiscard]] std::optional<Array> as_array() const noexcept;
    [[nodiscard]] std::optional<Object> as_object() const noexcept;

private:
    using NodeId = std::uint16_t;

    Value(Slab* slab, NodeId id, std::uint32_t generation) noexcept;

    Slab* slab_{nullptr};
    NodeId id_{0};
    std::uint32_t generation_{0};

    friend class Array;
    friend class Object;
    friend class Slab;
};

} // namespace slabjson
