#include "benchmark_support.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <slabjson/serialize.hpp>
#include <slabjson/static_slab.hpp>

namespace {

namespace sb = slabjson::benchmarking;

struct Options {
    std::filesystem::path report_path;
};

[[nodiscard]] Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--report" && index + 1 < argc) {
            options.report_path = argv[++index];
        } else {
            throw std::runtime_error(
                "usage: slabjson_conformance [--report PATH]");
        }
    }
    return options;
}

[[nodiscard]] bool all_equal(
    const std::vector<char>& buffer,
    char expected)
{
    return std::all_of(
        buffer.begin(),
        buffer.end(),
        [expected](char value) { return value == expected; });
}

[[nodiscard]] bool verify_full_input(std::ostringstream& report)
{
    const std::string valid = "true \t\r\n";
    slabjson::StaticSlab<sb::kSlabCapacity> valid_slab;
    const bool slab_valid =
        static_cast<bool>(slabjson::parse(valid_slab, valid));
    const bool cjson_valid =
        static_cast<bool>(sb::parse_cjson_full(valid));

    const std::array<char, 10> embedded_nul{
        't', 'r', 'u', 'e', '\0', 'f', 'a', 'l', 's', 'e',
    };
    const std::string_view invalid{
        embedded_nul.data(),
        embedded_nul.size(),
    };
    slabjson::StaticSlab<sb::kSlabCapacity> invalid_slab;
    const bool slab_invalid =
        !slabjson::parse(invalid_slab, invalid);
    const bool cjson_invalid =
        !sb::parse_cjson_full(invalid);

    report << "\nFull input checks\n"
           << "trailing_rfc_whitespace\t"
           << (slab_valid ? "accept" : "reject") << '\t'
           << (cjson_valid ? "accept" : "reject") << '\n'
           << "embedded_nul_trailing_data\t"
           << (slab_invalid ? "reject" : "accept") << '\t'
           << (cjson_invalid ? "reject" : "accept") << '\n';
    return slab_valid && cjson_valid && slab_invalid && cjson_invalid;
}

[[nodiscard]] bool verify_preflight(
    const sb::CorpusDocument& document,
    std::ostringstream& report)
{
    slabjson::StaticSlab<sb::kSlabCapacity> slab;
    auto parsed = slabjson::parse(slab, document.input);
    if (!parsed) {
        report << "preflight setup parse failed\n";
        return false;
    }

    bool success = true;
    for (bool pretty : std::array{false, true}) {
        const auto size_result = pretty
            ? slabjson::serialized_size_pretty(parsed.value())
            : slabjson::serialized_size(parsed.value());
        if (!size_result || size_result.value() == 0) {
            success = false;
            continue;
        }

        std::vector<char> output(size_result.value() - 1, '#');
        const auto result = pretty
            ? slabjson::serialize_pretty(parsed.value(), output)
            : slabjson::serialize(parsed.value(), output);
        const bool unchanged = all_equal(output, '#');
        const bool failed_for_capacity =
            !result
            && result.error().code
                == slabjson::ErrorCode::OutputCapacityExceeded;
        success = success && unchanged && failed_for_capacity;
        report << (pretty ? "pretty" : "compact")
               << "_undersized_output\t"
               << (failed_for_capacity ? "capacity_error" : "wrong_result")
               << '\t'
               << (unchanged ? "unchanged" : "modified")
               << '\n';
    }

    auto cjson_value = sb::parse_cjson_full(document.input);
    if (!cjson_value) {
        report << "cJSON preflight setup parse failed\n";
        return false;
    }
    for (bool pretty : std::array{false, true}) {
        const std::size_t capacity =
            sb::cjson_output_capacity(cjson_value.get(), pretty);
        if (capacity < 2) {
            success = false;
            continue;
        }
        std::vector<char> exact(capacity);
        const bool exact_success = cJSON_PrintPreallocated(
            cjson_value.get(),
            exact.data(),
            static_cast<int>(exact.size()),
            pretty ? 1 : 0) != 0;
        std::vector<char> small(capacity - 5);
        const bool small_failed = cJSON_PrintPreallocated(
            cjson_value.get(),
            small.data(),
            static_cast<int>(small.size()),
            pretty ? 1 : 0) == 0;
        success = success && exact_success && small_failed;
        report << "cjson_"
               << (pretty ? "pretty" : "compact")
               << "_preallocated\t"
               << (exact_success ? "exact_success" : "exact_failure")
               << '\t'
               << (small_failed ? "small_failure" : "small_success")
               << '\n';
    }
    return success;
}

