#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <span>
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

void check_error(
    std::string_view input,
    slabjson::ErrorCode expected_code,
    std::size_t expected_offset,
    int line)
{
    slabjson::StaticSlab<2048> slab;
    auto result = slabjson::parse(slab, input);
    check(!result, "parse should fail", line);
    if (result) {
        return;
    }
    check(result.error().code == expected_code, "error code", line);
    check(result.error().offset == expected_offset, "error offset", line);
    check(slab.used_bytes() == 0, "failed parse rollback", line);
}

#define CHECK_ERROR(input, code, offset) \
    check_error((input), (code), (offset), __LINE__)

void check_round_trip(std::string_view input, int line)
{
    slabjson::StaticSlab<4096> slab;
    auto parsed = slabjson::parse(slab, input);
    check(static_cast<bool>(parsed), "parse round trip input", line);
    if (!parsed) {
        return;
    }

    std::array<char, 1024> output{};
    auto serialized = slabjson::serialize(parsed.value(), output);
    check(static_cast<bool>(serialized), "serialize parsed value", line);
    if (!serialized) {
        return;
    }

    slabjson::StaticSlab<4096> second_slab;
    auto reparsed = slabjson::parse(
        second_slab,
        std::string_view{output.data(), serialized.value()});
    check(static_cast<bool>(reparsed), "reparse serialized value", line);
}

#define CHECK_ROUND_TRIP(input) check_round_trip((input), __LINE__)

} // namespace

