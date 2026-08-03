#pragma once

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

#include <slabjson/error.hpp>

namespace slabjson {

namespace detail {

template <typename T>
concept ResultPayload = std::is_object_v<T>
    && std::is_nothrow_move_constructible_v<T>
    && std::is_nothrow_destructible_v<T>;

} // namespace detail

template <typename T>
class Result {
    static_assert(
        detail::ResultPayload<T>,
        "Result<T> requires an object payload with non-throwing move "
        "construction and destruction");

public:
    Result(const T& value)
        requires (std::is_copy_constructible_v<T>
            && std::is_nothrow_destructible_v<T>)
        : has_value_(true)
    {
        ::new (static_cast<void*>(&storage_.value)) T(value);
    }

    Result(T&& value) noexcept
        requires detail::ResultPayload<T>
        : has_value_(true)
    {
        ::new (static_cast<void*>(&storage_.value)) T(std::move(value));
    }

    Result(Error error) noexcept
        : has_value_(false)
    {
        ::new (static_cast<void*>(&storage_.error)) Error(error);
    }

    Result(const Result& other)
        requires (std::is_copy_constructible_v<T>
            && std::is_nothrow_destructible_v<T>)
        : has_value_(other.has_value_)
    {
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value)) T(other.storage_.value);
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
    }

    Result(Result&& other) noexcept
        requires detail::ResultPayload<T>
        : has_value_(other.has_value_)
    {
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value)) T(std::move(other.storage_.value));
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
    }

    Result& operator=(const Result& other)
        requires (std::is_copy_constructible_v<T>
            && detail::ResultPayload<T>)
    {
        if (this == &other) {
            return *this;
        }

        Result replacement{other};
        replace_from(std::move(replacement));
        return *this;
    }

    Result& operator=(Result&& other) noexcept
        requires detail::ResultPayload<T>
    {
        if (this == &other) {
            return *this;
        }

        replace_from(std::move(other));
        return *this;
    }

    ~Result()
    {
        destroy_active();
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return has_value_;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    T& value() & noexcept
    {
        assert(has_value_);
        return storage_.value;
    }

    const T& value() const& noexcept
    {
        assert(has_value_);
        return storage_.value;
    }

    T&& value() && noexcept
    {
        assert(has_value_);
        return std::move(storage_.value);
    }

    Error error() const noexcept
    {
        assert(!has_value_);
        return storage_.error;
    }

private:
    void replace_from(Result&& replacement) noexcept
    {
        destroy_active();
        has_value_ = replacement.has_value_;
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value))
                T(std::move(replacement.storage_.value));
        } else {
            ::new (static_cast<void*>(&storage_.error))
                Error(replacement.storage_.error);
        }
    }

    void destroy_active() noexcept
    {
        if (has_value_) {
            storage_.value.~T();
        } else {
            storage_.error.~Error();
        }
    }

    union Storage {
        T value;
        Error error;

        Storage() {}
        ~Storage() {}
    } storage_;

    bool has_value_;
};

template <>
class Result<void> {
public:
    Result() noexcept = default;

    Result(Error error) noexcept
        : error_(error)
        , has_value_(false)
    {
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return has_value_;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    void value() const noexcept
    {
        assert(has_value_);
    }

    Error error() const noexcept
    {
        assert(!has_value_);
        return error_;
    }

private:
    Error error_{};
    bool has_value_{true};
};

} // namespace slabjson
