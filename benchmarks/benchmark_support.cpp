#include "benchmark_support.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <slabjson/serialize.hpp>
#include <slabjson/static_slab.hpp>

namespace slabjson::benchmarking {
namespace {

[[nodiscard]] bool is_json_whitespace(char character) noexcept
{
    return character == ' '
        || character == '\t'
        || character == '\n'
        || character == '\r';
}

[[nodiscard]] std::vector<std::string> split_tabs(
    std::string_view line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t separator = line.find('\t', start);
        if (separator == std::string_view::npos) {
            fields.emplace_back(line.substr(start));
            return fields;
        }
        fields.emplace_back(line.substr(start, separator - start));
        start = separator + 1;
    }
}

[[nodiscard]] std::size_t parse_size(std::string_view text)
{
    std::size_t value = 0;
    for (char character : text) {
        if (character < '0' || character > '9') {
            throw std::runtime_error("manifest contains an invalid size");
        }
        const std::size_t digit =
            static_cast<std::size_t>(character - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            throw std::runtime_error("manifest size overflows size_t");
        }
        value = value * 10 + digit;
    }
    return value;
}

[[nodiscard]] std::vector<std::string> read_manifest_lines(
    const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "unable to open manifest: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

[[nodiscard]] bool compare_serialized_value(
    cJSON* expected,
    const std::vector<char>& output,
    std::size_t written,
    std::string_view label,
    std::string& failure)
{
    CjsonPtr reparsed = parse_cjson_full(
        std::string_view{output.data(), written});
    if (!reparsed) {
        failure = std::string{label} + " output did not reparse";
        return false;
    }
    if (!cJSON_Compare(expected, reparsed.get(), 1)) {
        failure = std::string{label} + " output changed JSON semantics";
        return false;
    }
    return true;
}

} // namespace

void CjsonDeleter::operator()(cJSON* value) const noexcept
{
    cJSON_Delete(value);
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open input: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("unable to size input: " + path.string());
    }
    std::string result(static_cast<std::size_t>(end), '\0');
    input.seekg(0, std::ios::beg);
    if (!result.empty()) {
        input.read(result.data(), static_cast<std::streamsize>(result.size()));
        if (!input) {
            throw std::runtime_error(
                "unable to read input: " + path.string());
        }
    }
    return result;
}

std::vector<CorpusDocument> load_corpus()
{
    const std::filesystem::path manifest{
        SLABJSON_BENCHMARK_CORPUS_MANIFEST,
    };
    auto lines = read_manifest_lines(manifest);
    if (lines.empty()
        || lines.front() != "profile\tvariant\tpath\tinput_bytes") {
        throw std::runtime_error("unexpected corpus manifest header");
    }

    std::vector<CorpusDocument> documents;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        auto fields = split_tabs(lines[index]);
        if (fields.size() != 4) {
            throw std::runtime_error("invalid corpus manifest row");
        }

        CorpusDocument document{
            std::move(fields[0]),
            std::move(fields[1]),
            manifest.parent_path() / fields[2],
            parse_size(fields[3]),
            {},
        };
        document.input = read_file(document.path);
        if (document.input.size() != document.input_bytes) {
            throw std::runtime_error(
                "corpus manifest size does not match file: "
                + document.path.string());
        }
        documents.push_back(std::move(document));
    }
    return documents;
}

std::vector<CapacityDocument> load_capacity_documents()
{
    const std::filesystem::path manifest{
        SLABJSON_BENCHMARK_CAPACITY_MANIFEST,
    };
    auto lines = read_manifest_lines(manifest);
    if (lines.empty()
        || lines.front() != "name\tpath\tinput_bytes") {
        throw std::runtime_error("unexpected capacity manifest header");
    }

    std::vector<CapacityDocument> documents;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        auto fields = split_tabs(lines[index]);
        if (fields.size() != 3) {
            throw std::runtime_error("invalid capacity manifest row");
        }
        documents.push_back(CapacityDocument{
            std::move(fields[0]),
            std::filesystem::path{fields[1]},
            parse_size(fields[2]),
        });
    }
    return documents;
}

