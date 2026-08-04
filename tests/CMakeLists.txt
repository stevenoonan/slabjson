add_executable(test_slab test_slab.cpp)
target_link_libraries(test_slab PRIVATE slabjson::slabjson)
add_test(NAME slabjson.slab COMMAND test_slab)

add_executable(test_value test_value.cpp)
target_link_libraries(test_value PRIVATE slabjson::slabjson)
add_test(NAME slabjson.value COMMAND test_value)

add_executable(test_object test_object.cpp)
target_link_libraries(test_object PRIVATE slabjson::slabjson)
add_test(NAME slabjson.object COMMAND test_object)

add_executable(test_array test_array.cpp)
target_link_libraries(test_array PRIVATE slabjson::slabjson)
add_test(NAME slabjson.array COMMAND test_array)

add_executable(test_serialize test_serialize.cpp)
target_link_libraries(test_serialize PRIVATE slabjson::slabjson)
add_test(NAME slabjson.serialize COMMAND test_serialize)

add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE slabjson::slabjson)
add_test(NAME slabjson.parse COMMAND test_parse)

add_executable(test_hardening test_hardening.cpp)
target_link_libraries(test_hardening PRIVATE slabjson::slabjson)
add_test(NAME slabjson.hardening COMMAND test_hardening)

add_executable(test_cjson_compat test_cjson_compat.cpp)
target_link_libraries(test_cjson_compat PRIVATE slabjson::slabjson)
add_test(NAME slabjson.cjson_compat COMMAND test_cjson_compat)

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
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -DSLABJSON_CONSUMER_MODE=installed
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumerTest.cmake"
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
        "-DSLABJSON_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
        -DSLABJSON_CONSUMER_MODE=fetchcontent
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumerTest.cmake"
)

add_test(NAME slabjson.release.valid
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -DSLABJSON_TAG=v0.9.0
        -P "${PROJECT_SOURCE_DIR}/cmake/ValidateRelease.cmake"
)

add_test(NAME slabjson.release.rejects_invalid_tag
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -DSLABJSON_TAG=release-0.9.0
        -P "${PROJECT_SOURCE_DIR}/cmake/ValidateRelease.cmake"
)
set_tests_properties(slabjson.release.rejects_invalid_tag PROPERTIES
    WILL_FAIL TRUE
)

add_test(NAME slabjson.release.rejects_project_version_mismatch
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -DSLABJSON_TAG=v0.9.1
        -P "${PROJECT_SOURCE_DIR}/cmake/ValidateRelease.cmake"
)
set_tests_properties(slabjson.release.rejects_project_version_mismatch PROPERTIES
    WILL_FAIL TRUE
)

add_test(NAME slabjson.release.rejects_missing_changelog_entry
    COMMAND "${CMAKE_COMMAND}"
        "-DSLABJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -DSLABJSON_TAG=v0.9.0
        "-DSLABJSON_CHANGELOG_FILE=${PROJECT_SOURCE_DIR}/tests/fixtures/CHANGELOG-missing-release.md"
        -P "${PROJECT_SOURCE_DIR}/cmake/ValidateRelease.cmake"
)
set_tests_properties(slabjson.release.rejects_missing_changelog_entry
    PROPERTIES WILL_FAIL TRUE
)
