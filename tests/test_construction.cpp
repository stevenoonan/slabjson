#include <array>
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

template <typename T>
slabjson::Result<void> status(slabjson::Result<T> result)
{
    return result ? slabjson::Result<void>{} : result.error();
}
} // namespace

int main()
{
    using slabjson::Object;
    using slabjson::Array;
    using slabjson::ErrorCode;
    using slabjson::Result;
    using std::string_view;
    using ObjectAdd = Result<void> (*)(Object&, string_view);
    const ObjectAdd object_adders[] = {
        [](Object& o, string_view k) { return o.add(k, string_view{"value"}); },
        [](Object& o, string_view k) { return o.add(k, true); },
        [](Object& o, string_view k) { return o.add(k, std::int64_t{-7}); },
        [](Object& o, string_view k) { return o.add(k, std::uint64_t{7}); },
        [](Object& o, string_view k) { return o.add(k, 1.25); },
        [](Object& o, string_view k) { return o.add_null(k); },
        [](Object& o, string_view k) { return status(o.add_object(k)); },
        [](Object& o, string_view k) { return status(o.add_array(k)); },
    };
    for (auto add : object_adders) {
        slabjson::StaticSlab<256> slab;
        auto object = slab.make_object().value();
        const auto used = slab.used_bytes();
        auto invalid_key = add(object, "a\x80");
        CHECK(!invalid_key);
        if (!invalid_key) {
            CHECK(invalid_key.error().code == ErrorCode::InvalidUtf8);
            CHECK(invalid_key.error().offset == 1);
        }
        CHECK(slab.used_bytes() == used);
        CHECK(object.empty());
        auto skipped = add(object, "valid");
        CHECK(!skipped);
        if (!skipped) {
            CHECK(skipped.error().code == ErrorCode::InvalidUtf8);
            CHECK(skipped.error().offset == 1);
        }
        CHECK(slab.used_bytes() == used);
        object.clear_error();
        CHECK(add(object, "valid"));
        CHECK(object.size() == 1);

        slab.reset();
        const auto reset_used = slab.used_bytes();
        auto stale = add(object, "valid");
        CHECK(!stale);
        if (!stale) {
            CHECK(stale.error().code == ErrorCode::InvalidHandle);
        }
        CHECK(slab.used_bytes() == reset_used);
    }

    using ArrayAdd = Result<void> (*)(Array&);
    const ArrayAdd array_adders[] = {
        [](Array& a) { return a.add(string_view{"value"}); },
        [](Array& a) { return a.add(true); },
        [](Array& a) { return a.add(std::int64_t{-7}); },
        [](Array& a) { return a.add(std::uint64_t{7}); },
        [](Array& a) { return a.add(1.25); },
        [](Array& a) { return a.add_null(); },
        [](Array& a) { return status(a.add_object()); },
        [](Array& a) { return status(a.add_array()); },
    };
    for (auto add : array_adders) {
        slabjson::StaticSlab<256> slab;
        auto array = slab.make_array().value();
        CHECK(!array.add(std::numeric_limits<double>::infinity()));
        const auto used = slab.used_bytes();
        auto skipped = add(array);
        CHECK(!skipped);
        if (!skipped) {
            CHECK(skipped.error().code == ErrorCode::NonFiniteNumber);
        }
        CHECK(slab.used_bytes() == used);
        CHECK(array.empty());
        array.clear_error();
        CHECK(add(array));
        CHECK(array.size() == 1);

        slabjson::StaticSlab<24> exhausted;
        auto full = exhausted.make_array().value();
        const auto full_used = exhausted.used_bytes();
        auto failed = add(full);
        CHECK(!failed);
        if (!failed) {
            CHECK(failed.error().code == ErrorCode::OutOfMemory);
        }
        CHECK(exhausted.used_bytes() == full_used);
        CHECK(full.empty());
    }
    return failures == 0 ? 0 : 1;
}
