add_executable(slabjson_fuzz fuzz_parse.cpp)
target_link_libraries(slabjson_fuzz PRIVATE slabjson::slabjson)
target_compile_options(slabjson_fuzz PRIVATE
    -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
target_link_options(slabjson_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
