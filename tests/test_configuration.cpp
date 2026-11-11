#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>

#include <slabjson/cjson_compat.hpp>
#include <slabjson/slabjson.hpp>

// These names can coexist only when the disabled declarations are absent.
namespace slabjson {
#if !SLABJSON_ENABLE_PARSER
inline constexpr int parse = 0;
struct ParseOptions {};
#endif
#if !SLABJSON_ENABLE_SERIALIZER
inline constexpr int serialized_size = 0, serialize = 0, serialize_partial = 0;
#endif
#if !SLABJSON_ENABLE_SERIALIZER || !SLABJSON_ENABLE_PRETTY
inline constexpr int serialized_size_pretty = 0, serialize_pretty = 0, serialize_pretty_partial = 0;
#endif
namespace cjson {
#if !SLABJSON_ENABLE_CJSON_COMPAT
inline constexpr int create_null = 0, duplicate = 0;
#endif
#if !SLABJSON_ENABLE_CJSON_COMPAT || !SLABJSON_ENABLE_PARSER
inline constexpr int parse = 0;
#endif
#if !SLABJSON_ENABLE_CJSON_COMPAT || !SLABJSON_ENABLE_SERIALIZER
inline constexpr int print_unformatted = 0, print_preallocated = 0;
#endif
#if !SLABJSON_ENABLE_CJSON_COMPAT || !SLABJSON_ENABLE_SERIALIZER || !SLABJSON_ENABLE_PRETTY
inline constexpr int print_pretty = 0;
#endif
}
}

