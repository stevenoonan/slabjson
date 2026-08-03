cmake_minimum_required(VERSION 3.20)

project(slabjson_installed_consumer LANGUAGES CXX)

set(BUILD_SHARED_LIBS ON)
find_package(slabjson 0.9 CONFIG REQUIRED)

get_filename_component(slabjson_prefix "${slabjson_DIR}/../../.." ABSOLUTE)
foreach(required_path
    include/slabjson/slabjson.hpp
    lib/cmake/slabjson/slabjsonConfig.cmake
    lib/cmake/slabjson/slabjsonConfigVersion.cmake
    lib/cmake/slabjson/slabjsonTargets.cmake
    share/doc/slabjson/LICENSE
    share/doc/slabjson/README.md
)
    if(NOT EXISTS "${slabjson_prefix}/${required_path}")
        message(FATAL_ERROR "Missing installed path: ${required_path}")
    endif()
endforeach()

file(GLOB slabjson_archives
    "${slabjson_prefix}/lib/slabjson.lib"
    "${slabjson_prefix}/lib/libslabjson.a"
)
if(NOT slabjson_archives)
    message(FATAL_ERROR "The installed static archive is missing")
endif()

get_target_property(slabjson_target_type slabjson::slabjson TYPE)
if(NOT slabjson_target_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "The installed SlabJson target must be static")
endif()

add_executable(slabjson_consumer ../consumer_main.cpp)
target_link_libraries(slabjson_consumer PRIVATE slabjson::slabjson)

enable_testing()
add_test(NAME slabjson.consumer.run COMMAND slabjson_consumer)
