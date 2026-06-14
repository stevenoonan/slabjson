#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <cJSON.h>

#include <slabjson/slabjson.hpp>

namespace slabjson::benchmarking {

inline constexpr std::size_t kSlabCapacity = 65535;

struct CorpusDocument {
    std::string profile;
    std::string variant;
    std::filesystem::path path;
    std::size_t input_bytes;
    std::string input;
};

struct CapacityDocument {
    std::string name;
    std::filesystem::path path;
    std::size_t input_bytes;
};

struct CjsonDeleter {
    void operator()(cJSON* value) const noexcept;
};

using CjsonPtr = std::unique_ptr<cJSON, CjsonDeleter>;

[[nodiscard]] std::vector<CorpusDocument> load_corpus();
[[nodiscard]] std::vector<CapacityDocument> load_capacity_documents();
[[nodiscard]] std::string read_file(const std::filesystem::path& path);

[[nodiscard]] CjsonPtr parse_cjson_full(std::string_view input) noexcept;

[[nodiscard]] bool serialize_slab(
    Value value,
    bool pretty,
    std::vector<char>& output,
    std::size_t& written,
    Error* error = nullptr);

[[nodiscard]] bool serialize_cjson(
    cJSON* value,
    bool pretty,
    std::vector<char>& output,
    std::size_t& written);

[[nodiscard]] std::size_t cjson_output_capacity(
    const cJSON* value,
    bool pretty);

[[nodiscard]] bool validate_document(
    const CorpusDocument& document,
    std::string& failure);

[[nodiscard]] bool is_capacity_error(ErrorCode code) noexcept;

} // namespace slabjson::benchmarking
