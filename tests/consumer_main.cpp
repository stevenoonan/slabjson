#include <array>
#include <span>
#include <string_view>

#include <slabjson/slabjson.hpp>

int main()
{
    slabjson::StaticSlab<512> slab;
    auto parsed = slabjson::parse(slab, R"({"answer":42})");
    if (!parsed) {
        return 1;
    }

    auto object = parsed.value().as_object();
    if (!object) {
        return 2;
    }
    auto answer = object->get_int64("answer");
    if (!answer || answer.value() != 42) {
        return 3;
    }

    std::array<char, 32> output{};
    auto written = slabjson::serialize(parsed.value(), std::span<char>{output});
    if (!written) {
        return 4;
    }
    return std::string_view{output.data(), written.value()}
            == R"({"answer":42})"
        ? 0
        : 5;
}
