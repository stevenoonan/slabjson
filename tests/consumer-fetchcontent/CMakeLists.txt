cmake_minimum_required(VERSION 3.20)

project(slabjson_fetchcontent_consumer LANGUAGES CXX)

include(FetchContent)

if(SLABJSON_SOURCE_DIR)
    FetchContent_Declare(slabjson SOURCE_DIR "${SLABJSON_SOURCE_DIR}")
elseif(SLABJSON_GIT_REPOSITORY AND SLABJSON_GIT_TAG)
    FetchContent_Declare(slabjson
        GIT_REPOSITORY "${SLABJSON_GIT_REPOSITORY}"
        GIT_TAG "${SLABJSON_GIT_TAG}"
        GIT_SHALLOW TRUE
    )
else()
    message(FATAL_ERROR
        "Provide SLABJSON_SOURCE_DIR or SLABJSON_GIT_REPOSITORY and "
        "SLABJSON_GIT_TAG"
    )
endif()

set(BUILD_SHARED_LIBS ON)
FetchContent_MakeAvailable(slabjson)

if(SLABJSON_BUILD_TESTS OR SLABJSON_BUILD_BENCHMARKS)
    message(FATAL_ERROR "SlabJson tooling must be disabled via FetchContent")
endif()
get_target_property(slabjson_target_type slabjson::slabjson TYPE)
if(NOT slabjson_target_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "The FetchContent SlabJson target must be static")
endif()

add_executable(slabjson_consumer ../consumer_main.cpp)
target_link_libraries(slabjson_consumer PRIVATE slabjson::slabjson)

enable_testing()
add_test(NAME slabjson.consumer.run COMMAND slabjson_consumer)
