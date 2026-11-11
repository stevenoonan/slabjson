cmake_minimum_required(VERSION 3.20)

project(slabjson
    VERSION 0.9.1
    DESCRIPTION "A fixed-capacity, allocation-free C++20 JSON library"
    HOMEPAGE_URL "https://github.com/stevenoonan/slabjson"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(slabjson_build_tests_default OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(slabjson_build_tests_default ON)
endif()
option(SLABJSON_BUILD_TESTS
    "Build SlabJson tests"
    ${slabjson_build_tests_default}
)
unset(slabjson_build_tests_default)
option(SLABJSON_BUILD_BENCHMARKS "Build SlabJson host benchmarks" OFF)
option(SLABJSON_BUILD_CONFORMANCE "Build the standalone JSONTestSuite runner" OFF)
option(SLABJSON_BUILD_FUZZERS "Build Clang libFuzzer targets with sanitizers" OFF)
option(SLABJSON_ENABLE_EXCEPTIONS "Allow exceptions" OFF)
option(SLABJSON_ENABLE_LTO "Enable link-time optimization for SlabJson" OFF)
option(SLABJSON_ENABLE_PARSER "Include JSON parsing" ON)
option(SLABJSON_ENABLE_SERIALIZER "Include JSON serialization" ON)
option(SLABJSON_ENABLE_PRETTY "Include pretty JSON output" ON)
option(SLABJSON_ENABLE_CJSON_COMPAT "Include cJSON compatibility helpers" ON)
option(SLABJSON_OPTIMIZE_SIZE "Prefer smaller code over serializer throughput" OFF)
option(SLABJSON_BUILD_FOOTPRINT "Enable the Cortex-M4 footprint report target" OFF)
set(SLABJSON_NUMBER_MODE "FULL" CACHE STRING "Numeric storage: FULL, COMPACT32, INTEGER32")
set_property(CACHE SLABJSON_NUMBER_MODE PROPERTY STRINGS FULL COMPACT32 INTEGER32)
set(SLABJSON_MAX_PARSER_DEPTH "32" CACHE STRING "Maximum compiled parser nesting depth (1..65534)")
if(NOT SLABJSON_NUMBER_MODE MATCHES "^(FULL|COMPACT32|INTEGER32)$")
    message(FATAL_ERROR "Invalid SLABJSON_NUMBER_MODE: ${SLABJSON_NUMBER_MODE}")
endif()
string(LENGTH "${SLABJSON_MAX_PARSER_DEPTH}" slabjson_depth_digits)
if(NOT SLABJSON_MAX_PARSER_DEPTH MATCHES "^[1-9][0-9]*$"
    OR slabjson_depth_digits GREATER 5
    OR SLABJSON_MAX_PARSER_DEPTH GREATER 65534)
    message(FATAL_ERROR "SLABJSON_MAX_PARSER_DEPTH must be an integer in 1..65534")
endif()
set(SLABJSON_FULL_NUMBERS 0)
set(SLABJSON_HAS_FLOAT 1)
if(SLABJSON_NUMBER_MODE STREQUAL "FULL")
    set(SLABJSON_FULL_NUMBERS 1)
elseif(SLABJSON_NUMBER_MODE STREQUAL "INTEGER32")
    set(SLABJSON_HAS_FLOAT 0)
endif()
configure_file(cmake/config.hpp.in generated/slabjson/config.hpp @ONLY)

add_library(slabjson STATIC
    src/array.cpp
    src/object.cpp
    src/slab.cpp
    src/value.cpp
)
foreach(feature PARSER SERIALIZER CJSON_COMPAT)
    if(SLABJSON_ENABLE_${feature})
        if(feature STREQUAL "PARSER")
            target_sources(slabjson PRIVATE src/parse.cpp)
        elseif(feature STREQUAL "SERIALIZER")
            target_sources(slabjson PRIVATE src/serialize.cpp)
        else()
            target_sources(slabjson PRIVATE src/cjson_compat.cpp)
        endif()
    endif()
endforeach()
add_library(slabjson::slabjson ALIAS slabjson)

target_include_directories(slabjson
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(SLABJSON_OPTIMIZE_SIZE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(slabjson PRIVATE -Os -ffunction-sections -fdata-sections)
    elseif(MSVC)
        target_compile_options(slabjson PRIVATE /O1 /Gy /Gw)
    endif()
endif()

if(SLABJSON_BUILD_FOOTPRINT)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(SLABJSON_ARM_CXX arm-none-eabi-g++ REQUIRED)
    add_custom_target(slabjson_footprint_report
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/footprint_report.py"
            --source "${CMAKE_CURRENT_SOURCE_DIR}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/footprint"
            --compiler "${SLABJSON_ARM_CXX}"
        USES_TERMINAL VERBATIM)
endif()

target_compile_features(slabjson PUBLIC cxx_std_20)

set_target_properties(slabjson PROPERTIES
    EXPORT_NAME slabjson
)

if(SLABJSON_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT slabjson_ipo_supported OUTPUT slabjson_ipo_error)
    if(slabjson_ipo_supported)
        set_property(TARGET slabjson PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO requested but not supported: ${slabjson_ipo_error}")
    endif()
endif()

if(NOT SLABJSON_ENABLE_EXCEPTIONS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(slabjson PRIVATE -fno-exceptions -fno-rtti)
    elseif(MSVC)
        target_compile_options(slabjson PRIVATE /EHs-c- /GR-)
        target_compile_definitions(slabjson PRIVATE _HAS_EXCEPTIONS=0)
    endif()
endif()

if(SLABJSON_BUILD_TESTS)
    include(CTest)
endif()

if(SLABJSON_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(SLABJSON_BUILD_BENCHMARKS)
    if(NOT SLABJSON_FULL_NUMBERS OR NOT SLABJSON_ENABLE_PARSER OR NOT SLABJSON_ENABLE_SERIALIZER
        OR NOT SLABJSON_ENABLE_PRETTY OR NOT SLABJSON_ENABLE_CJSON_COMPAT)
        message(FATAL_ERROR "Host benchmarks require FULL numbers and all features")
    endif()
    enable_testing()
    add_subdirectory(benchmarks)
endif()

if(SLABJSON_BUILD_CONFORMANCE)
    if(NOT SLABJSON_ENABLE_PARSER OR NOT SLABJSON_FULL_NUMBERS)
        message(FATAL_ERROR "JSONTestSuite conformance requires the parser and FULL numbers")
    endif()
    include(cmake/JsonTestSuite.cmake)
    enable_testing()
    add_executable(slabjson_json_conformance tests/conformance_main.cpp)
    target_link_libraries(slabjson_json_conformance PRIVATE slabjson::slabjson)
    target_compile_definitions(slabjson_json_conformance PRIVATE
        SLABJSON_JSON_TEST_SUITE_DIR="${SLABJSON_JSON_TEST_SUITE_DIR}")
    add_test(NAME slabjson.conformance COMMAND slabjson_json_conformance)
endif()

if(SLABJSON_BUILD_FUZZERS)
    if(NOT SLABJSON_FULL_NUMBERS OR NOT SLABJSON_ENABLE_PARSER OR NOT SLABJSON_ENABLE_SERIALIZER
        OR NOT SLABJSON_ENABLE_PRETTY OR NOT SLABJSON_ENABLE_CJSON_COMPAT)
        message(FATAL_ERROR "The fuzz harness requires FULL numbers and all features")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "SlabJson fuzz targets require Clang with libFuzzer")
    endif()
    target_compile_options(slabjson PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(slabjson INTERFACE -fsanitize=address,undefined)
    add_subdirectory(fuzz)
endif()

set(slabjson_cmake_install_dir
    "${CMAKE_INSTALL_LIBDIR}/cmake/slabjson"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slabjsonConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slabjsonConfig.cmake"
    INSTALL_DESTINATION "${slabjson_cmake_install_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/slabjsonConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
)

install(TARGETS slabjson
    EXPORT slabjsonTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/slabjson"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/slabjson/config.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/slabjson")

install(EXPORT slabjsonTargets
    FILE slabjsonTargets.cmake
    NAMESPACE slabjson::
    DESTINATION "${slabjson_cmake_install_dir}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slabjsonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slabjsonConfigVersion.cmake"
    DESTINATION "${slabjson_cmake_install_dir}"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
