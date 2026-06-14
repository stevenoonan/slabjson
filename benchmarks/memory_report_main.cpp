#include "benchmark_support.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <slabjson/cjson_compat.hpp>
#include <slabjson/serialize.hpp>
#include <slabjson/static_slab.hpp>

namespace {

namespace sb = slabjson::benchmarking;

struct AllocationStats {
    std::size_t current;
    std::size_t peak;
    std::size_t total;
    std::size_t allocations;
    std::size_t generation;
};

struct alignas(std::max_align_t) AllocationHeader {
    std::size_t size;
    std::size_t generation;
};

AllocationStats allocation_stats{0, 0, 0, 0, 1};

void reset_allocation_stats()
{
    allocation_stats.current = 0;
    allocation_stats.peak = 0;
    allocation_stats.total = 0;
    allocation_stats.allocations = 0;
    ++allocation_stats.generation;
    if (allocation_stats.generation == 0) {
        allocation_stats.generation = 1;
    }
}

void* tracked_malloc(std::size_t size)
{
    if (size > std::numeric_limits<std::size_t>::max()
            - sizeof(AllocationHeader)) {
        return nullptr;
    }
    auto* header = static_cast<AllocationHeader*>(
        std::malloc(sizeof(AllocationHeader) + size));
    if (header == nullptr) {
        return nullptr;
    }
    header->size = size;
    header->generation = allocation_stats.generation;

    allocation_stats.current += size;
    allocation_stats.total += size;
    ++allocation_stats.allocations;
    if (allocation_stats.current > allocation_stats.peak) {
        allocation_stats.peak = allocation_stats.current;
    }
    return header + 1;
}

void tracked_free(void* pointer)
{
    if (pointer == nullptr) {
        return;
    }
    auto* header = static_cast<AllocationHeader*>(pointer) - 1;
    if (header->generation == allocation_stats.generation) {
        if (header->size > allocation_stats.current) {
            allocation_stats.current = 0;
        } else {
            allocation_stats.current -= header->size;
        }
    }
    std::free(header);
}

struct Row {
    std::string implementation;
    std::string corpus;
    std::string variant;
    std::string operation;
    std::size_t input_bytes;
    std::size_t output_bytes;
    bool success;
    std::size_t slab_used;
    std::size_t slab_capacity;
    std::size_t cjson_current;
    std::size_t cjson_peak;
    std::size_t cjson_total;
    std::size_t cjson_allocations;
};

[[nodiscard]] std::string csv_escape(std::string_view value)
{
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) {
        return std::string{value};
    }
    std::string escaped{"\""};
    for (char character : value) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

void write_rows(std::ostream& output, const std::vector<Row>& rows)
{
    output
        << "implementation,corpus,variant,operation,input_bytes,"
           "output_bytes,success,slab_used,slab_capacity,"
           "cjson_current,cjson_peak,cjson_total,cjson_allocations\n";
    for (const Row& row : rows) {
        output << csv_escape(row.implementation) << ','
               << csv_escape(row.corpus) << ','
               << csv_escape(row.variant) << ','
               << csv_escape(row.operation) << ','
               << row.input_bytes << ','
               << row.output_bytes << ','
               << (row.success ? "true" : "false") << ','
               << row.slab_used << ','
               << row.slab_capacity << ','
               << row.cjson_current << ','
               << row.cjson_peak << ','
               << row.cjson_total << ','
               << row.cjson_allocations << '\n';
    }
}

[[nodiscard]] bool cjson_balanced_after_cleanup()
{
    return allocation_stats.current == 0;
}

void add_slab_rows(
    const sb::CorpusDocument& document,
    std::vector<Row>& rows)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    auto parsed = slabjson::parse(slab, document.input);
    const std::size_t parsed_used = slab.used_bytes();
    rows.push_back(Row{
        "SlabJson",
        document.profile,
        document.variant,
        "parse",
        document.input.size(),
        0,
        static_cast<bool>(parsed),
        parsed_used,
        slab.capacity_bytes(),
        0,
        0,
        0,
        0,
    });
    if (!parsed) {
        return;
    }

