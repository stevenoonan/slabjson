#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string_view>

#include <slabjson/slabjson.hpp>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "line " << line << ": check failed: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

} // namespace

int main()
{
    slabjson::StaticSlab<1024> slab;

    auto null_result = slab.make_null();
    CHECK(null_result);
    auto null_value = null_result.value();
    CHECK(null_value.valid());
    CHECK(null_value.type() == slabjson::ValueType::Null);
    CHECK(null_value.is_null());
    CHECK(!null_value.as_bool());

    auto bool_result = slab.make_bool(true);
    CHECK(bool_result);
    auto bool_value = bool_result.value();
    CHECK(bool_value.type() == slabjson::ValueType::Bool);
    CHECK(bool_value.is_bool());
    CHECK(bool_value.as_bool().value_or(false));
    CHECK(!bool_value.as_number());

    auto number_result = slab.make_number(-12.5);
    CHECK(number_result);
    auto number_value = number_result.value();
    CHECK(number_value.type() == slabjson::ValueType::Number);
    CHECK(number_value.is_number());
    CHECK(std::abs(number_value.as_number().value_or(0.0) + 12.5) < 0.000001);
    CHECK(!number_value.as_string());

    std::array<char, 6> source{'h', 'e', 'l', 'l', 'o', '\0'};
    auto string_result = slab.make_string(std::string_view{source.data(), 5});
    CHECK(string_result);
    auto string_value = string_result.value();
    source[0] = 'j';
    CHECK(string_value.type() == slabjson::ValueType::String);
    CHECK(string_value.is_string());
    CHECK(string_value.as_string().value_or("") == "hello");
    CHECK(!string_value.as_bool());

    auto empty_result = slab.make_string("");
    CHECK(empty_result);
    CHECK(empty_result.value().as_string().value_or("not empty").empty());

    slab.reset();
    CHECK(!null_value.valid());
    CHECK(!null_value.is_null());
    CHECK(!string_value.as_string());

    auto replacement = slab.make_bool(false);
    CHECK(replacement);
    CHECK(replacement.value().valid());
    CHECK(!null_value.valid());

    {
        slabjson::StaticSlab<128> small_slab;
        std::array<char, 128> large_string{};
        auto too_large = small_slab.make_string(
            std::string_view{large_string.data(), large_string.size()});
        CHECK(!too_large);
        CHECK(too_large.error().code == slabjson::ErrorCode::StringCapacityExceeded);
        CHECK(small_slab.used_bytes() == 0);
    }

    return failures == 0 ? 0 : 1;
}
