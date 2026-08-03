#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <slabjson/slabjson.hpp>

static_assert(std::forward_iterator<slabjson::Array::iterator>);
static_assert(std::forward_iterator<slabjson::Object::iterator>);
static_assert(std::is_nothrow_move_constructible_v<slabjson::Result<int>>);
static_assert(std::is_nothrow_move_assignable_v<slabjson::Result<int>>);

namespace {

struct ThrowingMovePayload {
    ThrowingMovePayload(ThrowingMovePayload&&) noexcept(false);
};

static_assert(!slabjson::detail::ResultPayload<ThrowingMovePayload>);

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)

struct CopyFailure {
};

class ThrowingCopyPayload {
public:
    explicit ThrowingCopyPayload(int value) noexcept
        : value_(value)
    {
    }

    ThrowingCopyPayload(const ThrowingCopyPayload& other)
        : value_(other.value_)
    {
        throw CopyFailure{};
    }

    ThrowingCopyPayload(ThrowingCopyPayload&& other) noexcept
        : value_(other.value_)
    {
        other.value_ = 0;
    }

    [[nodiscard]] int value() const noexcept
    {
        return value_;
    }

private:
    int value_;
};

#endif

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "line " << line << ": check failed: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void check_serializes(
    slabjson::Value value,
    std::string_view expected,
    int line)
{
    std::array<char, 128> output{};
    auto result = slabjson::serialize(value, output);
    check(static_cast<bool>(result), "serialize", line);
    if (result) {
        check(
            std::string_view{output.data(), result.value()} == expected,
            "serialized value",
            line);
    }
}

#define CHECK_SERIALIZES(value, expected) \
    check_serializes((value), (expected), __LINE__)

template <typename T>
void self_move_assign(T& value)
{
    T* alias = &value;
    value = std::move(*alias);
}

} // namespace