namespace {
int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #__VA_ARGS__); ++failures; } } while (false)
using namespace slabjson;

template <typename T>
concept Constructible = requires(Slab& slab, Object& object, Array& array, T number) {
    slab.make_number(number);
    object.add("number", number);
    array.add(number);
};
template <typename T>
concept HasFloatAccessor = requires(T value) { value.as_number(); };
template <typename T>
concept HasLegacyAccessor = requires(T value) { value.as_int64(); value.as_uint64(); };

static_assert(Constructible<int_t> && Constructible<uint_t>);
static_assert(Constructible<std::int32_t> && Constructible<std::uint32_t>);
static_assert(Constructible<short> && Constructible<unsigned short>);
static_assert(HasLegacyAccessor<Value> == bool(SLABJSON_FULL_NUMBERS));
static_assert(HasFloatAccessor<Value> == bool(SLABJSON_HAS_FLOAT));
#if SLABJSON_ENABLE_CJSON_COMPAT
template <typename T>
concept CompatConstructible = requires(Slab& slab, T number) { cjson::create_number(slab, number); };
static_assert(CompatConstructible<int_t> && CompatConstructible<uint_t>);
static_assert(CompatConstructible<std::int32_t> && CompatConstructible<std::uint32_t>);
#if !SLABJSON_FULL_NUMBERS
static_assert(!CompatConstructible<std::int64_t> && !CompatConstructible<double>);
#endif
#if !SLABJSON_HAS_FLOAT
static_assert(!CompatConstructible<float>);
#endif
#endif
#if !SLABJSON_FULL_NUMBERS
static_assert(std::is_same_v<int_t, std::int32_t>);
static_assert(std::is_same_v<uint_t, std::uint32_t>);
static_assert(!Constructible<std::int64_t> && !Constructible<std::uint64_t>);
static_assert(!Constructible<double>);
#endif
#if SLABJSON_HAS_FLOAT
static_assert(Constructible<slabjson::float_t>);
static_assert(Constructible<float>);
#if !SLABJSON_FULL_NUMBERS
static_assert(std::is_same_v<slabjson::float_t, float>);
#endif
#else
static_assert(!Constructible<float> && !Constructible<double>);
#endif

void storage_and_numbers()
{
    StaticSlab<4096> slab;
    auto value = slab.make_null();
    CHECK(value);
    CHECK(slab.used_bytes() == (SLABJSON_FULL_NUMBERS ? 24 : 16));
    slab.reset();
    CHECK(!value.value().valid());

    auto minimum = slab.make_number(std::numeric_limits<int_t>::min()).value();
    auto maximum = slab.make_number(std::numeric_limits<uint_t>::max()).value();
    CHECK(minimum.as_integer() == std::numeric_limits<int_t>::min());
    CHECK(!minimum.as_unsigned_integer());
    CHECK(maximum.as_unsigned_integer() == std::numeric_limits<uint_t>::max());
    CHECK(!maximum.as_integer());
    CHECK(slab.make_number(uint_t{42}).value().as_integer() == 42);
    CHECK(slab.make_number(int_t{42}).value().as_unsigned_integer() == 42);
    const std::uint32_t count = UINT32_MAX;
    const std::int32_t delta = -7;
    CHECK(slab.make_number(count).value().as_unsigned_integer() == count);
    CHECK(slab.make_number(delta).value().as_integer() == delta);
    CHECK(minimum.number_kind() == NumberKind::SignedInteger);
    CHECK(maximum.number_kind() == NumberKind::UnsignedInteger);
#if SLABJSON_ENABLE_SERIALIZER
    std::array<char, 64> encoded{};
    auto min_size = serialize(minimum, encoded).value();
    CHECK(std::string_view(encoded.data(), min_size) == std::to_string(std::numeric_limits<int_t>::min()));
    auto max_size = serialize(maximum, encoded).value();
    CHECK(std::string_view(encoded.data(), max_size) == std::to_string(std::numeric_limits<uint_t>::max()));
#endif
#if SLABJSON_HAS_FLOAT
    auto fraction = slab.make_number(1.25f).value();
    CHECK(fraction.as_number() == slabjson::float_t{1.25});
    CHECK(!fraction.as_integer());
    CHECK(!slab.make_number(std::numeric_limits<slabjson::float_t>::infinity()));
    CHECK(!slab.make_number(std::numeric_limits<slabjson::float_t>::quiet_NaN()));
#endif

    auto object = slab.make_object().value();
    auto array = object.add_array("items").value();
    CHECK(array.add(minimum));
    CHECK(array.add(maximum));
    CHECK(array.size() == 2);
    CHECK(array.at(0)->as_integer() == std::numeric_limits<int_t>::min());
    CHECK(object.add("answer", 42));
    CHECK(object.get_integer("answer").value() == 42);
    CHECK(object.get_unsigned_integer("answer").value() == 42);
    CHECK(object.add("count", count));
    CHECK(object.add("delta", delta));
    CHECK(object.get_unsigned_integer("count").value() == count);
    CHECK(object.get_integer("delta").value() == delta);
    CHECK(array.add(count));
    CHECK(array.add(delta));
    CHECK(array.at(2)->as_unsigned_integer() == count);
    CHECK(array.at(3)->as_integer() == delta);
#if SLABJSON_HAS_FLOAT
    CHECK(object.add("fraction", 1.25f));
    CHECK(object.get_number("fraction").value() == 1.25f);
    CHECK(array.add(1.25f));
    CHECK(array.at(4)->as_number() == 1.25f);
#endif
#if SLABJSON_ENABLE_CJSON_COMPAT
    CHECK(cjson::create_number(slab, count).value().as_unsigned_integer() == count);
    CHECK(cjson::create_number(slab, delta).value().as_integer() == delta);
#endif
    CHECK(!array.add(minimum));
    CHECK(array.status().error().code == ErrorCode::AlreadyAttached);
    array.clear_error();
    auto cycle = array.add(object.value());
    CHECK(!cycle && cycle.error().code == ErrorCode::CycleDetected);
    array.clear_error();
    StaticSlab<128> other;
    CHECK(!array.add(other.make_null().value()));
    array.clear_error();
    const auto before = slab.used_bytes();
    CHECK(!object.add("bad\x80", 1));
    CHECK(slab.used_bytes() == before);
    object.clear_error();
    CHECK(object.add("after", true));
    slab.reset();
    CHECK(!object.valid() && !array.valid() && !maximum.valid());
}

void caller_storage()
{
    alignas(std::max_align_t) std::array<std::byte, 1024> storage{};
    Slab slab{std::span(storage).subspan(1)};
    CHECK(slab.valid() && slab.capacity_bytes() < storage.size() - 1);
    auto text = slab.make_string("valid").value();
    const auto before = slab.used_bytes();
#if SLABJSON_ENABLE_PARSER
    auto overlap = slabjson::parse(slab, {reinterpret_cast<const char*>(storage.data() + 32), 4});
    CHECK(!overlap && overlap.error().code == ErrorCode::InvalidArgument);
    CHECK(slab.used_bytes() == before);
#endif
#if SLABJSON_ENABLE_SERIALIZER
    auto alias = std::span<char>{reinterpret_cast<char*>(storage.data() + 256), 64};
    auto overlap_output = serialize(text, alias);
    CHECK(!overlap_output && overlap_output.error().code == ErrorCode::InvalidArgument);
    CHECK(slab.used_bytes() == before);
    // Caller storage is writable; serialization must validate even a string
    // that was valid when inserted and needs no JSON escaping.
    auto view = text.as_string().value();
    const_cast<char*>(view.data())[0] = '\x80';
    std::array<char, 32> output;
    output.fill('#');
    auto invalid = serialize(text, output);
    CHECK(!invalid && invalid.error().code == ErrorCode::InvalidUtf8);
    CHECK(std::all_of(output.begin(), output.end(), [](char ch) { return ch == '#'; }));
#endif
    CHECK(text.valid() && slab.used_bytes() == before);
}

#if SLABJSON_ENABLE_PARSER
void parsing()
{
    StaticSlab<65535> slab;
    auto existing = slab.make_string("retained").value();
    const auto before = slab.used_bytes();
    auto invalid = slabjson::parse(slab, R"([1,])");
    CHECK(!invalid && invalid.error().code == ErrorCode::ParseUnexpectedToken);
    CHECK(invalid.error().offset == 3);
    CHECK(slab.used_bytes() == before && existing.as_string() == "retained");
    for (std::string_view text : {R"({"a":)", R"({"a" 0})", "[", "[1,", "{", "{\"a\"", "[1 2]", "{\"a\":0,}", "[[]", "null false"}) {
        CHECK(!slabjson::parse(slab, text));
        CHECK(slab.used_bytes() == before);
    }
    auto unicode = slabjson::parse(slab, R"(["\uD83D\uDE00","line\n\u0000"])");
    CHECK(unicode);
    auto items = unicode.value().as_array().value();
    auto key = slabjson::parse(slab, R"({"\u00e9":42})");
    CHECK(key && key.value().as_object()->get_integer("é").value() == 42);
    CHECK(items.at(0)->as_string() == "\xf0\x9f\x98\x80");
    CHECK(items.at(1)->as_string()->size() == 6);
    const auto unicode_used = slab.used_bytes();
    CHECK(!slabjson::parse(slab, "\"\x80\""));
    CHECK(slab.used_bytes() == unicode_used);

    ParseOptions options;
    options.max_depth = 0;
    CHECK(slabjson::parse(slab, "0", options));
    CHECK(!slabjson::parse(slab, "[]", options));
    options.max_depth = kMaxParserDepth + 1;
    CHECK(slabjson::parse(slab, "0", options).error().code == ErrorCode::InvalidArgument);
    options = {};
    options.allow_trailing_whitespace = false;
    CHECK(slabjson::parse(slab, "null ", options).error().offset == 4);

    // Keep these within slab capacity even when the compiled limit is very large.
    const auto depth = std::min<std::uint16_t>(kMaxParserDepth, 64);
    std::string nested(depth, '[');
    nested += '0';
    nested.append(depth, ']');
    slab.reset();
    options = {};
    options.max_depth = depth;
    CHECK(slabjson::parse(slab, nested, options));
    slab.reset();
    nested.insert(nested.begin(), '[');
    nested += ']';
    auto excessive = slabjson::parse(slab, nested, options);
    CHECK(!excessive && excessive.error().code == ErrorCode::ParserDepthExceeded);
    CHECK(excessive.error().offset == depth);
    CHECK(slab.used_bytes() == 0);

    for (auto text : {std::to_string(std::numeric_limits<int_t>::min()),
                     std::to_string(std::numeric_limits<uint_t>::max())}) {
        slab.reset();
        CHECK(slabjson::parse(slab, text));
    }
#if !SLABJSON_FULL_NUMBERS
    for (std::string_view text : {"-2147483649", "4294967296", "18446744073709551615"}) {
        slab.reset();
        auto overflow = slabjson::parse(slab, text);
        CHECK(!overflow && overflow.error().code == ErrorCode::ParseInvalidNumber);
        CHECK(overflow.error().offset == 0 && slab.used_bytes() == 0);
    }
#endif
#if SLABJSON_HAS_FLOAT
    slab.reset();
    auto fraction = slabjson::parse(slab, "1.25e0");
    CHECK(fraction && fraction.value().as_number() == slabjson::float_t{1.25});
    auto negative_zero = slabjson::parse(slab, "-0.0");
    CHECK(negative_zero && std::signbit(*negative_zero.value().as_number()));
    const auto used = slab.used_bytes();
    CHECK(!slabjson::parse(slab, "1e999"));
    CHECK(slab.used_bytes() == used);
#else
    for (std::string_view text : {"1.0", "1e0", "-0.0", "0E-1"}) {
        slab.reset();
        auto unsupported = slabjson::parse(slab, text);
        CHECK(!unsupported && unsupported.error().code == ErrorCode::ParseInvalidNumber);
        CHECK(slab.used_bytes() == 0);
    }
#endif
    StaticSlab<32> small;
    auto retained = small.make_bool(true).value();
    const auto small_before = small.used_bytes();
    CHECK(!slabjson::parse(small, "[1,2,3]"));
    CHECK(small.used_bytes() == small_before && retained.as_bool() == true);
}
#endif

void serialization_and_duplication()
{
    StaticSlab<4096> slab;
    auto root = slab.make_object().value();
    CHECK(root.add("n", int_t{-7}));
    auto array = root.add_array("items").value();
    CHECK(array.add("é\n"));
    CHECK(array.add(true));
    CHECK(array.add_null());
    CHECK(array.add_object());
    CHECK(root.add("tail", "end"));
#if SLABJSON_ENABLE_SERIALIZER
    constexpr std::string_view expected = "{\"n\":-7,\"items\":[\"é\\n\",true,null,{}],\"tail\":\"end\"}";
    std::array<char, 512> output{};
    auto written = serialize(root, output);
    CHECK(written && std::string_view(output.data(), written.value()) == expected);
    CHECK(serialized_size(root).value() == expected.size());
    CHECK(serialize_partial(root, output).value() == expected.size());
    output.fill('#');
    CHECK(!serialize(root, std::span(output).first(expected.size() - 1)));
    CHECK(std::all_of(output.begin(), output.end(), [](char ch) { return ch == '#'; }));
    CHECK(!serialize_partial(root, std::span(output).first(8)));
    CHECK(output[0] == '{' && output[8] == '#');
    CHECK(serialize(root, std::span(output).first(expected.size())).value() == expected.size());
#if SLABJSON_ENABLE_PRETTY
    auto pretty = serialize_pretty(root, output);
    CHECK(pretty && serialized_size_pretty(root).value() == pretty.value());
    CHECK(serialize_pretty_partial(root, output).value() == pretty.value());
#endif
#if SLABJSON_ENABLE_PARSER && SLABJSON_MAX_PARSER_DEPTH >= 2
    StaticSlab<4096> roundtrip;
    CHECK(slabjson::parse(roundtrip, {output.data(), serialize(root, output).value()}));
#endif
    // Serialize a subtree without following its parent or next sibling.
    CHECK(serialize(array, output));
    CHECK(output[0] == '[');
#endif
#if SLABJSON_ENABLE_CJSON_COMPAT
    StaticSlab<4096> destination;
    auto clone = cjson::duplicate(destination, root);
    CHECK(clone && clone.value().as_object()->size() == 3);
    auto cloned_items = clone.value().as_object()->get_array("items").value();
    CHECK(cloned_items.size() == 4 && cloned_items.at(3)->as_object()->empty());
    CHECK(cloned_items.at(0)->as_string() == "é\n");
    CHECK(cjson::duplicate(destination, root, false).value().as_object()->empty());
    StaticSlab<32> small;
    auto retained = small.make_bool(true).value();
    const auto before = small.used_bytes();
    CHECK(!cjson::duplicate(small, root));
    CHECK(small.used_bytes() == before && retained.as_bool() == true);
    auto detached = cjson::detach_item_from_array(array, 0);
    CHECK(detached && detached.value().as_string() == "é\n");
    CHECK(array.add(detached.value()));
    CHECK(array.size() == 4);
#if SLABJSON_ENABLE_SERIALIZER
    output.fill('#');
#if SLABJSON_ENABLE_PRETTY
    CHECK(cjson::print_preallocated(root, output, true));
#else
    auto pretty = cjson::print_preallocated(root, output, true);
    CHECK(!pretty && pretty.error().code == ErrorCode::InvalidArgument);
    CHECK(std::all_of(output.begin(), output.end(), [](char ch) { return ch == '#'; }));
#endif
    CHECK(cjson::print_preallocated(root, output, false));
#endif
#endif
}
} // namespace

int main()
{
    storage_and_numbers();
    caller_storage();
#if SLABJSON_ENABLE_PARSER
    parsing();
#endif
    serialization_and_duplication();
    return failures ? 1 : 0;
}
