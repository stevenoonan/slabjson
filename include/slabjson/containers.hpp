#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

class Slab;

class Object {
public:
    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] Result<void> add(std::string_view key, Value value) noexcept;
    [[nodiscard]] Result<void> add(
        std::string_view key,
        std::string_view value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, const char* value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, bool value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, int value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, std::int64_t value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, double value) noexcept;
    [[nodiscard]] Result<void> add_null(std::string_view key) noexcept;

    [[nodiscard]] Result<Object> add_object(std::string_view key) noexcept;
    [[nodiscard]] Result<Array> add_array(std::string_view key) noexcept;

    [[nodiscard]] std::optional<Value> find(std::string_view key) const noexcept;
    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    [[nodiscard]] Result<void> remove(std::string_view key) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] Value value() const noexcept;
    operator Value() const noexcept;

private:
    using NodeId = std::uint16_t;

    Object(Slab* slab, NodeId id, std::uint32_t generation) noexcept;

    Slab* slab_{nullptr};
    NodeId id_{0};
    std::uint32_t generation_{0};

    friend class Slab;
    friend class Value;
};

class Array {
public:
    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] Result<void> add(Value value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view value) noexcept;
    [[nodiscard]] Result<void> add(const char* value) noexcept;
    [[nodiscard]] Result<void> add(bool value) noexcept;
    [[nodiscard]] Result<void> add(int value) noexcept;
    [[nodiscard]] Result<void> add(std::int64_t value) noexcept;
    [[nodiscard]] Result<void> add(double value) noexcept;
    [[nodiscard]] Result<void> add_null() noexcept;

    [[nodiscard]] Result<Object> add_object() noexcept;
    [[nodiscard]] Result<Array> add_array() noexcept;

    [[nodiscard]] std::optional<Value> at(std::size_t index) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] Value value() const noexcept;
    operator Value() const noexcept;

private:
    using NodeId = std::uint16_t;

    Array(Slab* slab, NodeId id, std::uint32_t generation) noexcept;

    Slab* slab_{nullptr};
    NodeId id_{0};
    std::uint32_t generation_{0};

    friend class Slab;
    friend class Value;
};

} // namespace slabjson
