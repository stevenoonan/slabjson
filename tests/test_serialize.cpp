#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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

void check_serializes(
    slabjson::Value value,
    std::string_view expected,
    int line)
{
    std::array<char, 1024> output{};
    auto size_result = slabjson::serialized_size(value);
    check(static_cast<bool>(size_result), "serialized_size(value)", line);
    if (!size_result) {
        return;
    }
    check(size_result.value() == expected.size(), "serialized size", line);

    auto result = slabjson::serialize(value, output);
    check(static_cast<bool>(result), "serialize(value, output)", line);
    if (!result) {
        return;
    }
    check(result.value() == expected.size(), "written size", line);
    check(
        std::string_view{output.data(), result.value()} == expected,
        "serialized text",
        line);
}

#define CHECK_SERIALIZES(value, expected) \
    check_serializes((value), (expected), __LINE__)

} // namespace

int main()
{
    {
        slabjson::StaticSlab<2048> slab;

        auto null_value = slab.make_null();
        auto true_value = slab.make_bool(true);
        auto false_value = slab.make_bool(false);
        auto integer_value = slab.make_number(4120);
        auto number_value = slab.make_number(-12.5);
        auto string_value = slab.make_string("hello");
        auto object_value = slab.make_object();
        auto array_value = slab.make_array();

        CHECK(null_value);
        CHECK(true_value);
        CHECK(false_value);
        CHECK(integer_value);
        CHECK(number_value);
        CHECK(string_value);
        CHECK(object_value);
        CHECK(array_value);

        CHECK_SERIALIZES(null_value.value(), "null");
        CHECK_SERIALIZES(true_value.value(), "true");
        CHECK_SERIALIZES(false_value.value(), "false");
        CHECK_SERIALIZES(integer_value.value(), "4120");
        CHECK_SERIALIZES(number_value.value(), "-12.5");
        CHECK_SERIALIZES(string_value.value(), "\"hello\"");
        CHECK_SERIALIZES(object_value.value(), "{}");
        CHECK_SERIALIZES(array_value.value(), "[]");
    }

    {
        slabjson::StaticSlab<4096> slab;
        auto root_result = slab.make_object();
        CHECK(root_result);
        auto root = root_result.value();

        CHECK(root.add("device_id", "hub-123"));
        CHECK(root.add("battery_mv", 4120));
        CHECK(root.add("connected", true));
        CHECK(root.add_null("fault"));

        auto tags_result = root.add_array("tags");
        CHECK(tags_result);
        auto tags = tags_result.value();
        CHECK(tags.add("hub"));
        CHECK(tags.add("production"));

        auto metadata_result = root.add_object("metadata");
        CHECK(metadata_result);
        auto metadata = metadata_result.value();
        CHECK(metadata.add("version", 2));
        auto flags_result = metadata.add_array("flags");
        CHECK(flags_result);
        CHECK(flags_result.value().add(false));

        CHECK_SERIALIZES(
            root,
            "{\"device_id\":\"hub-123\",\"battery_mv\":4120,"
            "\"connected\":true,\"fault\":null,"
            "\"tags\":[\"hub\",\"production\"],"
            "\"metadata\":{\"version\":2,\"flags\":[false]}}");

        CHECK_SERIALIZES(tags, "[\"hub\",\"production\"]");
    }

    {
        slabjson::StaticSlab<2048> slab;
        auto object_result = slab.make_object();
        CHECK(object_result);
        auto object = object_result.value();

        CHECK(object.add("same", 1));
        CHECK(object.add("same", 2));
        CHECK(object.add("removed", 3));
        CHECK(object.remove("removed"));
        CHECK_SERIALIZES(object, "{\"same\":1,\"same\":2}");
    }

    {
        slabjson::StaticSlab<2048> slab;
        auto object_result = slab.make_object();
        CHECK(object_result);
        auto object = object_result.value();

        const char escaped_value[] = {
            '"',
            '\\',
            '\b',
            '\f',
            '\n',
            '\r',
            '\t',
            '\0',
            '\x01',
            '/',
            static_cast<char>(0xc3),
            static_cast<char>(0xa9),
        };
        CHECK(object.add(
            "a\"b\\c\n",
            std::string_view{escaped_value, sizeof(escaped_value)}));
        CHECK_SERIALIZES(
            object,
            "{\"a\\\"b\\\\c\\n\":"
            "\"\\\"\\\\\\b\\f\\n\\r\\t\\u0000\\u0001/\xc3\xa9\"}");
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto value_result = slab.make_string("exact");
        CHECK(value_result);
        auto value = value_result.value();

        auto size_result = slabjson::serialized_size(value);
        CHECK(size_result);
        CHECK(size_result.value() == 7);

        std::array<char, 8> exact_output;
        exact_output.fill('#');
        auto exact_result = slabjson::serialize(
            value,
            std::span<char>{exact_output}.first(size_result.value()));
        CHECK(exact_result);
        CHECK(exact_result.value() == size_result.value());
        CHECK((std::string_view{exact_output.data(), 7} == "\"exact\""));
        CHECK(exact_output[7] == '#');

        std::array<char, 8> small_output;
        small_output.fill('#');
        auto small_result = slabjson::serialize(
            value,
            std::span<char>{small_output}.first(size_result.value() - 1));
        CHECK(!small_result);
        CHECK(
            small_result.error().code
            == slabjson::ErrorCode::OutputCapacityExceeded);
        for (char character : small_output) {
            CHECK(character == '#');
        }

        auto empty_result = slabjson::serialize(value, std::span<char>{});
        CHECK(!empty_result);
        CHECK(
            empty_result.error().code
            == slabjson::ErrorCode::OutputCapacityExceeded);
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto nan_result =
            slab.make_number(std::numeric_limits<double>::quiet_NaN());
        auto infinity_result =
            slab.make_number(std::numeric_limits<double>::infinity());
        CHECK(nan_result);
        CHECK(infinity_result);

        auto nan_size = slabjson::serialized_size(nan_result.value());
        CHECK(!nan_size);
        CHECK(nan_size.error().code == slabjson::ErrorCode::InvalidArgument);

        std::array<char, 32> output;
        output.fill('#');
        auto infinity =
            slabjson::serialize(infinity_result.value(), output);
        CHECK(!infinity);
        CHECK(infinity.error().code == slabjson::ErrorCode::InvalidArgument);
        for (char character : output) {
            CHECK(character == '#');
        }
    }

    {
        alignas(std::max_align_t) std::array<std::byte, 1024> storage{};
        slabjson::Slab slab{storage};
        auto object_result = slab.make_object();
        CHECK(object_result);
        CHECK(object_result.value().add("value", 1));

        auto output = std::span<char>{
            reinterpret_cast<char*>(storage.data()),
            storage.size(),
        };
        auto overlap_result =
            slabjson::serialize(object_result.value(), output);
        CHECK(!overlap_result);
        CHECK(
            overlap_result.error().code
            == slabjson::ErrorCode::InvalidArgument);
    }

    {
        slabjson::StaticSlab<8192> slab;
        auto root_result = slab.make_array();
        CHECK(root_result);
        auto root = root_result.value();
        auto current = root;

        constexpr std::size_t kDepth = 128;
        for (std::size_t index = 0; index < kDepth; ++index) {
            auto child_result = current.add_array();
            CHECK(child_result);
            if (!child_result) {
                break;
            }
            current = child_result.value();
        }
        CHECK(current.add_null());

        auto size_result = slabjson::serialized_size(root);
        CHECK(size_result);
        CHECK(size_result.value() == (kDepth + 1) * 2 + 4);

        std::array<char, 512> output{};
        auto result = slabjson::serialize(root, output);
        CHECK(result);
        CHECK(result.value() == size_result.value());
        CHECK(output.front() == '[');
        CHECK(output[result.value() - 1] == ']');
    }

    {
        slabjson::StaticSlab<256> slab;
        auto value_result = slab.make_null();
        CHECK(value_result);
        auto value = value_result.value();
        slab.reset();

        auto size_result = slabjson::serialized_size(value);
        CHECK(!size_result);
        CHECK(size_result.error().code == slabjson::ErrorCode::InvalidHandle);

        std::array<char, 16> output{};
        auto result = slabjson::serialize(value, output);
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::InvalidHandle);
    }

    return failures == 0 ? 0 : 1;
}
