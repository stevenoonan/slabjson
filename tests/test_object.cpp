#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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
    {
        slabjson::StaticSlab<4096> slab;
        auto root_result = slab.make_object();
        CHECK(root_result);
        auto root = root_result.value();

        CHECK(root.valid());
        CHECK(root.empty());
        CHECK(root.value().is_object());
        CHECK(root.value().as_object().has_value());
        CHECK(!root.value().as_array());

        std::array<char, 5> key{'n', 'a', 'm', 'e', '\0'};
        CHECK(root.add(std::string_view{key.data(), 4}, "sensor"));
        key[0] = 'x';
        CHECK(root.add("enabled", true));
        CHECK(root.add("count", 7));
        CHECK(root.add("wide", static_cast<std::int64_t>(123456)));
        CHECK(root.add("unsigned", std::numeric_limits<std::uint64_t>::max()));
        CHECK(root.add("ratio", 2.5));
        CHECK(root.add_null("missing"));

        CHECK(root.size() == 7);
        CHECK(root.contains("name"));
        CHECK(!root.contains("xame"));
        CHECK(root.find("name")->as_string().value_or("") == "sensor");
        CHECK(root.find("enabled")->as_bool().value_or(false));
        CHECK(root.find("count")->as_number().value_or(0.0) == 7.0);
        CHECK(root.find("wide")->as_number().value_or(0.0) == 123456.0);
        CHECK(root.find("ratio")->as_number().value_or(0.0) == 2.5);
        CHECK(root.find("missing")->is_null());
        CHECK(!root.find("unknown"));

        auto name = root.get_string("name");
        CHECK(name);
        CHECK(name.value() == "sensor");

        auto enabled = root.get_bool("enabled");
        CHECK(enabled);
        CHECK(enabled.value());

        auto count = root.get_int64("count");
        CHECK(count);
        CHECK(count.value() == 7);

        auto unsigned_value = root.get_uint64("unsigned");
        CHECK(unsigned_value);
        CHECK(unsigned_value.value() == std::numeric_limits<std::uint64_t>::max());

        auto ratio = root.get_number("ratio");
        CHECK(ratio);
        CHECK(ratio.value() == 2.5);

        auto converted_count = root.get_number("count");
        CHECK(converted_count);
        CHECK(converted_count.value() == 7.0);

        auto nested_result = root.add_object("nested");
        CHECK(nested_result);
        auto nested = nested_result.value();
        CHECK(nested.add("value", 42));
        auto nested_access = root.get_object("nested");
        CHECK(nested_access);
        CHECK(nested_access.value().get_int64("value").value() == 42);
        CHECK(root.find("nested")->as_object()->find("value")
                  ->as_number().value_or(0.0)
            == 42.0);

        auto array_result = root.add_array("items");
        CHECK(array_result);
        auto items = array_result.value();
        CHECK(items.add("first"));
        auto items_access = root.get_array("items");
        CHECK(items_access);
        CHECK(items_access.value().at(0)->as_string().value_or("") == "first");
        CHECK(root.find("items")->as_array()->at(0)
                  ->as_string().value_or("")
            == "first");

        CHECK(root.add("duplicate", 1));
        CHECK(root.add("duplicate", 2));
        CHECK(root.get_int64("duplicate").value() == 1);
        CHECK(root.find("duplicate")->as_number().value_or(0.0) == 1.0);
        CHECK(root.remove("duplicate"));
        CHECK(root.get_int64("duplicate").value() == 2);
        CHECK(root.find("duplicate")->as_number().value_or(0.0) == 2.0);
        CHECK(root.remove("duplicate"));
        CHECK(!root.contains("duplicate"));

        auto missing_get = root.get_string("unknown");
        CHECK(!missing_get);
        CHECK(missing_get.error().code == slabjson::ErrorCode::NotFound);

        auto string_as_bool = root.get_bool("name");
        CHECK(!string_as_bool);
        CHECK(string_as_bool.error().code == slabjson::ErrorCode::TypeMismatch);

        auto string_as_int64 = root.get_int64("name");
        CHECK(!string_as_int64);
        CHECK(string_as_int64.error().code == slabjson::ErrorCode::TypeMismatch);

        auto unsigned_as_int64 = root.get_int64("unsigned");
        CHECK(!unsigned_as_int64);
        CHECK(unsigned_as_int64.error().code == slabjson::ErrorCode::TypeMismatch);

        auto signed_as_uint64 = root.get_uint64("count");
        CHECK(signed_as_uint64);
        CHECK(signed_as_uint64.value() == 7);

        auto string_as_number = root.get_number("name");
        CHECK(!string_as_number);
        CHECK(string_as_number.error().code == slabjson::ErrorCode::TypeMismatch);

        auto string_as_object = root.get_object("name");
        CHECK(!string_as_object);
        CHECK(string_as_object.error().code == slabjson::ErrorCode::TypeMismatch);

        auto string_as_array = root.get_array("name");
        CHECK(!string_as_array);
        CHECK(string_as_array.error().code == slabjson::ErrorCode::TypeMismatch);

        auto bool_as_string = root.get_string("enabled");
        CHECK(!bool_as_string);
        CHECK(bool_as_string.error().code == slabjson::ErrorCode::TypeMismatch);

        auto missing_remove = root.remove("duplicate");
        CHECK(!missing_remove);
        CHECK(missing_remove.error().code == slabjson::ErrorCode::NotFound);

        auto detached_result = slab.make_string("detached");
        CHECK(detached_result);
        auto detached = detached_result.value();
        CHECK(root.add("detached", detached));

        auto second_parent_result = slab.make_object();
        CHECK(second_parent_result);
        auto second_parent = second_parent_result.value();
        auto already_attached = second_parent.add("again", detached);
        CHECK(!already_attached);
        CHECK(already_attached.error().code == slabjson::ErrorCode::AlreadyAttached);
        slab.clear_error();
        CHECK(root.remove("detached"));
        CHECK(second_parent.add("moved", detached));
        CHECK(second_parent.find("moved")->as_string().value_or("") == "detached");

        slabjson::StaticSlab<256> other_slab;
        auto foreign_result = other_slab.make_null();
        CHECK(foreign_result);
        auto foreign = root.add("foreign", foreign_result.value());
        CHECK(!foreign);
        CHECK(foreign.error().code == slabjson::ErrorCode::CrossSlab);
        slab.clear_error();

        auto cycle = nested.add("root", root.value());
        CHECK(!cycle);
        CHECK(cycle.error().code == slabjson::ErrorCode::CycleDetected);
        slab.clear_error();

        auto null_string = root.add("bad", static_cast<const char*>(nullptr));
        CHECK(!null_string);
        CHECK(null_string.error().code == slabjson::ErrorCode::InvalidArgument);

        slab.reset();
        CHECK(!root.valid());
        CHECK(root.empty());
        CHECK(!root.find("name"));
        auto stale_get = root.get_string("name");
        CHECK(!stale_get);
        CHECK(stale_get.error().code == slabjson::ErrorCode::InvalidHandle);
        auto stale_add = root.add("value", true);
        CHECK(!stale_add);
        CHECK(stale_add.error().code == slabjson::ErrorCode::InvalidHandle);
    }

    {
        slabjson::StaticSlab<1024> slab;
        auto root = slab.make_object().value();
        auto items = root.add_array("items").value();

        root.add("device_id", "widget-123");
        CHECK(root.status());
        CHECK(slab.status());

        root.add("bad", static_cast<const char*>(nullptr));
        const std::size_t used_after_error = slab.used_bytes();
        const std::size_t root_size_after_error = root.size();

        auto skipped = root.add("skipped", 42);
        items.add("also skipped");

        auto root_status = root.status();
        auto items_status = items.status();
        auto slab_status = slab.status();
        CHECK(!root_status);
        CHECK(!items_status);
        CHECK(!slab_status);
        CHECK(root_status.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(items_status.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(slab_status.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(!skipped);
        CHECK(skipped.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(slab.used_bytes() == used_after_error);
        CHECK(root.size() == root_size_after_error);
        CHECK(!root.contains("skipped"));
        CHECK(items.empty());

        items.clear_error();
        CHECK(root.status());
        CHECK(items.status());
        CHECK(root.add("recovered", true));
        CHECK(root.contains("recovered"));

        root.add("bad", static_cast<const char*>(nullptr));
        CHECK(!slab.status());
        slab.reset();
        CHECK(slab.status());
        CHECK(!root.status());
        CHECK(root.status().error().code == slabjson::ErrorCode::InvalidHandle);
    }

    {
        slabjson::StaticSlab<256> slab;
        auto object_result = slab.make_object();
        CHECK(object_result);
        auto object = object_result.value();
        const std::size_t used_before = slab.used_bytes();

        std::array<char, 256> oversized_key{};
        auto result = object.add(
            std::string_view{oversized_key.data(), oversized_key.size()},
            true);
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::StringCapacityExceeded);
        CHECK(slab.used_bytes() == used_before);
        CHECK(object.empty());
    }

    return failures == 0 ? 0 : 1;
}
