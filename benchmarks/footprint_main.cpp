#include <array>
#include <cstddef>
#include <slabjson/slabjson.hpp>

#ifdef FOOTPRINT_HOST_PROBE
#include <cstdio>
#endif

// All workloads use the same document and provisioned buffer sizes. Keeping
// buffers outside main separates their RAM cost from compiler stack reports.
#if FOOTPRINT_OPERATION != 0
std::array<std::byte, 512> slab_storage{};
#if FOOTPRINT_OPERATION == 1 || FOOTPRINT_OPERATION == 3
char input[] = R"({"n":-7,"items":["é\n",true,null,{}],"tail":"end"})";
#endif
#if FOOTPRINT_OPERATION == 2 || FOOTPRINT_OPERATION == 3
std::array<char, 128> output{};
#endif
volatile std::size_t observed_used;
#endif

int main()
{
#if FOOTPRINT_OPERATION != 0
    slabjson::Slab slab{slab_storage};
#if FOOTPRINT_OPERATION == 2 || FOOTPRINT_OPERATION == 4
    auto object = slab.make_object();
    if (!object) { return 1; }
    auto root = object.value();
    if (!root.add("n", -7)) { return 2; }
    auto items = root.add_array("items");
    if (!items || !items.value().add("é\n") || !items.value().add(true)
        || !items.value().add_null() || !items.value().add_object()
        || !root.add("tail", "end")) { return 3; }
    auto value = root.value();
#else
    auto parsed = slabjson::parse(slab, input);
    if (!parsed) { return 4; }
    auto value = parsed.value();
#endif
#if FOOTPRINT_OPERATION == 2 || FOOTPRINT_OPERATION == 3
    auto written = slabjson::serialize(value, output);
    if (!written) { return 5; }
#endif
    observed_used = slab.used_bytes();
#ifdef FOOTPRINT_HOST_PROBE
    std::printf("%zu %zu %zu %zu %zu\n", slab.used_bytes(), slab.capacity_bytes(), slabjson::Slab::node_size_bytes(),
#if FOOTPRINT_OPERATION == 1 || FOOTPRINT_OPERATION == 3
        sizeof(input),
#else
        std::size_t{0},
#endif
#if FOOTPRINT_OPERATION == 2 || FOOTPRINT_OPERATION == 3
        output.size()
#else
        std::size_t{0}
#endif
    );
#endif
#endif
    return 0;
}
