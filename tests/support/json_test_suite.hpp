#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <slabjson/slabjson.hpp>

namespace slabjson::testing {

struct ConformanceResults {
    std::size_t valid{0};
    std::size_t invalid{0};
    std::size_t implementation_defined{0};
    bool accepted_expected_inputs{true};

    [[nodiscard]] bool counts_match() const noexcept
    {
        return valid == 95 && invalid == 188 && implementation_defined == 35;
    }

    [[nodiscard]] bool passed() const noexcept
    {
        return accepted_expected_inputs && counts_match();
    }

    void print_summary(std::ostream& output) const
    {
        output << "summary\tvalid=" << valid << "\tinvalid=" << invalid
               << "\timplementation_defined=" << implementation_defined
               << "\tcounts=" << (counts_match() ? "ok" : "unexpected") << '\n';
    }
};

template <typename ReportCase>
[[nodiscard]] ConformanceResults run_json_test_suite(
    const std::filesystem::path& directory, ReportCase report_case)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    ConformanceResults results;
    for (const auto& path : paths) {
        const auto name = path.filename().string();
        if (name.size() < 2 || name[1] != '_'
            || (name[0] != 'y' && name[0] != 'n' && name[0] != 'i')) {
            continue;
        }
        std::ifstream source(path, std::ios::binary);
        if (!source) {
            throw std::runtime_error("unable to open input: " + path.string());
        }
        const std::string input{std::istreambuf_iterator<char>{source}, {}};
        if (source.bad()) {
            throw std::runtime_error("unable to read input: " + path.string());
        }
        StaticSlab<65535> slab;
        const bool accepted = static_cast<bool>(parse(slab, input));
        switch (name[0]) {
        case 'y':
            ++results.valid;
            results.accepted_expected_inputs &= accepted;
            break;
        case 'n':
            ++results.invalid;
            results.accepted_expected_inputs &= !accepted;
            break;
        case 'i':
            ++results.implementation_defined;
            break;
        }
        report_case(name[0], name, input, accepted);
    }
    return results;
}

} // namespace slabjson::testing