[[nodiscard]] bool verify_corpus(std::ostringstream& report)
{
    auto documents = sb::load_corpus();
    const std::set<std::string> expected_profiles{
        "twitter_1",
        "twitter_4",
        "twitter_8",
        "citm_events_10",
        "citm_events_50",
        "citm_events_100",
        "canada_groups_1",
        "canada_groups_8",
        "canada_groups_16",
    };

    std::map<std::string, std::set<std::string>> variants;
    bool success = documents.size() == expected_profiles.size() * 2;
    report << "\nGenerated corpus\n"
           << "profile\tvariant\tinput_bytes\tslab_used\tstatus\n";
    for (const auto& document : documents) {
        variants[document.profile].insert(document.variant);

        slabjson::StaticSlab<sb::kSlabCapacity> slab;
        auto parsed = slabjson::parse(slab, document.input);
        std::string failure;
        const bool valid =
            parsed && sb::validate_document(document, failure);
        success = success && valid;
        report << document.profile << '\t'
               << document.variant << '\t'
               << document.input.size() << '\t'
               << slab.used_bytes() << '\t'
               << (valid ? "ok" : failure) << '\n';
    }

    if (variants.size() != expected_profiles.size()) {
        success = false;
    }
    for (const auto& profile : expected_profiles) {
        const auto found = variants.find(profile);
        if (found == variants.end()
            || found->second
                != std::set<std::string>{"compact", "pretty"}) {
            success = false;
        }
    }

    if (!documents.empty()) {
        report << "\nOutput preflight\n";
        success =
            verify_preflight(documents.front(), report) && success;
    }
    return success;
}

[[nodiscard]] bool verify_capacity_documents(
    std::ostringstream& report)
{
    auto documents = sb::load_capacity_documents();
    bool success = documents.size() == 3;
    report << "\nCapacity-limited canonical documents\n"
           << "name\tinput_bytes\tstatus\terror_code\n";
    for (const auto& document : documents) {
        const std::string input = sb::read_file(document.path);
        slabjson::StaticSlab<sb::kSlabCapacity> slab;
        auto result = slabjson::parse(slab, input);
        const bool classified =
            input.size() == document.input_bytes
            && document.input_bytes > sb::kSlabCapacity
            && !result
            && sb::is_capacity_error(result.error().code)
            && slab.used_bytes() == 0;
        success = success && classified;
        report << document.name << '\t'
               << document.input_bytes << '\t'
               << (classified ? "capacity_limited" : "unexpected")
               << '\t'
               << (result
                    ? 0
                    : static_cast<int>(result.error().code))
               << '\n';
    }
    return success;
}

[[nodiscard]] bool run_json_test_suite(
    std::ostringstream& report)
{
    const std::filesystem::path directory{
        SLABJSON_JSON_TEST_SUITE_DIR,
    };
    std::vector<std::filesystem::path> paths;
    for (const auto& entry :
         std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()
            && entry.path().extension() == ".json") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    std::size_t valid_count = 0;
    std::size_t invalid_count = 0;
    std::size_t implementation_count = 0;
    bool success = true;
    report << "JSONTestSuite\n"
           << "category\tfile\tslabjson\tcjson\n";
    for (const auto& path : paths) {
        const std::string name = path.filename().string();
        if (name.size() < 2 || name[1] != '_') {
            continue;
        }

        const char category = name[0];
        if (category != 'y'
            && category != 'n'
            && category != 'i') {
            continue;
        }

        const std::string input = sb::read_file(path);
        slabjson::StaticSlab<sb::kSlabCapacity> slab;
        const bool slab_accepts =
            static_cast<bool>(slabjson::parse(slab, input));
        const bool cjson_accepts =
            static_cast<bool>(sb::parse_cjson_full(input));

        if (category == 'y') {
            ++valid_count;
            success = success && slab_accepts;
        } else if (category == 'n') {
            ++invalid_count;
            success = success && !slab_accepts;
        } else {
            ++implementation_count;
        }

        report << category << '\t'
               << name << '\t'
               << (slab_accepts ? "accept" : "reject") << '\t'
               << (cjson_accepts ? "accept" : "reject") << '\n';
    }

    const bool counts_match =
        valid_count == 95
        && invalid_count == 188
        && implementation_count == 35;
    success = success && counts_match;
    report << "summary\tvalid=" << valid_count
           << "\tinvalid=" << invalid_count
           << "\timplementation_defined=" << implementation_count
           << "\tcounts=" << (counts_match ? "ok" : "unexpected")
           << '\n';
    return success;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        std::ostringstream report;

        bool success = run_json_test_suite(report);
        success = verify_corpus(report) && success;
        success = verify_capacity_documents(report) && success;
        success = verify_full_input(report) && success;

        report << "\nresult\t" << (success ? "pass" : "fail") << '\n';
        const std::string text = report.str();
        std::cout << text;

        if (!options.report_path.empty()) {
            std::ofstream output(options.report_path);
            if (!output) {
                throw std::runtime_error(
                    "unable to open report output");
            }
            output << text;
        }
        return success ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "conformance setup failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
