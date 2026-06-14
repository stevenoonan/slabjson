add_executable(test_slab test_slab.cpp)
target_link_libraries(test_slab PRIVATE slabjson)
add_test(NAME slabjson.slab COMMAND test_slab)

add_executable(test_value test_value.cpp)
target_link_libraries(test_value PRIVATE slabjson)
add_test(NAME slabjson.value COMMAND test_value)

add_executable(test_object test_object.cpp)
target_link_libraries(test_object PRIVATE slabjson)
add_test(NAME slabjson.object COMMAND test_object)

add_executable(test_array test_array.cpp)
target_link_libraries(test_array PRIVATE slabjson)
add_test(NAME slabjson.array COMMAND test_array)

add_executable(test_serialize test_serialize.cpp)
target_link_libraries(test_serialize PRIVATE slabjson)
add_test(NAME slabjson.serialize COMMAND test_serialize)

add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE slabjson)
add_test(NAME slabjson.parse COMMAND test_parse)

add_executable(test_hardening test_hardening.cpp)
target_link_libraries(test_hardening PRIVATE slabjson)
add_test(NAME slabjson.hardening COMMAND test_hardening)
