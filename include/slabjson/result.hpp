#pragma once

#include <cassert>
#include <new>
#include <utility>

#include <slabjson/error.hpp>

namespace slabjson {

template <typename T>
class Result {
public:
    Result(const T& value)
        : has_value_(true)
    {
        ::new (static_cast<void*>(&storage_.value)) T(value);
    }

    Result(T&& value) noexcept(noexcept(T(std::move(value))))
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
        : has_value_(other.has_value_)
    {
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value)) T(other.storage_.value);
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
    }

    Result(Result&& other) noexcept(noexcept(T(std::move(other.storage_.value))))
        : has_value_(other.has_value_)
    {
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value)) T(std::move(other.storage_.value));
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
    }

    Result& operator=(const Result& other)
    {
        if (this == &other) {
            return *this;
        }

        destroy_active();
        has_value_ = other.has_value_;
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value)) T(other.storage_.value);
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
        return *this;
    }

    Result& operator=(Result&& other)
        noexcept(noexcept(T(std::move(other.storage_.value))))
    {
        if (this == &other) {
            return *this;
        }

        destroy_active();
        has_value_ = other.has_value_;
        if (has_value_) {
            ::new (static_cast<void*>(&storage_.value))
                T(std::move(other.storage_.value));
        } else {
            ::new (static_cast<void*>(&storage_.error)) Error(other.storage_.error);
        }
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