    std::vector<char> compact_output;
    std::size_t compact_written = 0;
    const bool compact_success = sb::serialize_slab(
        parsed.value(),
        false,
        compact_output,
        compact_written);

    slabjson::StaticSlab<sb::kSlabCapacity> roundtrip_slab;
    auto roundtrip_value =
        slabjson::parse(roundtrip_slab, document.input);
    std::vector<char> roundtrip_output;
    std::size_t roundtrip_written = 0;
    const bool roundtrip_success =
        roundtrip_value
        && sb::serialize_slab(
            roundtrip_value.value(),
            false,
            roundtrip_output,
            roundtrip_written);
    rows.push_back(Row{
        "SlabJson",
        document.profile,
        document.variant,
        "roundtrip_compact",
        document.input.size(),
        roundtrip_written,
        roundtrip_success,
        roundtrip_slab.used_bytes(),
        roundtrip_slab.capacity_bytes(),
        0,
        0,
        0,
        0,
    });

    if (document.variant != "compact") {
        return;
    }

    rows.push_back(Row{
        "SlabJson",
        document.profile,
        document.variant,
        "serialize_compact",
        document.input.size(),
        compact_written,
        compact_success,
        parsed_used,
        slab.capacity_bytes(),
        0,
        0,
        0,
        0,
    });

    std::vector<char> pretty_output;
    std::size_t pretty_written = 0;
    const bool pretty_success = sb::serialize_slab(
        parsed.value(),
        true,
        pretty_output,
        pretty_written);
    rows.push_back(Row{
        "SlabJson",
        document.profile,
        document.variant,
        "serialize_pretty",
        document.input.size(),
        pretty_written,
        pretty_success,
        parsed_used,
        slab.capacity_bytes(),
        0,
        0,
        0,
        0,
    });

    slabjson::StaticSlab<sb::kSlabCapacity> destination;
    auto duplicate = slabjson::cjson::duplicate(
        destination,
        parsed.value(),
        true);
    rows.push_back(Row{
        "SlabJson",
        document.profile,
        document.variant,
        "duplicate_recursive",
        document.input.size(),
        0,
        static_cast<bool>(duplicate),
        destination.used_bytes(),
        destination.capacity_bytes(),
        0,
        0,
        0,
        0,
    });
}

