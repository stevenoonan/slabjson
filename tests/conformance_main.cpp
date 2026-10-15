#include <exception>
#include <iostream>

#include "support/json_test_suite.hpp"

int main()
{
    try {
        std::cout << "JSONTestSuite\ncategory\tfile\tslabjson\n";
        auto results = slabjson::testing::run_json_test_suite(
            SLABJSON_JSON_TEST_SUITE_DIR,
            [](char category, const auto& name, const auto&, bool accepted) {
                std::cout << category << '\t' << name << '\t'
                          << (accepted ? "accept" : "reject") << '\n';
            });
        results.print_summary(std::cout);
        return results.passed() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "conformance setup failed: " << error.what() << '\n';
        return 1;
    }
}
