set(custom_binary_dir "${SLABJSON_BINARY_DIR}/custom-install-library")
set(SLABJSON_INSTALL_LIBDIR "lib64/slabjson")
set(SLABJSON_INSTALL_INCLUDEDIR "headers/sdk")
set(SLABJSON_INSTALL_DOCDIR "docs/slabjson")

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${SLABJSON_SOURCE_DIR}"
    -B "${custom_binary_dir}"
    -G "${SLABJSON_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${SLABJSON_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${SLABJSON_CONFIG}"
    "-DCMAKE_CXX_FLAGS=${SLABJSON_CXX_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS=${SLABJSON_EXE_LINKER_FLAGS}"
    "-DCMAKE_INSTALL_LIBDIR=${SLABJSON_INSTALL_LIBDIR}"
    "-DCMAKE_INSTALL_INCLUDEDIR=${SLABJSON_INSTALL_INCLUDEDIR}"
    "-DCMAKE_INSTALL_DOCDIR=${SLABJSON_INSTALL_DOCDIR}"
    -DSLABJSON_BUILD_TESTS=OFF
    -DSLABJSON_BUILD_BENCHMARKS=OFF
)
if(SLABJSON_GENERATOR_PLATFORM)
    list(APPEND configure_command -A "${SLABJSON_GENERATOR_PLATFORM}")
endif()
if(SLABJSON_GENERATOR_TOOLSET)
    list(APPEND configure_command -T "${SLABJSON_GENERATOR_TOOLSET}")
endif()
execute_process(COMMAND ${configure_command} RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Configuring the custom install failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${custom_binary_dir}"
        --config "${SLABJSON_CONFIG}"
    RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Building the custom install failed: ${build_result}")
endif()

set(SLABJSON_BINARY_DIR "${custom_binary_dir}")
set(SLABJSON_CONSUMER_MODE installed)
include("${SLABJSON_SOURCE_DIR}/cmake/RunConsumerTest.cmake")
