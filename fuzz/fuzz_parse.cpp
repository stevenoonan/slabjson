#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <slabjson/cjson_compat.hpp>
#include <slabjson/slabjson.hpp>

namespace {
void require(bool condition)
{
    if (!condition) {
        std::abort();
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    slabjson::StaticSlab<65535> source;
    auto existing = source.make_string("preserved").value();
    const auto before = source.used_bytes();
    auto parsed = slabjson::parse(source, {reinterpret_cast<const char*>(data), size});
    require(existing.as_string() == "preserved");
    if (!parsed) {
        require(source.used_bytes() == before);
        return 0;
    }

    slabjson::StaticSlab<65535> destination;
    std::array<char, 131072> output{}, copied{};
    for (bool pretty : {false, true}) {
        auto measured = pretty ? slabjson::serialized_size_pretty(parsed.value())
                               : slabjson::serialized_size(parsed.value());
        require(bool(measured));
        if (measured.value() > output.size()) {
            continue;
        }
        auto written = pretty ? slabjson::serialize_pretty(parsed.value(), output)
                              : slabjson::serialize(parsed.value(), output);
        require(written && written.value() == measured.value());

        destination.reset();
        auto clone = slabjson::cjson::duplicate(destination, parsed.value());
        require(bool(clone));
        auto clone_written = pretty ? slabjson::serialize_pretty(clone.value(), copied)
                                    : slabjson::serialize(clone.value(), copied);
        require(clone_written && clone_written.value() == written.value());
        require(std::memcmp(output.data(), copied.data(), written.value()) == 0);
        auto partial = pretty ? slabjson::serialize_pretty_partial(parsed.value(), copied)
                              : slabjson::serialize_partial(parsed.value(), copied);
        require(partial && partial.value() == written.value());
        require(std::memcmp(output.data(), copied.data(), written.value()) == 0);

        destination.reset();
        auto reparsed = slabjson::parse(destination, {output.data(), written.value()});
        require(bool(reparsed));
        auto replay = pretty ? slabjson::serialize_pretty(reparsed.value(), copied)
                             : slabjson::serialize(reparsed.value(), copied);
        require(replay && replay.value() == written.value());
        require(std::memcmp(output.data(), copied.data(), written.value()) == 0);

        output.fill('#');
        auto short_output = std::span{output}.first(measured.value() - 1);
        auto insufficient = pretty ? slabjson::serialize_pretty(parsed.value(), short_output)
                                   : slabjson::serialize(parsed.value(), short_output);
        require(!insufficient);
        require(insufficient.error().code == slabjson::ErrorCode::OutputCapacityExceeded);
        require(std::all_of(output.begin(), output.end(), [](char ch) { return ch == '#'; }));
    }

    slabjson::StaticSlab<64> small;
    auto retained = small.make_bool(true).value();
    const auto small_before = small.used_bytes();
    auto clone = slabjson::cjson::duplicate(small, parsed.value());
    if (!clone) {
        require(small.used_bytes() == small_before);
        require(retained.as_bool() == true);
    }
    return 0;
}