int main()
{
    {
        slabjson::StaticSlab<4096> slab;

        auto null_value = slabjson::parse(slab, "null");
        CHECK(null_value);
        CHECK(null_value.value().is_null());

        auto true_value = slabjson::parse(slab, "true");
        CHECK(true_value);
        CHECK(true_value.value().as_bool().value_or(false));

        auto false_value = slabjson::parse(slab, "false");
        CHECK(false_value);
        CHECK(!false_value.value().as_bool().value_or(true));

        auto integer_value = slabjson::parse(slab, "123");
        CHECK(integer_value);
        CHECK(integer_value.value().as_number().value_or(0.0) == 123.0);

        auto number_value = slabjson::parse(slab, "-12.5e+2");
        CHECK(number_value);
        CHECK(number_value.value().as_number().value_or(0.0) == -1250.0);

        std::array<char, 7> source{'"', 'h', 'e', 'l', 'l', 'o', '"'};
        auto string_value = slabjson::parse(
            slab,
            std::string_view{source.data(), source.size()});
        CHECK(string_value);
        source[1] = 'x';
        CHECK(string_value.value().as_string().value_or("") == "hello");

        auto empty_array = slabjson::parse(slab, "[]");
        CHECK(empty_array);
        CHECK(empty_array.value().as_array()->empty());

        auto empty_object = slabjson::parse(slab, "{}");
        CHECK(empty_object);
        CHECK(empty_object.value().as_object()->empty());
    }

    {
        slabjson::StaticSlab<4096> slab;
        auto parsed = slabjson::parse(
            slab,
            " { \"a\" : 1, \"b\" : true, \"c\" : null, "
            "\"nested\":{\"x\":-2.5},"
            "\"array\":[true,false,\"text\",3e2] } \n");
        CHECK(parsed);

        auto root = parsed.value().as_object();
        CHECK(root);
        CHECK(root->size() == 5);
        CHECK(root->find("a")->as_number().value_or(0.0) == 1.0);
        CHECK(root->find("b")->as_bool().value_or(false));
        CHECK(root->find("c")->is_null());
        CHECK(root->find("nested")->as_object()->find("x")
                  ->as_number().value_or(0.0)
            == -2.5);

        auto array = root->find("array")->as_array();
        CHECK(array);
        CHECK(array->size() == 4);
        CHECK(array->at(0)->as_bool().value_or(false));
        CHECK(!array->at(1)->as_bool().value_or(true));
        CHECK(array->at(2)->as_string().value_or("") == "text");
        CHECK(array->at(3)->as_number().value_or(0.0) == 300.0);
    }

    {
        slabjson::StaticSlab<4096> slab;
        auto parsed = slabjson::parse(
            slab,
            "{\"a\\\"b\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\","
            "\"unicode\":\"A\\u00e9\\u20ac\\uD83D\\uDE00\","
            "\"raw\":\"\xc3\xa9\"}");
        CHECK(parsed);

        auto root = parsed.value().as_object();
        CHECK(root);
        CHECK((root->find("a\"b")->as_string().value_or("")
            == std::string_view{"\"\\/\b\f\n\r\t", 8}));
        CHECK(root->find("unicode")->as_string().value_or("")
            == "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
        CHECK(root->find("raw")->as_string().value_or("") == "\xc3\xa9");

        std::array<char, 256> output{};
        auto serialized = slabjson::serialize(parsed.value(), output);
        CHECK(serialized);
        CHECK((std::string_view{output.data(), serialized.value()}
            == "{\"a\\\"b\":\"\\\"\\\\/\\b\\f\\n\\r\\t\","
               "\"unicode\":\"A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\","
               "\"raw\":\"\xc3\xa9\"}"));
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto parsed = slabjson::parse(
            slab,
            "{\"same\":1,\"same\":2}");
        CHECK(parsed);
        auto object = parsed.value().as_object();
        CHECK(object);
        CHECK(object->size() == 2);
        CHECK(object->find("same")->as_number().value_or(0.0) == 1.0);
    }

    CHECK_ROUND_TRIP("null");
    CHECK_ROUND_TRIP("[1,2,3]");
    CHECK_ROUND_TRIP("{\"a\":1}");
    CHECK_ROUND_TRIP(
        "{\"nested\":{\"x\":1},\"array\":[true,false]}");

    CHECK_ERROR("", slabjson::ErrorCode::ParseUnexpectedEnd, 0);
    CHECK_ERROR("   ", slabjson::ErrorCode::ParseUnexpectedEnd, 3);
    CHECK_ERROR("{", slabjson::ErrorCode::ParseUnexpectedEnd, 1);
    CHECK_ERROR("[1,2,", slabjson::ErrorCode::ParseUnexpectedEnd, 5);
    CHECK_ERROR(
        "{\"a\"}",
        slabjson::ErrorCode::ParseUnexpectedToken,
        4);
    CHECK_ERROR(
        "{\"a\":}",
        slabjson::ErrorCode::ParseUnexpectedToken,
        5);
    CHECK_ERROR(
        "\"unterminated",
        slabjson::ErrorCode::ParseUnexpectedEnd,
        13);
    CHECK_ERROR(
        "\"\\q\"",
        slabjson::ErrorCode::ParseInvalidEscape,
        2);
    CHECK_ERROR(
        "\"line\nbreak\"",
        slabjson::ErrorCode::ParseInvalidString,
        5);
    CHECK_ERROR(
        "01",
        slabjson::ErrorCode::ParseInvalidNumber,
        1);
    CHECK_ERROR(
        "1.",
        slabjson::ErrorCode::ParseInvalidNumber,
        2);
    CHECK_ERROR(
        "1e+",
        slabjson::ErrorCode::ParseInvalidNumber,
        3);
    CHECK_ERROR(
        "1e9999",
        slabjson::ErrorCode::ParseInvalidNumber,
        0);
    CHECK_ERROR(
        "true x",
        slabjson::ErrorCode::ParseTrailingCharacters,
        5);
    CHECK_ERROR(
        "[1,]",
        slabjson::ErrorCode::ParseUnexpectedToken,
        3);
    CHECK_ERROR(
        "{\"a\":1,}",
        slabjson::ErrorCode::ParseUnexpectedToken,
        7);
    CHECK_ERROR(
        "\"\\u12x4\"",
        slabjson::ErrorCode::ParseInvalidUnicodeEscape,
        5);
    CHECK_ERROR(
        "\"\\uD800\"",
        slabjson::ErrorCode::ParseInvalidUnicodeEscape,
        1);
    CHECK_ERROR(
        "\"\\uDC00\"",
        slabjson::ErrorCode::ParseInvalidUnicodeEscape,
        1);
    CHECK_ERROR(
        "\"\\uD800\\u0041\"",
        slabjson::ErrorCode::ParseInvalidUnicodeEscape,
        7);

    {
        const char invalid_utf8[] = {
            '"',
            static_cast<char>(0xc0),
            static_cast<char>(0x80),
            '"',
        };
        CHECK_ERROR(
            (std::string_view{invalid_utf8, sizeof(invalid_utf8)}),
            slabjson::ErrorCode::ParseInvalidString,
            1);
    }

    {
        slabjson::StaticSlab<1024> slab;
        slabjson::ParseOptions options;
        options.allow_trailing_whitespace = false;
        auto result = slabjson::parse(slab, "true ", options);
        CHECK(!result);
        CHECK(
            result.error().code
            == slabjson::ErrorCode::ParseTrailingCharacters);
        CHECK(result.error().offset == 4);
        CHECK(slab.used_bytes() == 0);
    }

    {
        slabjson::StaticSlab<1024> slab;
        slabjson::ParseOptions options;
        options.max_depth = 2;

        auto allowed = slabjson::parse(slab, "[[]]", options);
        CHECK(allowed);

        const std::size_t used_before = slab.used_bytes();
        auto too_deep = slabjson::parse(slab, "[[[]]]", options);
        CHECK(!too_deep);
        CHECK(
            too_deep.error().code
            == slabjson::ErrorCode::ParserDepthExceeded);
        CHECK(too_deep.error().offset == 2);
        CHECK(slab.used_bytes() == used_before);
    }

    {
        slabjson::StaticSlab<1024> slab;
        slabjson::ParseOptions options;
        options.max_depth = 0;

        auto primitive = slabjson::parse(slab, "1", options);
        CHECK(primitive);

        auto container = slabjson::parse(slab, "[]", options);
        CHECK(!container);
        CHECK(
            container.error().code
            == slabjson::ErrorCode::ParserDepthExceeded);
        CHECK(container.error().offset == 0);
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto existing_result = slab.make_string("existing");
        CHECK(existing_result);
        auto existing = existing_result.value();
        const std::size_t used_before = slab.used_bytes();

        auto failed = slabjson::parse(
            slab,
            "{\"a\":1,\"b\":[true,}");
        CHECK(!failed);
        CHECK(slab.used_bytes() == used_before);
        CHECK(existing.valid());
        CHECK(existing.as_string().value_or("") == "existing");

        auto success = slabjson::parse(slab, "{\"ok\":true}");
        CHECK(success);
        CHECK(existing.valid());
    }

    {
        slabjson::StaticSlab<128> slab;
        auto result = slabjson::parse(
            slab,
            "[null,null,null,null,null,null,null,null]");
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::OutOfMemory);
        CHECK(slab.used_bytes() == 0);
    }

    {
        slabjson::StaticSlab<128> slab;
        std::array<char, 110> input{};
        input.fill('a');
        input.front() = '"';
        input.back() = '"';

        auto result = slabjson::parse(
            slab,
            std::string_view{input.data(), input.size()});
        CHECK(!result);
        CHECK(
            result.error().code
            == slabjson::ErrorCode::StringCapacityExceeded);
        CHECK(result.error().offset == 0);
        CHECK(slab.used_bytes() == 0);
    }

    {
        alignas(std::max_align_t) std::array<std::byte, 1024> storage{};
        constexpr std::string_view json = "{\"value\":1}";
        for (std::size_t index = 0; index < json.size(); ++index) {
            storage[index] = std::byte{
                static_cast<unsigned char>(json[index]),
            };
        }

        slabjson::Slab slab{storage};
        auto input = std::string_view{
            reinterpret_cast<const char*>(storage.data()),
            json.size(),
        };
        auto result = slabjson::parse(slab, input);
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(slab.used_bytes() == 0);
    }

    return failures == 0 ? 0 : 1;
}
