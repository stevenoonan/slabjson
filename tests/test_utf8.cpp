#include <array>
#include <cstring>
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

struct Case {
    std::string_view text;
    bool valid;
    std::size_t string_error{0};
    std::size_t parse_error{0};
};
} // namespace

int main()
{
    const Case cases[] = {
        {"ascii", true},
        {"\xc2\x80", true}, {"\xdf\xbf", true},
        {"\xe0\xa0\x80", true}, {"\xed\x9f\xbf", true},
        {"\xee\x80\x80", true}, {"\xef\xbf\xbf", true},
        {"\xf0\x90\x80\x80", true}, {"\xf4\x8f\xbf\xbf", true},
        {"\x80", false, 0, 1}, {"\xc0\x80", false, 0, 1},
        {"\xe0\x80\x80", false, 0, 1},
        {"\xed\xa0\x80", false, 0, 1},
        {"\xf0\x80\x80\x80", false, 0, 1},
        {"\xf4\x90\x80\x80", false, 0, 1},
        {"\xf5\x80\x80\x80", false, 0, 1},
        {"\xe2(\xa1", false, 1, 2},
        {"a\xe2\x82!", false, 3, 4},
        // In JSON, the closing quote can be the invalid continuation byte.
        {"\xc2", false, 0, 2}, {"\xe2", false, 0, 1},
        {"\xe2\x82", false, 0, 3},
        {"\xf0\x90\x80", false, 0, 4},
    };
    for (const auto& test : cases) {
        slabjson::StaticSlab<256> slab;
        auto string = slab.make_string(test.text);
        auto object = slab.make_object().value();
        const auto used_before = slab.used_bytes();
        auto member = object.add(test.text, true);
        std::array<char, 32> json{};
        json[0] = '"';
        std::memcpy(json.data() + 1, test.text.data(), test.text.size());
        json[test.text.size() + 1] = '"';
        auto parsed = slabjson::parse(slab, {json.data(), test.text.size() + 2});
        CHECK(bool(string) == test.valid);
        CHECK(bool(member) == test.valid);
        CHECK(bool(parsed) == test.valid);
        if (!test.valid) {
            if (!string) {
                CHECK(string.error().code == slabjson::ErrorCode::InvalidUtf8);
                CHECK(string.error().offset == test.string_error);
            }
            if (!member) {
                CHECK(member.error().code == slabjson::ErrorCode::InvalidUtf8);
                CHECK(member.error().offset == test.string_error);
            }
            if (!parsed) {
                CHECK(parsed.error().code == slabjson::ErrorCode::ParseInvalidString);
                CHECK(parsed.error().offset == test.parse_error);
            }
            CHECK(slab.used_bytes() == used_before);
            CHECK(object.empty());
        } else if (string && parsed) {
            CHECK(string.value().as_string() == test.text);
            CHECK(parsed.value().as_string() == test.text);
        }
    }
    return failures == 0 ? 0 : 1;
}
