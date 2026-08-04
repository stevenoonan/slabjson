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
        auto array_result = slab.make_array();
        CHECK(array_result);
        auto array = array_result.value();

        CHECK(array.valid());
        CHECK(array.empty());
        CHECK(array.value().is_array());
        CHECK(array.value().as_array().has_value());
        CHECK(!array.value().as_object());

        CHECK(array.add("text"));
        CHECK(array.add(true));
        CHECK(array.add(4));
        CHECK(array.add(static_cast<std::int64_t>(500000)));
        CHECK(array.add(1.25));
        CHECK(array.add_null());

        CHECK(array.size() == 6);
        CHECK(array.at(0)->as_string().value_or("") == "text");
        CHECK(array.at(1)->as_bool().value_or(false));
        CHECK(array.at(2)->as_number().value_or(0.0) == 4.0);
        CHECK(array.at(3)->as_number().value_or(0.0) == 500000.0);
        CHECK(array.at(4)->as_number().value_or(0.0) == 1.25);
        CHECK(array.at(5)->is_null());
        CHECK(!array.at(6));
        CHECK(!array.at(1000));

        auto object_result = array.add_object();
        CHECK(object_result);
        auto object = object_result.value();
        CHECK(object.add("id", 9));
        CHECK(array.at(6)->as_object()->find("id")
                  ->as_number().value_or(0.0)
            == 9.0);

        auto nested_result = array.add_array();
        CHECK(nested_result);
        auto nested = nested_result.value();
        CHECK(nested.add(false));
        CHECK(array.at(7)->as_array()->at(0)->as_bool().has_value());
        CHECK(!array.at(7)->as_array()->at(0)->as_bool().value());

        auto detached_result = slab.make_string("detached");
        CHECK(detached_result);
        auto detached = detached_result.value();
        CHECK(array.add(detached));
        CHECK(array.at(8)->as_string().value_or("") == "detached");

        auto second_array_result = slab.make_array();
        CHECK(second_array_result);
        auto second_array = second_array_result.value();
        auto already_attached = second_array.add(detached);
        CHECK(!already_attached);
        CHECK(already_attached.error().code == slabjson::ErrorCode::AlreadyAttached);
        slab.clear_error();

        auto self_cycle = second_array.add(second_array.value());
        CHECK(!self_cycle);
        CHECK(self_cycle.error().code == slabjson::ErrorCode::CycleDetected);
        slab.clear_error();

        auto ancestor_cycle = nested.add(array.value());
        CHECK(!ancestor_cycle);
        CHECK(ancestor_cycle.error().code == slabjson::ErrorCode::CycleDetected);
        slab.clear_error();

        slabjson::StaticSlab<256> other_slab;
        auto foreign_result = other_slab.make_bool(false);
        CHECK(foreign_result);
        auto foreign = array.add(foreign_result.value());
        CHECK(!foreign);
        CHECK(foreign.error().code == slabjson::ErrorCode::CrossSlab);
        slab.clear_error();

        auto null_string = array.add(static_cast<const char*>(nullptr));
        CHECK(!null_string);
        CHECK(null_string.error().code == slabjson::ErrorCode::InvalidArgument);

        slab.reset();
        CHECK(!array.valid());
        CHECK(array.empty());
        CHECK(!array.at(0));
        auto stale_add = array.add(true);
        CHECK(!stale_add);
        CHECK(stale_add.error().code == slabjson::ErrorCode::InvalidHandle);
    }

    {
        slabjson::StaticSlab<256> slab;
        auto array_result = slab.make_array();
        CHECK(array_result);
        auto array = array_result.value();
        const std::size_t used_before = slab.used_bytes();

        std::array<char, 256> oversized_string{};
        auto result = array.add(
            std::string_view{oversized_string.data(), oversized_string.size()});
        CHECK(!result);
        CHECK(result.error().code == slabjson::ErrorCode::StringCapacityExceeded);
        CHECK(slab.used_bytes() == used_before);
        CHECK(array.empty());
    }

    return failures == 0 ? 0 : 1;
}