int main()
{
    {
        slabjson::Result<int> value{1};
        slabjson::Result<int> other_value{2};
        slabjson::Result<int> error{
            slabjson::Error{slabjson::ErrorCode::NotFound, 3},
        };
        slabjson::Result<int> other_error{
            slabjson::Error{slabjson::ErrorCode::InvalidUtf8, 4},
        };

        value = other_value;
        CHECK(value && value.value() == 2);
        error = other_error;
        CHECK(!error);
        CHECK(error.error().code == slabjson::ErrorCode::InvalidUtf8);
        CHECK(error.error().offset == 4);

        value = error;
        CHECK(!value);
        CHECK(value.error().code == slabjson::ErrorCode::InvalidUtf8);
        error = slabjson::Result<int>{7};
        CHECK(error && error.value() == 7);

        slabjson::Result<int> moved_value{9};
        error = std::move(moved_value);
        CHECK(error && error.value() == 9);
        slabjson::Result<int> moved_error{
            slabjson::Error{slabjson::ErrorCode::NotFound, 5},
        };
        error = std::move(moved_error);
        CHECK(!error && error.error().offset == 5);

        other_value = other_value;
        CHECK(other_value && other_value.value() == 2);
        self_move_assign(other_value);
        CHECK(other_value && other_value.value() == 2);
    }

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    {
        slabjson::Result<ThrowingCopyPayload> source{
            ThrowingCopyPayload{1},
        };
        slabjson::Result<ThrowingCopyPayload> destination{
            ThrowingCopyPayload{2},
        };

        bool copy_threw = false;
        try {
            destination = source;
        } catch (const CopyFailure&) {
            copy_threw = true;
        }

        CHECK(copy_threw);
        CHECK(destination);
        CHECK(destination.value().value() == 2);
    }
#endif

    {
        slabjson::StaticSlab<4096> slab;
        auto minimum = slab.make_number(
            std::numeric_limits<std::int64_t>::min());
        auto maximum = slab.make_number(
            std::numeric_limits<std::int64_t>::max());
        auto unsigned_maximum = slab.make_number(
            std::numeric_limits<std::uint64_t>::max());
        auto above_double_exact = slab.make_number(
            std::uint64_t{9007199254740993ULL});

        CHECK(minimum && maximum && unsigned_maximum && above_double_exact);
        CHECK(
            minimum.value().number_kind()
            == slabjson::NumberKind::SignedInteger);
        CHECK(
            minimum.value().as_int64()
            == std::numeric_limits<std::int64_t>::min());
        CHECK(!minimum.value().as_uint64());
        CHECK(
            maximum.value().as_int64()
            == std::numeric_limits<std::int64_t>::max());
        CHECK(
            maximum.value().as_uint64()
            == static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()));
        CHECK(
            unsigned_maximum.value().number_kind()
            == slabjson::NumberKind::UnsignedInteger);
        CHECK(
            unsigned_maximum.value().as_uint64()
            == std::numeric_limits<std::uint64_t>::max());
        CHECK(!unsigned_maximum.value().as_int64());

        CHECK_SERIALIZES(minimum.value(), "-9223372036854775808");
        CHECK_SERIALIZES(maximum.value(), "9223372036854775807");
        CHECK_SERIALIZES(unsigned_maximum.value(), "18446744073709551615");
        CHECK_SERIALIZES(above_double_exact.value(), "9007199254740993");
    }

    {
        slabjson::StaticSlab<4096> slab;
        auto array_result = slab.make_array();
        CHECK(array_result);
        auto array = array_result.value();

        signed char signed_char = -1;
        unsigned char unsigned_char = 2;
        short signed_short = -3;
        unsigned short unsigned_short = 4;
        int signed_int = -5;
        unsigned int unsigned_int = 6;
        long signed_long = -7;
        unsigned long unsigned_long = 8;
        long long signed_long_long = -9;
        unsigned long long unsigned_long_long = 10;

        CHECK(array.add(signed_char));
        CHECK(array.add(unsigned_char));
        CHECK(array.add(signed_short));
        CHECK(array.add(unsigned_short));
        CHECK(array.add(signed_int));
        CHECK(array.add(unsigned_int));
        CHECK(array.add(signed_long));
        CHECK(array.add(unsigned_long));
        CHECK(array.add(signed_long_long));
        CHECK(array.add(unsigned_long_long));

        std::size_t index = 0;
        for (slabjson::Value value : array) {
            if (index % 2 == 0) {
                CHECK(
                    value.number_kind()
                    == slabjson::NumberKind::SignedInteger);
            } else {
                CHECK(
                    value.number_kind()
                    == slabjson::NumberKind::UnsignedInteger);
            }
            ++index;
        }
        CHECK(index == 10);

        auto object = slab.make_object().value();
        CHECK(object.add("unsigned", unsigned_int));
        CHECK(object.add("uint64", std::uint64_t{9007199254740993ULL}));
        CHECK(
            object.find("unsigned")->number_kind()
            == slabjson::NumberKind::UnsignedInteger);
        CHECK(object.find("unsigned")->as_int64() == 6);
        CHECK(
            object.find("uint64")->as_uint64()
            == std::uint64_t{9007199254740993ULL});
    }

    {
        slabjson::StaticSlab<4096> slab;
        auto parsed_min = slabjson::parse(slab, "-9223372036854775808");
        auto parsed_max = slabjson::parse(slab, "9223372036854775807");
        auto parsed_unsigned =
            slabjson::parse(slab, "18446744073709551615");
        auto parsed_above_double =
            slabjson::parse(slab, "9007199254740993");
        auto parsed_float = slabjson::parse(slab, "1e0");

        CHECK(parsed_min && parsed_max && parsed_unsigned);
        CHECK(parsed_above_double && parsed_float);
        CHECK(
            parsed_min.value().as_int64()
            == std::numeric_limits<std::int64_t>::min());
        CHECK(
            parsed_max.value().as_int64()
            == std::numeric_limits<std::int64_t>::max());
        CHECK(
            parsed_max.value().as_uint64()
            == static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()));
        CHECK(
            parsed_unsigned.value().as_uint64()
            == std::numeric_limits<std::uint64_t>::max());
        CHECK(
            parsed_above_double.value().as_int64()
            == std::int64_t{9007199254740993LL});
        CHECK(
            parsed_float.value().number_kind()
            == slabjson::NumberKind::FloatingPoint);
        CHECK(!parsed_float.value().as_int64());
        CHECK(!parsed_float.value().as_uint64());

        const std::size_t used_before = slab.used_bytes();
        auto overflow =
            slabjson::parse(slab, "18446744073709551616");
        CHECK(!overflow);
        CHECK(
            overflow.error().code
            == slabjson::ErrorCode::ParseInvalidNumber);
        CHECK(slab.used_bytes() == used_before);
    }

    {
        slabjson::StaticSlab<1024> slab;
        const std::size_t used_before = slab.used_bytes();
        auto nan =
            slab.make_number(std::numeric_limits<double>::quiet_NaN());
        auto infinity =
            slab.make_number(std::numeric_limits<double>::infinity());
        CHECK(!nan && !infinity);
        CHECK(nan.error().code == slabjson::ErrorCode::NonFiniteNumber);
        CHECK(
            infinity.error().code
            == slabjson::ErrorCode::NonFiniteNumber);
        CHECK(slab.used_bytes() == used_before);

        auto array = slab.make_array().value();
        const std::size_t array_used = slab.used_bytes();
        auto add_nan =
            array.add(std::numeric_limits<double>::quiet_NaN());
        CHECK(!add_nan);
        CHECK(
            add_nan.error().code
            == slabjson::ErrorCode::NonFiniteNumber);
        CHECK(slab.used_bytes() == array_used);
    }

    {
        slabjson::StaticSlab<2048> slab;
        const char invalid_bytes[] = {
            static_cast<char>(0xe2),
            '(',
            static_cast<char>(0xa1),
        };
        auto invalid = slab.make_string(
            std::string_view{invalid_bytes, sizeof(invalid_bytes)});
        CHECK(!invalid);
        CHECK(invalid.error().code == slabjson::ErrorCode::InvalidUtf8);
        CHECK(invalid.error().offset == 1);
        CHECK(slab.used_bytes() == 0);

        auto object = slab.make_object().value();
        const std::size_t used_before = slab.used_bytes();
        auto bad_value = object.add(
            "key",
            std::string_view{invalid_bytes, sizeof(invalid_bytes)});
        CHECK(!bad_value);
        CHECK(bad_value.error().code == slabjson::ErrorCode::InvalidUtf8);
        CHECK(bad_value.error().offset == 1);
        CHECK(slab.used_bytes() == used_before);

        auto child = slab.make_null().value();
        const std::size_t child_used = slab.used_bytes();
        auto bad_key = object.add(
            std::string_view{invalid_bytes, sizeof(invalid_bytes)},
            child);
        CHECK(!bad_key);
        CHECK(bad_key.error().code == slabjson::ErrorCode::InvalidUtf8);
        CHECK(bad_key.error().offset == 1);
        CHECK(slab.used_bytes() == child_used);

        const char valid_with_nul[] = {'a', '\0', 'b'};
        auto valid = slab.make_string(
            std::string_view{valid_with_nul, sizeof(valid_with_nul)});
        CHECK(valid);
        CHECK_SERIALIZES(valid.value(), "\"a\\u0000b\"");
    }

    {
        slabjson::StaticSlab<2048> slab;
        auto object = slab.make_object().value();
        CHECK(object.add("first", 1));
        CHECK(object.add("same", 2));
        CHECK(object.add("same", 3));

        std::array<std::string_view, 3> keys{"first", "same", "same"};
        std::array<std::int64_t, 3> values{1, 2, 3};
        std::size_t index = 0;
        for (slabjson::ObjectMember member : object) {
            CHECK(member.key == keys[index]);
            CHECK(member.value.as_int64() == values[index]);
            ++index;
        }
        CHECK(index == 3);

        auto nested = object.add_array("nested").value();
        CHECK(nested.add(true));
        auto last = object.begin();
        ++last;
        ++last;
        ++last;
        CHECK((*last).key == "nested");
        CHECK((*last).value.is_array());
    }

    {
        slabjson::StaticSlab<2048> slab;
        auto parent = slab.make_object().value();
        auto other_parent = slab.make_object().value();
        auto child = slab.make_array().value();
        CHECK(parent.add("child", child));

        auto attached = other_parent.add("child", child);
        CHECK(!attached);
        CHECK(
            attached.error().code
            == slabjson::ErrorCode::AlreadyAttached);

        slabjson::StaticSlab<256> other_slab;
        auto foreign = other_slab.make_null().value();
        auto cross_slab = parent.add("foreign", foreign);
        CHECK(!cross_slab);
        CHECK(
            cross_slab.error().code
            == slabjson::ErrorCode::CrossSlab);

        auto cycle = child.add(parent);
        CHECK(!cycle);
        CHECK(cycle.error().code == slabjson::ErrorCode::CycleDetected);

        auto missing = parent.remove("missing");
        CHECK(!missing);
        CHECK(missing.error().code == slabjson::ErrorCode::NotFound);
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto existing = slab.make_string("existing").value();
        const std::size_t used_before = slab.used_bytes();

        slabjson::ParseOptions options;
        options.max_depth = slabjson::kMaxParserDepth + 1;
        auto result = slabjson::parse(slab, "null", options);
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(slab.used_bytes() == used_before);
        CHECK(existing.valid());
    }

    {
        slabjson::StaticSlab<256> slab;
        auto value = slab.make_null().value();
        auto array = slab.make_array().value();
        CHECK(array.add(value));
        auto iterator = array.begin();
        slab.reset();

        CHECK(value.type() == slabjson::ValueType::Invalid);
        CHECK(!(*iterator).valid());
    }

    return failures == 0 ? 0 : 1;
}
