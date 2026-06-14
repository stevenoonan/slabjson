#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <type_traits>

#include <slabjson/result.hpp>
#include <slabjson/value.hpp>

namespace slabjson {

class Slab;

struct ObjectMember {
    std::string_view key;
    Value value;
};

class ObjectIterator {
public:
    using value_type = ObjectMember;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using reference = ObjectMember;
    using pointer = void;

    ObjectIterator() noexcept = default;

    [[nodiscard]] ObjectMember operator*() const noexcept;
    ObjectIterator& operator++() noexcept;
    ObjectIterator operator++(int) noexcept;

    friend bool operator==(
        const ObjectIterator&,
        const ObjectIterator&) noexcept = default;

private:
    using NodeId = std::uint16_t;

    ObjectIterator(
        Slab* slab,
        NodeId id,
        std::uint32_t generation) noexcept;

    Slab* slab_{nullptr};
    NodeId id_{UINT16_MAX};
    std::uint32_t generation_{0};

    friend class Object;
};

class ArrayIterator {
public:
    using value_type = Value;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using reference = Value;
    using pointer = void;

    ArrayIterator() noexcept = default;

    [[nodiscard]] Value operator*() const noexcept;
    ArrayIterator& operator++() noexcept;
    ArrayIterator operator++(int) noexcept;

    friend bool operator==(
        const ArrayIterator&,
        const ArrayIterator&) noexcept = default;

private:
    using NodeId = std::uint16_t;

    ArrayIterator(
        Slab* slab,
        NodeId id,
        std::uint32_t generation) noexcept;

    Slab* slab_{nullptr};
    NodeId id_{UINT16_MAX};
    std::uint32_t generation_{0};

    friend class Array;
};

class Object {
public:
    using iterator = ObjectIterator;
    using const_iterator = ObjectIterator;

    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] Result<void> add(std::string_view key, Value value) noexcept;
    [[nodiscard]] Result<void> add(
        std::string_view key,
        std::string_view value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, const char* value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, bool value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, std::int64_t value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, std::uint64_t value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view key, double value) noexcept;

    template <std::signed_integral T>
        requires (!std::same_as<std::remove_cv_t<T>, bool>
            && !std::same_as<std::remove_cv_t<T>, std::int64_t>)
    [[nodiscard]] Result<void> add(std::string_view key, T value) noexcept
    {
        return add(key, static_cast<std::int64_t>(value));
    }

    template <std::unsigned_integral T>
        requires (!std::same_as<std::remove_cv_t<T>, bool>
            && !std::same_as<std::remove_cv_t<T>, std::uint64_t>)
    [[nodiscard]] Result<void> add(std::string_view key, T value) noexcept
    {
        return add(key, static_cast<std::uint64_t>(value));
    }

    [[nodiscard]] Result<void> add_null(std::string_view key) noexcept;

    [[nodiscard]] Result<Object> add_object(std::string_view key) noexcept;
    [[nodiscard]] Result<Array> add_array(std::string_view key) noexcept;

    [[nodiscard]] std::optional<Value> find(std::string_view key) const noexcept;
    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    [[nodiscard]] Result<void> remove(std::string_view key) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] iterator begin() const noexcept;
    [[nodiscard]] iterator end() const noexcept;

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
    using iterator = ArrayIterator;
    using const_iterator = ArrayIterator;

    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] Result<void> add(Value value) noexcept;
    [[nodiscard]] Result<void> add(std::string_view value) noexcept;
    [[nodiscard]] Result<void> add(const char* value) noexcept;
    [[nodiscard]] Result<void> add(bool value) noexcept;
    [[nodiscard]] Result<void> add(std::int64_t value) noexcept;
    [[nodiscard]] Result<void> add(std::uint64_t value) noexcept;
    [[nodiscard]] Result<void> add(double value) noexcept;

    template <std::signed_integral T>
        requires (!std::same_as<std::remove_cv_t<T>, bool>
            && !std::same_as<std::remove_cv_t<T>, std::int64_t>)
    [[nodiscard]] Result<void> add(T value) noexcept
    {
        return add(static_cast<std::int64_t>(value));
    }

    template <std::unsigned_integral T>
        requires (!std::same_as<std::remove_cv_t<T>, bool>
            && !std::same_as<std::remove_cv_t<T>, std::uint64_t>)
    [[nodiscard]] Result<void> add(T value) noexcept
    {
        return add(static_cast<std::uint64_t>(value));
    }

    [[nodiscard]] Result<void> add_null() noexcept;

    [[nodiscard]] Result<Object> add_object() noexcept;
    [[nodiscard]] Result<Array> add_array() noexcept;

    [[nodiscard]] std::optional<Value> at(std::size_t index) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] iterator begin() const noexcept;
    [[nodiscard]] iterator end() const noexcept;

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
