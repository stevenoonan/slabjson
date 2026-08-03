cmake_minimum_required(VERSION 3.20)

project(slabjson_subproject_consumer LANGUAGES CXX)

if(NOT SLABJSON_SOURCE_DIR)
    message(FATAL_ERROR "SLABJSON_SOURCE_DIR is required")
endif()

set(BUILD_SHARED_LIBS ON)
add_subdirectory("${SLABJSON_SOURCE_DIR}" slabjson)

if(SLABJSON_BUILD_TESTS OR SLABJSON_BUILD_BENCHMARKS)
    message(FATAL_ERROR "SlabJson tooling must be disabled as a subproject")
endif()
if(NOT TARGET slabjson::slabjson)
    message(FATAL_ERROR "The slabjson::slabjson target is required")
endif()
get_target_property(slabjson_target_type slabjson::slabjson TYPE)
if(NOT slabjson_target_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "The subproject SlabJson target must be static")
endif()

add_executable(slabjson_consumer ../consumer_main.cpp)
target_link_libraries(slabjson_consumer PRIVATE slabjson::slabjson)

enable_testing()
add_test(NAME slabjson.consumer.run COMMAND slabjson_consumer)
