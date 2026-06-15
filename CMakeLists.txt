cmake_minimum_required(VERSION 3.20)

project(slabjson VERSION 0.1.0 LANGUAGES CXX)

option(SLABJSON_BUILD_TESTS "Build SlabJson tests" ON)
option(SLABJSON_BUILD_BENCHMARKS "Build SlabJson host benchmarks" OFF)
option(SLABJSON_ENABLE_EXCEPTIONS "Allow exceptions" OFF)
option(SLABJSON_ENABLE_LTO "Enable link-time optimization for SlabJson" OFF)

add_library(slabjson
    src/array.cpp
    src/cjson_compat.cpp
    src/object.cpp
    src/parse.cpp
    src/serialize.cpp
    src/slab.cpp
    src/value.cpp
)

target_include_directories(slabjson
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(slabjson PUBLIC cxx_std_20)

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
        target_compile_options(slabjson PRIVATE /GR-)
    endif()
endif()

if(SLABJSON_BUILD_TESTS OR SLABJSON_BUILD_BENCHMARKS)
    enable_testing()
endif()

if(SLABJSON_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(SLABJSON_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
