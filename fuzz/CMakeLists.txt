cmake_minimum_required(VERSION 3.19)
project(libcimbar_security_fuzzers LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBCIMBAR_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "Path to the libcimbar source root")

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The initial fuzzing scaffold requires Clang/libFuzzer")
endif()

function(add_libcimbar_fuzzer target source)
    add_executable(${target} ${source})
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -fno-omit-frame-pointer
        -fsanitize=fuzzer,address,undefined
    )
    target_link_options(${target} PRIVATE
        -fno-omit-frame-pointer
        -fsanitize=fuzzer,address,undefined
    )
endfunction()

add_libcimbar_fuzzer(
    fuzz_fountain_metadata
    fuzz_fountain_metadata.cpp
)

target_include_directories(fuzz_fountain_metadata PRIVATE
    "${LIBCIMBAR_ROOT}/src/lib/fountain"
)
