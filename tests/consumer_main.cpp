#include <array>
#include <span>
#include <string_view>

#include <slabjson/slabjson.hpp>

#ifdef EXPECTED_NUMBER_MODE
static_assert(std::string_view{SLABJSON_NUMBER_MODE} == EXPECTED_NUMBER_MODE);
static_assert(SLABJSON_ENABLE_PARSER == EXPECTED_ENABLE_PARSER);
static_assert(SLABJSON_ENABLE_SERIALIZER == EXPECTED_ENABLE_SERIALIZER);
static_assert(SLABJSON_ENABLE_PRETTY == EXPECTED_ENABLE_PRETTY);
static_assert(SLABJSON_ENABLE_CJSON_COMPAT == EXPECTED_ENABLE_CJSON_COMPAT);
static_assert(SLABJSON_OPTIMIZE_SIZE == EXPECTED_OPTIMIZE_SIZE);
static_assert(SLABJSON_MAX_PARSER_DEPTH == EXPECTED_MAX_PARSER_DEPTH);
#endif

int main()
{
    slabjson::StaticSlab<512> slab;
#if SLABJSON_ENABLE_PARSER
    auto parsed = slabjson::parse(slab, R"({"answer":42})");
#else
    auto object_result = slab.make_object();
    if (!object_result || !object_result.value().add("answer", 42)) { return 1; }
    slabjson::Result<slabjson::Value> parsed = object_result.value().value();
#endif
    if (!parsed) {
        return 1;
    }

    auto object = parsed.value().as_object();
    if (!object) {
        return 2;
    }
    auto answer = object->get_integer("answer");
    if (!answer || answer.value() != 42) {
        return 3;
    }

#if SLABJSON_ENABLE_SERIALIZER
    std::array<char, 32> output{};
    auto written = slabjson::serialize(parsed.value(), std::span<char>{output});
    if (!written) {
        return 4;
    }
    return std::string_view{output.data(), written.value()}
            == R"({"answer":42})"
        ? 0
        : 5;
#else
    return 0;
#endif
}
