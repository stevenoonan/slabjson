cmake_minimum_required(VERSION 3.20)

project(slabjson
    VERSION 0.9.0
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
option(SLABJSON_ENABLE_EXCEPTIONS "Allow exceptions" OFF)
option(SLABJSON_ENABLE_LTO "Enable link-time optimization for SlabJson" OFF)

add_library(slabjson STATIC
    src/array.cpp
    src/cjson_compat.cpp
    src/object.cpp
    src/parse.cpp
    src/serialize.cpp
    src/slab.cpp
    src/value.cpp
)
add_library(slabjson::slabjson ALIAS slabjson)

target_include_directories(slabjson
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

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
    enable_testing()
    add_subdirectory(benchmarks)
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
