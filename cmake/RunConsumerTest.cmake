foreach(required_variable
    SLABJSON_SOURCE_DIR
    SLABJSON_BINARY_DIR
    SLABJSON_CXX_COMPILER
    SLABJSON_GENERATOR
    SLABJSON_CONFIG
    SLABJSON_CONSUMER_MODE
)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(consumer_source_dir
    "${SLABJSON_SOURCE_DIR}/tests/consumer-${SLABJSON_CONSUMER_MODE}"
)
set(consumer_work_dir
    "${SLABJSON_BINARY_DIR}/consumer-${SLABJSON_CONSUMER_MODE}-smoke"
)
set(consumer_binary_dir "${consumer_work_dir}/build")

file(REMOVE_RECURSE "${consumer_work_dir}")
file(MAKE_DIRECTORY "${consumer_work_dir}")

set(consumer_arguments)
if(SLABJSON_CONSUMER_MODE STREQUAL "installed")
    set(original_prefix "${consumer_work_dir}/original-prefix")
    set(relocated_prefix "${consumer_work_dir}/relocated-prefix")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${SLABJSON_BINARY_DIR}"
            --config "${SLABJSON_CONFIG}"
            --prefix "${original_prefix}"
        RESULT_VARIABLE install_result
    )
    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR "Installing SlabJson failed: ${install_result}")
    endif()
    file(RENAME "${original_prefix}" "${relocated_prefix}")
    list(APPEND consumer_arguments
        "-DCMAKE_PREFIX_PATH=${relocated_prefix}"
    )
else()
    list(APPEND consumer_arguments
        "-DSLABJSON_SOURCE_DIR=${SLABJSON_SOURCE_DIR}"
    )
endif()

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${consumer_source_dir}"
    -B "${consumer_binary_dir}"
    -G "${SLABJSON_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${SLABJSON_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${SLABJSON_CONFIG}"
    "-DCMAKE_CXX_FLAGS=${SLABJSON_CXX_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS=${SLABJSON_EXE_LINKER_FLAGS}"
    ${consumer_arguments}
)
if(SLABJSON_GENERATOR_PLATFORM)
    list(APPEND configure_command -A "${SLABJSON_GENERATOR_PLATFORM}")
endif()
if(SLABJSON_GENERATOR_TOOLSET)
    list(APPEND configure_command -T "${SLABJSON_GENERATOR_TOOLSET}")
endif()
execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configuring the ${SLABJSON_CONSUMER_MODE} consumer failed: "
        "${configure_result}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_binary_dir}"
        --config "${SLABJSON_CONFIG}"
    RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Building the ${SLABJSON_CONSUMER_MODE} consumer failed: "
        "${build_result}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}"
        --test-dir "${consumer_binary_dir}"
        --build-config "${SLABJSON_CONFIG}"
        --output-on-failure
    RESULT_VARIABLE test_result
)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR
        "Testing the ${SLABJSON_CONSUMER_MODE} consumer failed: "
        "${test_result}"
    )
endif()
