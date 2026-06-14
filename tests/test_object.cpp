#include <array>
#include <cstddef>
#include <cstdint>
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
        CHECK(root.add("ratio", 2.5));
        CHECK(root.add_null("missing"));

        CHECK(root.size() == 6);
        CHECK(root.contains("name"));
        CHECK(!root.contains("xame"));
        CHECK(root.find("name")->as_string().value_or("") == "sensor");
        CHECK(root.find("enabled")->as_bool().value_or(false));
        CHECK(root.find("count")->as_number().value_or(0.0) == 7.0);
        CHECK(root.find("wide")->as_number().value_or(0.0) == 123456.0);
        CHECK(root.find("ratio")->as_number().value_or(0.0) == 2.5);
        CHECK(root.find("missing")->is_null());
        CHECK(!root.find("unknown"));

        auto nested_result = root.add_object("nested");
        CHECK(nested_result);
        auto nested = nested_result.value();
        CHECK(nested.add("value", 42));
        CHECK(root.find("nested")->as_object()->find("value")
                  ->as_number().value_or(0.0)
            == 42.0);

        auto array_result = root.add_array("items");
        CHECK(array_result);
        auto items = array_result.value();
        CHECK(items.add("first"));
        CHECK(root.find("items")->as_array()->at(0)
                  ->as_string().value_or("")
            == "first");

        CHECK(root.add("duplicate", 1));
        CHECK(root.add("duplicate", 2));
        CHECK(root.find("duplicate")->as_number().value_or(0.0) == 1.0);
        CHECK(root.remove("duplicate"));
        CHECK(root.find("duplicate")->as_number().value_or(0.0) == 2.0);
        CHECK(root.remove("duplicate"));
        CHECK(!root.contains("duplicate"));

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
        CHECK(root.remove("detached"));
        CHECK(second_parent.add("moved", detached));
        CHECK(second_parent.find("moved")->as_string().value_or("") == "detached");

        slabjson::StaticSlab<256> other_slab;
        auto foreign_result = other_slab.make_null();
        CHECK(foreign_result);
        auto foreign = root.add("foreign", foreign_result.value());
        CHECK(!foreign);
        CHECK(foreign.error().code == slabjson::ErrorCode::CrossSlab);

        auto cycle = nested.add("root", root.value());
        CHECK(!cycle);
        CHECK(cycle.error().code == slabjson::ErrorCode::CycleDetected);

        auto null_string = root.add("bad", static_cast<const char*>(nullptr));
        CHECK(!null_string);
        CHECK(null_string.error().code == slabjson::ErrorCode::InvalidArgument);

        slab.reset();
        CHECK(!root.valid());
        CHECK(root.empty());
        CHECK(!root.find("name"));
        auto stale_add = root.add("value", true);
        CHECK(!stale_add);
        CHECK(stale_add.error().code == slabjson::ErrorCode::InvalidHandle);
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
