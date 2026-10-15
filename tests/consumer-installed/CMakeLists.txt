cmake_minimum_required(VERSION 3.20)

project(slabjson_installed_consumer LANGUAGES CXX)

set(BUILD_SHARED_LIBS ON)
find_package(slabjson 0.9 CONFIG REQUIRED)

foreach(required_variable
    SLABJSON_INSTALL_PREFIX
    SLABJSON_INSTALL_INCLUDEDIR
    SLABJSON_INSTALL_DOCDIR
)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

get_filename_component(slabjson_include_dir "${SLABJSON_INSTALL_INCLUDEDIR}"
    ABSOLUTE BASE_DIR "${SLABJSON_INSTALL_PREFIX}")
get_filename_component(slabjson_doc_dir "${SLABJSON_INSTALL_DOCDIR}"
    ABSOLUTE BASE_DIR "${SLABJSON_INSTALL_PREFIX}")
foreach(required_path
    "${slabjson_include_dir}/slabjson/slabjson.hpp"
    "${slabjson_DIR}/slabjsonConfig.cmake"
    "${slabjson_DIR}/slabjsonConfigVersion.cmake"
    "${slabjson_DIR}/slabjsonTargets.cmake"
    "${slabjson_doc_dir}/LICENSE"
    "${slabjson_doc_dir}/README.md"
)
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing installed path: ${required_path}")
    endif()
endforeach()

get_target_property(slabjson_imported_configs
    slabjson::slabjson IMPORTED_CONFIGURATIONS)
if(NOT slabjson_imported_configs)
    message(FATAL_ERROR "The installed target has no imported configurations")
endif()
foreach(config IN LISTS slabjson_imported_configs)
    get_target_property(slabjson_archive
        slabjson::slabjson "IMPORTED_LOCATION_${config}")
    if(NOT EXISTS "${slabjson_archive}")
        message(FATAL_ERROR "Missing installed static archive: ${slabjson_archive}")
    endif()
endforeach()

get_target_property(slabjson_target_type slabjson::slabjson TYPE)
if(NOT slabjson_target_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "The installed SlabJson target must be static")
endif()

add_executable(slabjson_consumer ../consumer_main.cpp)
target_link_libraries(slabjson_consumer PRIVATE slabjson::slabjson)

enable_testing()
add_test(NAME slabjson.consumer.run COMMAND slabjson_consumer)
