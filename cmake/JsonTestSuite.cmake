include_guard(GLOBAL)

set(SLABJSON_JSON_TEST_SUITE_DIR "" CACHE PATH "Existing JSONTestSuite test_parsing directory")
if(NOT SLABJSON_JSON_TEST_SUITE_DIR)
    include(FetchContent)
    FetchContent_Declare(json_test_suite
        GIT_REPOSITORY https://github.com/nst/JSONTestSuite.git
        GIT_TAG 1ef36fa01286573e846ac449e8683f8833c5b26a
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(json_test_suite)
    set(SLABJSON_JSON_TEST_SUITE_DIR "${json_test_suite_SOURCE_DIR}/test_parsing"
        CACHE PATH "Existing JSONTestSuite test_parsing directory" FORCE)
endif()
if(NOT IS_DIRECTORY "${SLABJSON_JSON_TEST_SUITE_DIR}")
    message(FATAL_ERROR "JSONTestSuite directory is missing: ${SLABJSON_JSON_TEST_SUITE_DIR}")
endif()
