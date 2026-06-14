cmake_minimum_required(VERSION 3.20)

project(slabjson VERSION 0.1.0 LANGUAGES CXX)

option(SLABJSON_BUILD_TESTS "Build SlabJson tests" ON)
option(SLABJSON_ENABLE_EXCEPTIONS "Allow exceptions" OFF)

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

if(NOT SLABJSON_ENABLE_EXCEPTIONS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(slabjson PRIVATE -fno-exceptions -fno-rtti)
    elseif(MSVC)
        target_compile_options(slabjson PRIVATE /GR-)
    endif()
endif()

if(SLABJSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
