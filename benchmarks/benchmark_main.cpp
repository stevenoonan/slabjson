#include "benchmark_support.hpp"

#include <cstring>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <benchmark/benchmark.h>

#include <slabjson/cjson_compat.hpp>
#include <slabjson/serialize.hpp>
#include <slabjson/static_slab.hpp>

namespace {

namespace sb = slabjson::benchmarking;

void set_throughput(
    benchmark::State& state,
    std::size_t bytes)
{
    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations())
        * static_cast<std::int64_t>(bytes));
    state.counters["bytes"] =
        benchmark::Counter(static_cast<double>(bytes));
}

void slab_parse_lifecycle(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    for (auto _ : state) {
        (void)_;
        auto result = slabjson::parse(slab, document.input);
        benchmark::DoNotOptimize(result);
        if (!result) {
            state.SkipWithError("SlabJson parse failed");
            break;
        }
        slab.reset();
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void cjson_parse_lifecycle(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    for (auto _ : state) {
        (void)_;
        auto result = sb::parse_cjson_full(document.input);
        benchmark::DoNotOptimize(result.get());
        if (!result) {
            state.SkipWithError("cJSON parse failed");
            break;
        }
        result.reset();
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void slab_serialized_size(
    benchmark::State& state,
    const sb::CorpusDocument& document,
    bool pretty)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    auto parsed = slabjson::parse(slab, document.input);
    if (!parsed) {
        state.SkipWithError("SlabJson setup parse failed");
        return;
    }

    std::size_t output_bytes = 0;
    for (auto _ : state) {
        (void)_;
        auto result = pretty
            ? slabjson::serialized_size_pretty(parsed.value())
            : slabjson::serialized_size(parsed.value());
        benchmark::DoNotOptimize(result);
        if (!result) {
            state.SkipWithError("SlabJson sizing failed");
            break;
        }
        output_bytes = result.value();
    }
    set_throughput(state, output_bytes);
}

void slab_serialize_transactional(
    benchmark::State& state,
    const sb::CorpusDocument& document,
    bool pretty)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    auto parsed = slabjson::parse(slab, document.input);
    if (!parsed) {
        state.SkipWithError("SlabJson setup parse failed");
        return;
    }

    const auto size_result = pretty
        ? slabjson::serialized_size_pretty(parsed.value())
        : slabjson::serialized_size(parsed.value());
    if (!size_result) {
        state.SkipWithError("SlabJson output sizing failed");
        return;
    }
    std::vector<char> output(size_result.value());

    for (auto _ : state) {
        (void)_;
        auto result = pretty
            ? slabjson::serialize_pretty(parsed.value(), output)
            : slabjson::serialize(parsed.value(), output);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
        if (!result) {
            state.SkipWithError("SlabJson serialization failed");
            break;
        }
    }
    set_throughput(state, size_result.value());
}

void slab_serialize_partial(
    benchmark::State& state,
    const sb::CorpusDocument& document,
    bool pretty)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    auto parsed = slabjson::parse(slab, document.input);
    if (!parsed) {
        state.SkipWithError("SlabJson setup parse failed");
        return;
    }

    const auto size_result = pretty
        ? slabjson::serialized_size_pretty(parsed.value())
        : slabjson::serialized_size(parsed.value());
    if (!size_result) {
        state.SkipWithError("SlabJson output sizing failed");
        return;
    }
    std::vector<char> output(size_result.value());

    for (auto _ : state) {
        (void)_;
        auto result = pretty
            ? slabjson::serialize_pretty_partial(parsed.value(), output)
            : slabjson::serialize_partial(parsed.value(), output);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
        if (!result) {
            state.SkipWithError("SlabJson partial serialization failed");
            break;
        }
    }
    set_throughput(state, size_result.value());
}

void cjson_serialize(
    benchmark::State& state,
    const sb::CorpusDocument& document,
    bool pretty)
{
    auto parsed = sb::parse_cjson_full(document.input);
    if (!parsed) {
        state.SkipWithError("cJSON setup parse failed");
        return;
    }

    const std::size_t capacity =
        sb::cjson_output_capacity(parsed.get(), pretty);
    if (capacity == 0
        || capacity > static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        state.SkipWithError("cJSON output sizing failed");
        return;
    }
    std::vector<char> output(capacity);
    std::size_t output_bytes = 0;
    if (!cJSON_PrintPreallocated(
            parsed.get(),
            output.data(),
            static_cast<int>(output.size()),
            pretty ? 1 : 0)) {
        state.SkipWithError("cJSON setup serialization failed");
        return;
    }
    output_bytes = std::strlen(output.data());

    for (auto _ : state) {
        (void)_;
        int result = cJSON_PrintPreallocated(
            parsed.get(),
            output.data(),
            static_cast<int>(output.size()),
            pretty ? 1 : 0);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
        if (result == 0) {
            state.SkipWithError("cJSON serialization failed");
            break;
        }
    }
    set_throughput(state, output_bytes);
}

void slab_round_trip(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    slabjson::StaticSlab<sb::kSlabCapacity> sizing_slab;
    auto sizing_value = slabjson::parse(sizing_slab, document.input);
    if (!sizing_value) {
        state.SkipWithError("SlabJson setup parse failed");
        return;
    }
    auto size_result = slabjson::serialized_size(sizing_value.value());
    if (!size_result) {
        state.SkipWithError("SlabJson output sizing failed");
        return;
    }

    std::vector<char> output(size_result.value());
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    for (auto _ : state) {
        (void)_;
        auto parsed = slabjson::parse(slab, document.input);
        if (!parsed) {
            state.SkipWithError("SlabJson parse failed");
            break;
        }
        auto result = slabjson::serialize(parsed.value(), output);
        benchmark::DoNotOptimize(result);
        if (!result) {
            state.SkipWithError("SlabJson serialization failed");
            break;
        }
        slab.reset();
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void cjson_round_trip(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    auto sizing_value = sb::parse_cjson_full(document.input);
    if (!sizing_value) {
        state.SkipWithError("cJSON setup parse failed");
        return;
    }
    const std::size_t capacity =
        sb::cjson_output_capacity(sizing_value.get(), false);
    if (capacity == 0
        || capacity > static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        state.SkipWithError("cJSON output sizing failed");
        return;
    }
    std::vector<char> output(capacity);

    for (auto _ : state) {
        (void)_;
        auto parsed = sb::parse_cjson_full(document.input);
        if (!parsed) {
            state.SkipWithError("cJSON parse failed");
            break;
        }
        int result = cJSON_PrintPreallocated(
            parsed.get(),
            output.data(),
            static_cast<int>(output.size()),
            0);
        benchmark::DoNotOptimize(result);
        if (result == 0) {
            state.SkipWithError("cJSON serialization failed");
            break;
        }
        parsed.reset();
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void slab_duplicate_recursive(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    slabjson::StaticSlab<sb::kSlabCapacity> source_slab;
    auto source = slabjson::parse(source_slab, document.input);
    if (!source) {
        state.SkipWithError("SlabJson setup parse failed");
        return;
    }

    slabjson::StaticSlab<sb::kSlabCapacity> destination;
    for (auto _ : state) {
        (void)_;
        auto result = slabjson::cjson::duplicate(
            destination,
            source.value(),
            true);
        benchmark::DoNotOptimize(result);
        if (!result) {
            state.SkipWithError("SlabJson duplicate failed");
            break;
        }
        destination.reset();
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void cjson_duplicate_recursive(
    benchmark::State& state,
    const sb::CorpusDocument& document)
{
    auto source = sb::parse_cjson_full(document.input);
    if (!source) {
        state.SkipWithError("cJSON setup parse failed");
        return;
    }

    for (auto _ : state) {
        (void)_;
        cJSON* result = cJSON_Duplicate(source.get(), 1);
        benchmark::DoNotOptimize(result);
        if (result == nullptr) {
            state.SkipWithError("cJSON duplicate failed");
            break;
        }
        cJSON_Delete(result);
        benchmark::ClobberMemory();
    }
    set_throughput(state, document.input.size());
}

void register_pair(
    std::string_view family,
    const sb::CorpusDocument& document,
    void (*slab_function)(
        benchmark::State&,
        const sb::CorpusDocument&),
    void (*cjson_function)(
        benchmark::State&,
        const sb::CorpusDocument&))
{
    const std::string suffix =
        document.profile + "/" + document.variant;
    benchmark::RegisterBenchmark(
        (std::string{family} + "/SlabJson/" + suffix).c_str(),
        slab_function,
        std::cref(document));
    benchmark::RegisterBenchmark(
        (std::string{family} + "/cJSON/" + suffix).c_str(),
        cjson_function,
        std::cref(document));
}

} // namespace

int main(int argc, char** argv)
{
    try {
        auto documents = sb::load_corpus();
        for (const auto& document : documents) {
            std::string failure;
            if (!sb::validate_document(document, failure)) {
                std::cerr << document.profile << '/' << document.variant
                          << ": " << failure << '\n';
                return 1;
            }

            register_pair(
                "ParseLifecycle",
                document,
                slab_parse_lifecycle,
                cjson_parse_lifecycle);
            register_pair(
                "RoundTripCompact",
                document,
                slab_round_trip,
                cjson_round_trip);

            if (document.variant == "compact") {
                for (const bool pretty : {false, true}) {
                    const std::string style =
                        pretty ? "pretty" : "compact";
                    const std::string suffix =
                        document.profile + "/" + style;

                    benchmark::RegisterBenchmark(
                        ("SerializedSize/SlabJson/" + suffix).c_str(),
                        slab_serialized_size,
                        std::cref(document),
                        pretty);
                    benchmark::RegisterBenchmark(
                        ("SerializeTransactional/SlabJson/" + suffix).c_str(),
                        slab_serialize_transactional,
                        std::cref(document),
                        pretty);
                    benchmark::RegisterBenchmark(
                        ("SerializeTransactional/cJSON/" + suffix).c_str(),
                        cjson_serialize,
                        std::cref(document),
                        pretty);
                    benchmark::RegisterBenchmark(
                        ("SerializePartial/SlabJson/" + suffix).c_str(),
                        slab_serialize_partial,
                        std::cref(document),
                        pretty);
                    benchmark::RegisterBenchmark(
                        ("SerializePartial/cJSON/" + suffix).c_str(),
                        cjson_serialize,
                        std::cref(document),
                        pretty);
                }
                register_pair(
                    "DuplicateRecursive",
                    document,
                    slab_duplicate_recursive,
                    cjson_duplicate_recursive);
            }
        }

        benchmark::Initialize(&argc, argv);
        if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "benchmark setup failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
