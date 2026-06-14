#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>

#include <slabjson/cjson_compat.hpp>
#include <slabjson/static_slab.hpp>

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
    namespace cjson = slabjson::cjson;

    {
        slabjson::StaticSlab<8192> slab;

        auto root_result = cjson::create_object(slab);
        auto name_result = cjson::create_string(slab, "sensor");
        auto enabled_result = cjson::create_true(slab);
        auto count_result = cjson::create_number(slab, 7);
        auto unsigned_result = cjson::create_number(
            slab,
            std::numeric_limits<std::uint64_t>::max());
        auto ratio_result = cjson::create_number(slab, 2.5);
        auto null_result = cjson::create_null(slab);
        auto array_result = cjson::create_array(slab);

        CHECK(root_result);
        CHECK(name_result);
        CHECK(enabled_result);
        CHECK(count_result);
        CHECK(unsigned_result);
        CHECK(ratio_result);
        CHECK(null_result);
        CHECK(array_result);

        const auto root = root_result.value();
        const auto array = array_result.value();
        CHECK(cjson::add_item_to_object(root, "Name", name_result.value()));
        CHECK(cjson::add_item_to_object(root, "enabled", enabled_result.value()));
        CHECK(cjson::add_item_to_object(root, "count", count_result.value()));
        CHECK(cjson::add_item_to_object(
            root,
            "unsigned",
            unsigned_result.value()));
        CHECK(cjson::add_item_to_object(root, "ratio", ratio_result.value()));
        CHECK(cjson::add_item_to_object(root, "nothing", null_result.value()));
        CHECK(cjson::add_item_to_object(root, "items", array));

        auto false_result = cjson::create_false(slab);
        auto text_result = cjson::create_string(slab, "first");
        CHECK(false_result);
        CHECK(text_result);
        CHECK(cjson::add_item_to_array(array, false_result.value()));
        CHECK(cjson::add_item_to_array(array, text_result.value()));

        CHECK(cjson::get_array_size(root) == 7);
        CHECK(cjson::get_array_size(array) == 2);
        CHECK(cjson::get_array_item(array, 0).has_value());
        CHECK(cjson::is_false(*cjson::get_array_item(array, 0)));
        CHECK(!cjson::get_array_item(array, 2));

        auto case_insensitive = cjson::get_object_item(root, "name");
        CHECK(case_insensitive);
        CHECK(cjson::get_string_value(*case_insensitive).value_or("") == "sensor");
        CHECK(!cjson::get_object_item_case_sensitive(root, "name"));
        CHECK(cjson::get_object_item_case_sensitive(root, "Name"));
        CHECK(cjson::has_object_item(root, "NAME"));

        CHECK(cjson::is_object(root));
        CHECK(cjson::is_array(array));
        CHECK(cjson::is_string(name_result.value()));
        CHECK(cjson::is_true(enabled_result.value()));
        CHECK(cjson::is_bool(enabled_result.value()));
        CHECK(cjson::is_number(count_result.value()));
        CHECK(cjson::is_null(null_result.value()));
        CHECK(cjson::get_number_value(count_result.value()).value_or(0.0) == 7.0);

        std::array<char, 512> compact{};
        auto compact_result = cjson::print_unformatted(root, compact);
        CHECK(compact_result);
        CHECK(compact[compact_result.value()] == '\0');
        CHECK((std::string_view{compact.data(), compact_result.value()}
            == "{\"Name\":\"sensor\",\"enabled\":true,\"count\":7,"
               "\"unsigned\":18446744073709551615,\"ratio\":2.5,"
               "\"nothing\":null,\"items\":[false,\"first\"]}"));

        std::array<char, 512> pretty{};
        auto pretty_result = cjson::print_preallocated(root, pretty, true, 2);
        CHECK(pretty_result);
        CHECK(pretty[pretty_result.value()] == '\0');
        CHECK((std::string_view{pretty.data(), pretty_result.value()}
            == "{\n"
               "  \"Name\": \"sensor\",\n"
               "  \"enabled\": true,\n"
               "  \"count\": 7,\n"
               "  \"unsigned\": 18446744073709551615,\n"
               "  \"ratio\": 2.5,\n"
               "  \"nothing\": null,\n"
               "  \"items\": [\n"
               "    false,\n"
               "    \"first\"\n"
               "  ]\n"
               "}"));

        std::array<char, 4> too_small;
        too_small.fill('#');
        auto small_result = cjson::print_unformatted(root, too_small);
        CHECK(!small_result);
        CHECK(
            small_result.error().code
            == slabjson::ErrorCode::OutputCapacityExceeded);
        for (char character : too_small) {
            CHECK(character == '#');
        }

        slabjson::StaticSlab<8192> copy_slab;
        auto copy_result = cjson::duplicate(copy_slab, root);
        CHECK(copy_result);
        std::array<char, 512> copied{};
        auto copied_result =
            cjson::print_unformatted(copy_result.value(), copied);
        CHECK(copied_result);
        CHECK((std::string_view{copied.data(), copied_result.value()}
            == std::string_view{compact.data(), compact_result.value()}));
        CHECK(
            cjson::get_object_item(copy_result.value(), "unsigned")
                ->as_uint64()
                == std::numeric_limits<std::uint64_t>::max());

        auto shallow_result = cjson::duplicate(copy_slab, root, false);
        CHECK(shallow_result);
        CHECK(cjson::is_object(shallow_result.value()));
        CHECK(cjson::get_array_size(shallow_result.value()) == 0);

        auto detached_name =
            cjson::detach_item_from_object(root, "name");
        CHECK(detached_name);
        CHECK(!cjson::has_object_item(root, "Name"));
        auto second_object = cjson::create_object(slab);
        CHECK(second_object);
        CHECK(cjson::add_item_to_object(
            second_object.value(),
            "moved",
            detached_name.value()));
        auto same_slab_copy =
            cjson::duplicate(slab, second_object.value());
        CHECK(same_slab_copy);
        CHECK(cjson::is_object(same_slab_copy.value()));
        CHECK(cjson::has_object_item(same_slab_copy.value(), "moved"));

        auto detached_first =
            cjson::detach_item_from_array(array, 0);
        CHECK(detached_first);
        CHECK(cjson::is_false(detached_first.value()));
        CHECK(cjson::get_array_size(array) == 1);
        CHECK(cjson::add_item_to_array(array, detached_first.value()));
        CHECK(cjson::get_array_size(array) == 2);

        CHECK(cjson::delete_item_from_array(array, 0));
        CHECK(cjson::get_array_size(array) == 1);
        CHECK(cjson::delete_item_from_object(root, "NOTHING"));
        CHECK(!cjson::has_object_item(root, "nothing"));

        auto missing_array = cjson::detach_item_from_array(array, 10);
        CHECK(!missing_array);
        CHECK(missing_array.error().code == slabjson::ErrorCode::NotFound);
        auto wrong_type =
            cjson::add_item_to_array(count_result.value(), detached_first.value());
        CHECK(!wrong_type);
        CHECK(wrong_type.error().code == slabjson::ErrorCode::TypeMismatch);

        cjson::delete_all(slab);
        CHECK(cjson::is_invalid(root));
        CHECK(!cjson::get_object_item(root, "enabled"));
    }

    {
        slabjson::StaticSlab<2048> slab;
        auto parsed = cjson::parse(
            slab,
            "{\"value\":42,\"items\":[true]}");
        CHECK(parsed);
        CHECK(cjson::is_object(parsed.value()));
        CHECK(
            cjson::get_number_value(
                *cjson::get_object_item(parsed.value(), "value"))
                .value_or(0.0)
            == 42.0);
    }

    {
        slabjson::StaticSlab<4096> source_slab;
        auto source = cjson::parse(
            source_slab,
            "{\"long\":\"abcdefghijklmnopqrstuvwxyz\","
            "\"nested\":[1,2,3,4,5,6,7,8]}");
        CHECK(source);

        slabjson::StaticSlab<128> small_slab;
        const std::size_t used_before = small_slab.used_bytes();
        auto copy = cjson::duplicate(small_slab, source.value());
        CHECK(!copy);
        CHECK(small_slab.used_bytes() == used_before);
    }

    {
        const char key_bytes[] = {
            'k',
            '\0',
            static_cast<char>(0xc3),
            static_cast<char>(0xa9),
        };
        const char value_bytes[] = {
            static_cast<char>(0xe2),
            static_cast<char>(0x82),
            static_cast<char>(0xac),
            '\0',
            'x',
        };
        const std::string_view key{key_bytes, sizeof(key_bytes)};
        const std::string_view value{value_bytes, sizeof(value_bytes)};

        slabjson::StaticSlab<4096> source_slab;
        auto source = cjson::create_object(source_slab);
        auto string = cjson::create_string(source_slab, value);
        CHECK(source);
        CHECK(string);
        CHECK(cjson::add_item_to_object(
            source.value(),
            key,
            string.value()));

        slabjson::StaticSlab<4096> copy_slab;
        auto copy = cjson::duplicate(copy_slab, source.value());
        CHECK(copy);
        auto copied_string =
            cjson::get_object_item_case_sensitive(copy.value(), key);
        CHECK(copied_string);
        CHECK(copied_string->as_string() == value);
    }

    {
        slabjson::StaticSlab<32768> source_slab;
        auto root_result = cjson::create_array(source_slab);
        CHECK(root_result);
        auto current = root_result.value();

        constexpr std::size_t kDepth = 128;
        for (std::size_t index = 0; index < kDepth; ++index) {
            auto child = cjson::create_array(source_slab);
            CHECK(child);
            CHECK(cjson::add_item_to_array(current, child.value()));
            current = child.value();
        }
        auto leaf = cjson::create_bool(source_slab, true);
        CHECK(leaf);
        CHECK(cjson::add_item_to_array(current, leaf.value()));

        slabjson::StaticSlab<32768> copy_slab;
        auto copy = cjson::duplicate(copy_slab, root_result.value());
        CHECK(copy);

        std::array<char, 512> output{};
        auto printed = cjson::print_unformatted(copy.value(), output);
        CHECK(printed);
        CHECK(printed.value() == (kDepth + 1) * 2 + 4);

        std::array<char, 1024> pretty_output{};
        auto pretty =
            cjson::print_pretty(copy.value(), pretty_output, 0);
        CHECK(pretty);
        CHECK(pretty_output[pretty.value()] == '\0');
    }

    return failures == 0 ? 0 : 1;
}
