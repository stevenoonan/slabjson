if(NOT SLABJSON_SOURCE_DIR)
    message(FATAL_ERROR "SLABJSON_SOURCE_DIR is required")
endif()
if(NOT SLABJSON_TAG)
    message(FATAL_ERROR "SLABJSON_TAG is required")
endif()

file(READ "${SLABJSON_SOURCE_DIR}/CMakeLists.txt" cmake_contents)
string(REGEX MATCH
    "VERSION[ \t\r\n]+([0-9]+\\.[0-9]+\\.[0-9]+)"
    version_match
    "${cmake_contents}"
)
if(NOT version_match)
    message(FATAL_ERROR "Could not read the project version")
endif()
set(project_version "${CMAKE_MATCH_1}")

if(NOT SLABJSON_TAG STREQUAL "v${project_version}")
    message(FATAL_ERROR
        "Tag ${SLABJSON_TAG} does not match project version v${project_version}"
    )
endif()

if(NOT SLABJSON_CHANGELOG_FILE)
    set(SLABJSON_CHANGELOG_FILE "${SLABJSON_SOURCE_DIR}/CHANGELOG.md")
endif()

file(READ "${SLABJSON_CHANGELOG_FILE}" changelog_contents)
string(FIND
    "${changelog_contents}"
    "## [${project_version}] - "
    changelog_heading
)
if(changelog_heading EQUAL -1)
    message(FATAL_ERROR
        "CHANGELOG.md has no dated ${project_version} release heading"
    )
endif()

message(STATUS "Validated SlabJson ${project_version} release metadata")
