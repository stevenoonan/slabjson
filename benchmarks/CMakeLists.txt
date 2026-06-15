include(FetchContent)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_language(C)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_DOXYGEN OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG 192ef10025eb2c4cdd392bc502f0c852196baa48
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(google_benchmark)

FetchContent_Declare(
    cjson_source
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG c859b25da02955fef659d658b8f324b5cde87be3
    GIT_SUBMODULES ""
)
FetchContent_GetProperties(cjson_source)
if(NOT cjson_source_POPULATED)
    FetchContent_Populate(cjson_source)
endif()

add_library(slabjson_benchmark_cjson STATIC
    ${cjson_source_SOURCE_DIR}/cJSON.c
)
target_include_directories(slabjson_benchmark_cjson
    PUBLIC
        ${cjson_source_SOURCE_DIR}
)
if(NOT WIN32)
    target_link_libraries(slabjson_benchmark_cjson PUBLIC m)
endif()

FetchContent_Declare(
    json_test_suite
    GIT_REPOSITORY https://github.com/nst/JSONTestSuite.git
    GIT_TAG 1ef36fa01286573e846ac449e8683f8833c5b26a
    GIT_SUBMODULES ""
)
FetchContent_GetProperties(json_test_suite)
if(NOT json_test_suite_POPULATED)
    FetchContent_Populate(json_test_suite)
endif()

FetchContent_Declare(
    nativejson_benchmark
    GIT_REPOSITORY https://github.com/miloyip/nativejson-benchmark.git
    GIT_TAG 478d5727c2a4048e835a29c65adecc7d795360d5
    GIT_SUBMODULES ""
)
FetchContent_GetProperties(nativejson_benchmark)
if(NOT nativejson_benchmark_POPULATED)
    FetchContent_Populate(nativejson_benchmark)
endif()

set(SLABJSON_GENERATED_CORPUS_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/generated-corpus
)
set(SLABJSON_CORPUS_MANIFEST
    ${SLABJSON_GENERATED_CORPUS_DIR}/corpus_manifest.tsv
)
set(SLABJSON_CAPACITY_MANIFEST
    ${SLABJSON_GENERATED_CORPUS_DIR}/capacity_manifest.tsv
)

add_custom_command(
    OUTPUT
        ${SLABJSON_CORPUS_MANIFEST}
        ${SLABJSON_CAPACITY_MANIFEST}
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/generate_corpus.py
        --nativejson-data ${nativejson_benchmark_SOURCE_DIR}/data
        --output ${SLABJSON_GENERATED_CORPUS_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/generate_corpus.py
        ${nativejson_benchmark_SOURCE_DIR}/data/twitter.json
        ${nativejson_benchmark_SOURCE_DIR}/data/citm_catalog.json
        ${nativejson_benchmark_SOURCE_DIR}/data/canada.json
    VERBATIM
)

add_custom_target(slabjson_benchmark_corpus
    DEPENDS
        ${SLABJSON_CORPUS_MANIFEST}
        ${SLABJSON_CAPACITY_MANIFEST}
)

add_library(slabjson_benchmark_support STATIC
    benchmark_support.cpp
)
target_link_libraries(slabjson_benchmark_support
    PUBLIC
        slabjson
        slabjson_benchmark_cjson
)
target_compile_features(slabjson_benchmark_support PUBLIC cxx_std_20)
target_compile_definitions(slabjson_benchmark_support
    PUBLIC
        SLABJSON_BENCHMARK_CORPUS_MANIFEST="${SLABJSON_CORPUS_MANIFEST}"
        SLABJSON_BENCHMARK_CAPACITY_MANIFEST="${SLABJSON_CAPACITY_MANIFEST}"
        SLABJSON_JSON_TEST_SUITE_DIR="${json_test_suite_SOURCE_DIR}/test_parsing"
)
add_dependencies(
    slabjson_benchmark_support
    slabjson_benchmark_corpus
)

function(slabjson_configure_benchmark_target target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endfunction()

slabjson_configure_benchmark_target(slabjson_benchmark_support)

add_executable(slabjson_bench benchmark_main.cpp)
target_link_libraries(slabjson_bench
    PRIVATE
        benchmark::benchmark
        slabjson_benchmark_support
)
slabjson_configure_benchmark_target(slabjson_bench)

add_executable(slabjson_conformance conformance_main.cpp)
target_link_libraries(slabjson_conformance
    PRIVATE
        slabjson_benchmark_support
)
slabjson_configure_benchmark_target(slabjson_conformance)

add_executable(slabjson_memory_report memory_report_main.cpp)
target_link_libraries(slabjson_memory_report
    PRIVATE
        slabjson_benchmark_support
)
slabjson_configure_benchmark_target(slabjson_memory_report)

add_test(
    NAME slabjson.benchmark.conformance
    COMMAND slabjson_conformance
)

add_test(
    NAME slabjson.benchmark.memory
    COMMAND
        slabjson_memory_report
        --output
        ${CMAKE_CURRENT_BINARY_DIR}/memory-smoke.csv
)

foreach(family
    ParseLifecycle
    SerializedSize
    SerializeTransactional
    SerializePartial
    RoundTripCompact
    DuplicateRecursive
)
    add_test(
        NAME slabjson.benchmark.${family}
        COMMAND
            slabjson_bench
            --benchmark_filter=^${family}/
            --benchmark_min_time=1x
            --benchmark_repetitions=1
    )
endforeach()

set(SLABJSON_BENCHMARK_RESULTS_DIR
    ${CMAKE_BINARY_DIR}/benchmark-results
)

add_custom_target(slabjson_benchmark_report
    COMMAND
        ${CMAKE_COMMAND}
        -E make_directory
        ${SLABJSON_BENCHMARK_RESULTS_DIR}
    COMMAND
        $<TARGET_FILE:slabjson_conformance>
        --report
        ${SLABJSON_BENCHMARK_RESULTS_DIR}/conformance.txt
    COMMAND
        $<TARGET_FILE:slabjson_bench>
        --benchmark_min_time=0.05s
        --benchmark_min_warmup_time=0.05
        --benchmark_repetitions=5
        --benchmark_report_aggregates_only=true
        --benchmark_out=${SLABJSON_BENCHMARK_RESULTS_DIR}/timings.json
        --benchmark_out_format=json
    COMMAND
        $<TARGET_FILE:slabjson_memory_report>
        --output
        ${SLABJSON_BENCHMARK_RESULTS_DIR}/memory.csv
    DEPENDS
        slabjson_bench
        slabjson_conformance
        slabjson_memory_report
    USES_TERMINAL
    VERBATIM
)
