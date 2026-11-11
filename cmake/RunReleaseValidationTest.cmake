cmake_minimum_required(VERSION 3.20)

foreach(required_variable
    SLABJSON_SOURCE_DIR SLABJSON_BINARY_DIR SLABJSON_VERSION SLABJSON_TEST_CASE
)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT SLABJSON_VERSION MATCHES "^([0-9]+)[.]([0-9]+)[.]([0-9]+)$")
    message(FATAL_ERROR "Invalid test version: ${SLABJSON_VERSION}")
endif()
set(version_prefix "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
math(EXPR next_patch "${CMAKE_MATCH_3} + 1")

set(known_cases invalid_tag project_version_mismatch missing_changelog_entry)
if(NOT SLABJSON_TEST_CASE IN_LIST known_cases)
    message(FATAL_ERROR "Unknown release validation case: ${SLABJSON_TEST_CASE}")
endif()

function(check_validation tag expected_message expect_success)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSLABJSON_SOURCE_DIR=${fixture_dir}"
            "-DSLABJSON_TAG=${tag}"
            -P "${SLABJSON_SOURCE_DIR}/cmake/ValidateRelease.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(actual "${output}${error}")
    if(expect_success AND NOT result STREQUAL "0")
        message(FATAL_ERROR "Valid release fixture failed: ${actual}")
    elseif(NOT expect_success AND result STREQUAL "0")
        message(FATAL_ERROR "Invalid release fixture was accepted: ${tag}")
    endif()
    string(FIND "${actual}" "${expected_message}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Expected validation message '${expected_message}', got: ${actual}")
    endif()
endfunction()

# Exercise the current project version and a simulated patch bump. Every
# negative case first proves that its fixture passes before changing one input.
foreach(version "${SLABJSON_VERSION}" "${version_prefix}.${next_patch}")
    set(fixture_dir
        "${SLABJSON_BINARY_DIR}/release-fixtures/${SLABJSON_TEST_CASE}/${version}")
    file(MAKE_DIRECTORY "${fixture_dir}")
    file(WRITE "${fixture_dir}/CMakeLists.txt"
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(slabjson VERSION ${version} LANGUAGES CXX)\n")
    file(WRITE "${fixture_dir}/CHANGELOG.md"
        "# Changelog\n\n## [${version}] - 2000-01-01\n\n- Test fixture.\n")
    check_validation("v${version}" "Validated SlabJson ${version}" TRUE)

    if(SLABJSON_TEST_CASE STREQUAL "invalid_tag")
        check_validation("release-${version}" "does not match project version" FALSE)
    elseif(SLABJSON_TEST_CASE STREQUAL "project_version_mismatch")
        string(REGEX MATCH "[0-9]+$" patch "${version}")
        math(EXPR different_patch "${patch} + 1")
        string(REGEX REPLACE "[0-9]+$" "${different_patch}" different_version "${version}")
        check_validation("v${different_version}" "does not match project version" FALSE)
    else()
        configure_file("${SLABJSON_SOURCE_DIR}/tests/fixtures/CHANGELOG-missing-release.md"
            "${fixture_dir}/CHANGELOG.md" COPYONLY)
        check_validation("v${version}" "has no dated ${version} release heading" FALSE)
    endif()
endforeach()