CjsonPtr parse_cjson_full(std::string_view input) noexcept
{
    const char* parse_end = nullptr;
    cJSON* value = cJSON_ParseWithLengthOpts(
        input.data(),
        input.size(),
        &parse_end,
        0);
    if (value == nullptr || parse_end == nullptr) {
        cJSON_Delete(value);
        return {};
    }

    const char* const end = input.data() + input.size();
    while (parse_end < end && is_json_whitespace(*parse_end)) {
        ++parse_end;
    }
    if (parse_end != end) {
        cJSON_Delete(value);
        return {};
    }
    return CjsonPtr{value};
}

bool serialize_slab(
    Value value,
    bool pretty,
    std::vector<char>& output,
    std::size_t& written,
    Error* error)
{
    const auto size_result = pretty
        ? serialized_size_pretty(value)
        : serialized_size(value);
    if (!size_result) {
        if (error != nullptr) {
            *error = size_result.error();
        }
        return false;
    }

    output.assign(size_result.value(), '\0');
    const auto result = pretty
        ? serialize_pretty(value, output)
        : serialize(value, output);
    if (!result) {
        if (error != nullptr) {
            *error = result.error();
        }
        return false;
    }
    written = result.value();
    return true;
}

std::size_t cjson_output_capacity(
    const cJSON* value,
    bool pretty)
{
    char* rendered = pretty
        ? cJSON_Print(value)
        : cJSON_PrintUnformatted(value);
    if (rendered == nullptr) {
        return 0;
    }
    const std::size_t length = std::strlen(rendered);
    cJSON_free(rendered);
    if (length > std::numeric_limits<std::size_t>::max() - 5) {
        return 0;
    }
    return length + 5;
}

bool serialize_cjson(
    cJSON* value,
    bool pretty,
    std::vector<char>& output,
    std::size_t& written)
{
    const std::size_t capacity = cjson_output_capacity(value, pretty);
    if (capacity == 0
        || capacity > static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return false;
    }

    output.assign(capacity, '\0');
    if (!cJSON_PrintPreallocated(
            value,
            output.data(),
            static_cast<int>(output.size()),
            pretty ? 1 : 0)) {
        return false;
    }
    written = std::strlen(output.data());
    return true;
}

bool validate_document(
    const CorpusDocument& document,
    std::string& failure)
{
    StaticSlab<kSlabCapacity> slab;
    auto slab_result = slabjson::parse(slab, document.input);
    if (!slab_result) {
        std::ostringstream message;
        message << "SlabJson parse failed with code "
                << static_cast<int>(slab_result.error().code)
                << " at byte " << slab_result.error().offset;
        failure = message.str();
        return false;
    }

    CjsonPtr cjson_value = parse_cjson_full(document.input);
    if (!cjson_value) {
        failure = "cJSON did not consume the complete input";
        return false;
    }

    for (bool pretty : std::array{false, true}) {
        std::vector<char> slab_output;
        std::size_t slab_written = 0;
        if (!serialize_slab(
                slab_result.value(),
                pretty,
                slab_output,
                slab_written)) {
            failure = pretty
                ? "SlabJson pretty serialization failed"
                : "SlabJson compact serialization failed";
            return false;
        }
        if (!compare_serialized_value(
                cjson_value.get(),
                slab_output,
                slab_written,
                pretty ? "SlabJson pretty" : "SlabJson compact",
                failure)) {
            return false;
        }

        std::vector<char> cjson_output;
        std::size_t cjson_written = 0;
        if (!serialize_cjson(
                cjson_value.get(),
                pretty,
                cjson_output,
                cjson_written)) {
            failure = pretty
                ? "cJSON pretty serialization failed"
                : "cJSON compact serialization failed";
            return false;
        }
        if (!compare_serialized_value(
                cjson_value.get(),
                cjson_output,
                cjson_written,
                pretty ? "cJSON pretty" : "cJSON compact",
                failure)) {
            return false;
        }
    }
    return true;
}

bool is_capacity_error(ErrorCode code) noexcept
{
    return code == ErrorCode::OutOfMemory
        || code == ErrorCode::StringCapacityExceeded;
}

} // namespace slabjson::benchmarking
