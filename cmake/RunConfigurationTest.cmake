# Each rejection runs in its own disposable directory, without cached settings.
file(REMOVE_RECURSE "${SLABJSON_BINARY_DIR}")
set(configure_command
    "${CMAKE_COMMAND}" -S "${SLABJSON_SOURCE_DIR}" -B "${SLABJSON_BINARY_DIR}"
    -G "${SLABJSON_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${SLABJSON_CXX_COMPILER}"
    -DSLABJSON_BUILD_TESTS=OFF "-D${SLABJSON_SETTING}=${SLABJSON_VALUE}"
)
if(SLABJSON_GENERATOR_PLATFORM)
    list(APPEND configure_command -A "${SLABJSON_GENERATOR_PLATFORM}")
endif()
if(SLABJSON_GENERATOR_TOOLSET)
    list(APPEND configure_command -T "${SLABJSON_GENERATOR_TOOLSET}")
endif()
execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    message(FATAL_ERROR "Invalid ${SLABJSON_SETTING}=${SLABJSON_VALUE} was accepted")
endif()
if(NOT "${output}\n${error}" MATCHES "${SLABJSON_EXPECTED_MESSAGE}")
    message(FATAL_ERROR "Configuration failed for an unexpected reason:\n${output}\n${error}")
endif()
