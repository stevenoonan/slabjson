add_executable(test_configuration test_configuration.cpp)
target_link_libraries(test_configuration PRIVATE slabjson::slabjson)
add_test(NAME slabjson.configuration COMMAND test_configuration)

if(SLABJSON_FULL_NUMBERS AND SLABJSON_ENABLE_PARSER AND SLABJSON_ENABLE_SERIALIZER
    AND SLABJSON_ENABLE_PRETTY AND SLABJSON_ENABLE_CJSON_COMPAT
    AND SLABJSON_MAX_PARSER_DEPTH GREATER_EQUAL 32)
    foreach(suite slab value object array serialize parse hardening utf8 construction cjson_compat)
        add_executable(test_${suite} test_${suite}.cpp)
        target_link_libraries(test_${suite} PRIVATE slabjson::slabjson)
        add_test(NAME slabjson.${suite} COMMAND test_${suite})
    endforeach()
endif()

add_executable(test_allocations test_allocations.cpp)
target_link_libraries(test_allocations PRIVATE slabjson::slabjson)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(test_allocations PRIVATE SLABJSON_WRAP_ALLOCATIONS)
    target_link_options(test_allocations PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
endif()
add_test(NAME slabjson.allocations COMMAND test_allocations)

set(slabjson_consumer_configuration_args)
foreach(setting NUMBER_MODE ENABLE_PARSER ENABLE_SERIALIZER ENABLE_PRETTY ENABLE_CJSON_COMPAT OPTIMIZE_SIZE MAX_PARSER_DEPTH)
    list(APPEND slabjson_consumer_configuration_args "-DSLABJSON_${setting}=${SLABJSON_${setting}}")
endforeach()

add_test(NAME slabjson.consumer.installed
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DSLABJSON_CONFIG=$<CONFIG>"
        "-DSLABJSON_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        ${slabjson_consumer_configuration_args}
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -DSLABJSON_CONSUMER_MODE=installed
        "-DSLABJSON_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DSLABJSON_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DSLABJSON_INSTALL_DOCDIR=${CMAKE_INSTALL_DOCDIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumerTest.cmake"
)

add_test(NAME slabjson.consumer.custom_install_dirs
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DSLABJSON_CONFIG=$<CONFIG>"
        "-DSLABJSON_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        ${slabjson_consumer_configuration_args}
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -P "${PROJECT_SOURCE_DIR}/cmake/RunCustomInstallTest.cmake"
)

add_test(NAME slabjson.consumer.subproject
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DSLABJSON_CONFIG=$<CONFIG>"
        "-DSLABJSON_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        ${slabjson_consumer_configuration_args}
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -DSLABJSON_CONSUMER_MODE=subproject
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumerTest.cmake"
)

add_test(NAME slabjson.consumer.fetchcontent
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DSLABJSON_CONFIG=$<CONFIG>"
        "-DSLABJSON_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        ${slabjson_consumer_configuration_args}
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -DSLABJSON_CONSUMER_MODE=fetchcontent
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumerTest.cmake"
)

add_test(NAME slabjson.release.valid
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_TAG=v${PROJECT_VERSION}"
        -P "${PROJECT_SOURCE_DIR}/cmake/ValidateRelease.cmake"
)

foreach(rejection invalid_tag project_version_mismatch missing_changelog_entry)
    add_test(NAME slabjson.release.rejects_${rejection}
        COMMAND "${CMAKE_COMMAND}"
            "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DSLABJSON_VERSION=${PROJECT_VERSION}"
            "-DSLABJSON_TEST_CASE=${rejection}"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunReleaseValidationTest.cmake"
    )
endforeach()

foreach(value 0 -1 1.5 65535 999999999999999999999999999999999999)
    add_test(NAME slabjson.configuration.rejects_depth_${value}
        COMMAND "${CMAKE_COMMAND}"
            "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}/invalid-depth-${value}"
            "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            -DSLABJSON_SETTING=SLABJSON_MAX_PARSER_DEPTH "-DSLABJSON_VALUE=${value}"
            "-DSLABJSON_EXPECTED_MESSAGE=SLABJSON_MAX_PARSER_DEPTH must be an integer in 1..65534"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunConfigurationTest.cmake")
endforeach()
add_test(NAME slabjson.configuration.rejects_number_mode
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DSLABJSON_BINARY_DIR=${PROJECT_BINARY_DIR}/invalid-number-mode"
        "-DSLABJSON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSLABJSON_GENERATOR=${CMAKE_GENERATOR}"
        "-DSLABJSON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DSLABJSON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        -DSLABJSON_SETTING=SLABJSON_NUMBER_MODE -DSLABJSON_VALUE=INVALID
        "-DSLABJSON_EXPECTED_MESSAGE=Invalid SLABJSON_NUMBER_MODE: INVALID"
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConfigurationTest.cmake")