[[nodiscard]] bool add_cjson_rows(
    const sb::CorpusDocument& document,
    std::vector<Row>& rows)
{
    bool balanced = true;

    reset_allocation_stats();
    auto parsed = sb::parse_cjson_full(document.input);
    const AllocationStats parse_stats = allocation_stats;
    const bool parse_success = static_cast<bool>(parsed);
    parsed.reset();
    balanced = balanced && cjson_balanced_after_cleanup();
    rows.push_back(Row{
        "cJSON",
        document.profile,
        document.variant,
        "parse",
        document.input.size(),
        0,
        parse_success,
        0,
        0,
        parse_stats.current,
        parse_stats.peak,
        parse_stats.total,
        parse_stats.allocations,
    });
    if (!parse_success) {
        return balanced;
    }

    auto source = sb::parse_cjson_full(document.input);
    if (!source) {
        return false;
    }
    const std::size_t compact_capacity =
        sb::cjson_output_capacity(source.get(), false);
    const std::size_t pretty_capacity =
        sb::cjson_output_capacity(source.get(), true);

    reset_allocation_stats();
    std::vector<char> roundtrip_output(compact_capacity);
    auto roundtrip = sb::parse_cjson_full(document.input);
    const bool roundtrip_printed =
        roundtrip
        && cJSON_PrintPreallocated(
            roundtrip.get(),
            roundtrip_output.data(),
            static_cast<int>(roundtrip_output.size()),
            0);
    const std::size_t roundtrip_written = roundtrip_printed
        ? std::strlen(roundtrip_output.data())
        : 0;
    const AllocationStats roundtrip_stats = allocation_stats;
    roundtrip.reset();
    balanced = balanced && cjson_balanced_after_cleanup();
    rows.push_back(Row{
        "cJSON",
        document.profile,
        document.variant,
        "roundtrip_compact",
        document.input.size(),
        roundtrip_written,
        roundtrip_printed,
        0,
        0,
        roundtrip_stats.current,
        roundtrip_stats.peak,
        roundtrip_stats.total,
        roundtrip_stats.allocations,
    });

    if (document.variant != "compact") {
        return balanced;
    }

    reset_allocation_stats();
    std::vector<char> compact_output(compact_capacity);
    const bool compact_success = cJSON_PrintPreallocated(
        source.get(),
        compact_output.data(),
        static_cast<int>(compact_output.size()),
        0) != 0;
    const std::size_t compact_written = compact_success
        ? std::strlen(compact_output.data())
        : 0;
    const AllocationStats compact_stats = allocation_stats;
    rows.push_back(Row{
        "cJSON",
        document.profile,
        document.variant,
        "serialize_compact",
        document.input.size(),
        compact_written,
        compact_success,
        0,
        0,
        compact_stats.current,
        compact_stats.peak,
        compact_stats.total,
        compact_stats.allocations,
    });

    reset_allocation_stats();
    std::vector<char> pretty_output(pretty_capacity);
    const bool pretty_success = cJSON_PrintPreallocated(
        source.get(),
        pretty_output.data(),
        static_cast<int>(pretty_output.size()),
        1) != 0;
    const std::size_t pretty_written = pretty_success
        ? std::strlen(pretty_output.data())
        : 0;
    const AllocationStats pretty_stats = allocation_stats;
    rows.push_back(Row{
        "cJSON",
        document.profile,
        document.variant,
        "serialize_pretty",
        document.input.size(),
        pretty_written,
        pretty_success,
        0,
        0,
        pretty_stats.current,
        pretty_stats.peak,
        pretty_stats.total,
        pretty_stats.allocations,
    });

    reset_allocation_stats();
    cJSON* duplicate = cJSON_Duplicate(source.get(), 1);
    const AllocationStats duplicate_stats = allocation_stats;
    const bool duplicate_success = duplicate != nullptr;
    cJSON_Delete(duplicate);
    balanced = balanced && cjson_balanced_after_cleanup();
    rows.push_back(Row{
        "cJSON",
        document.profile,
        document.variant,
        "duplicate_recursive",
        document.input.size(),
        0,
        duplicate_success,
        0,
        0,
        duplicate_stats.current,
        duplicate_stats.peak,
        duplicate_stats.total,
        duplicate_stats.allocations,
    });

    source.reset();
    return balanced;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        std::filesystem::path output_path;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--output" && index + 1 < argc) {
                output_path = argv[++index];
            } else {
                throw std::runtime_error(
                    "usage: slabjson_memory_report [--output PATH]");
            }
        }

        cJSON_Hooks hooks{
            tracked_malloc,
            tracked_free,
        };
        cJSON_InitHooks(&hooks);

        std::vector<Row> rows;
        bool balanced = true;
        for (const auto& document : sb::load_corpus()) {
            add_slab_rows(document, rows);
            balanced = add_cjson_rows(document, rows) && balanced;
        }

        for (const auto& document : sb::load_capacity_documents()) {
            rows.push_back(Row{
                "SlabJson",
                document.name,
                "canonical",
                "capacity_check",
                document.input_bytes,
                0,
                false,
                0,
                sb::kSlabCapacity,
                0,
                0,
                0,
                0,
            });
        }

        if (!output_path.empty()) {
            std::ofstream output(output_path);
            if (!output) {
                throw std::runtime_error(
                    "unable to open memory report output");
            }
            write_rows(output, rows);
        } else {
            write_rows(std::cout, rows);
        }

        cJSON_InitHooks(nullptr);
        if (!balanced) {
            std::cerr << "cJSON allocation hooks detected an imbalance\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& exception) {
        cJSON_InitHooks(nullptr);
        std::cerr << "memory report failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
