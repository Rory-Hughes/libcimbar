cmake_minimum_required(VERSION 3.19)
project(libcimbar_security_fuzzers LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBCIMBAR_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "Path to the libcimbar source root")

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The initial fuzzing scaffold requires Clang/libFuzzer")
endif()

function(add_libcimbar_fuzzer target)
    add_executable(${target} ${ARGN})
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

function(add_libcimbar_sanitizers target)
    if(TARGET ${target})
        target_compile_options(${target} PRIVATE
            -fno-omit-frame-pointer
            -fsanitize=address,undefined
        )
    endif()
endfunction()

add_libcimbar_fuzzer(
    fuzz_fountain_metadata
    fuzz_fountain_metadata.cpp
)

target_include_directories(fuzz_fountain_metadata PRIVATE
    "${LIBCIMBAR_ROOT}/src/lib/fountain"
)

add_subdirectory(
    "${LIBCIMBAR_ROOT}/src/third_party_lib/wirehair"
    "${CMAKE_CURRENT_BINARY_DIR}/wirehair"
)
target_compile_options(wirehair PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
)

add_libcimbar_fuzzer(
    fuzz_fountain_state
    fuzz_fountain_state.cpp
)

target_include_directories(fuzz_fountain_state PRIVATE
    "${LIBCIMBAR_ROOT}/src/lib"
    "${LIBCIMBAR_ROOT}/src/lib/fountain"
    "${LIBCIMBAR_ROOT}/src/third_party_lib"
)
target_link_libraries(fuzz_fountain_state PRIVATE wirehair)

add_libcimbar_fuzzer(
    fuzz_frame_sequence
    fuzz_frame_sequence.cpp
)

target_include_directories(fuzz_frame_sequence PRIVATE
    "${LIBCIMBAR_ROOT}/src/lib"
    "${LIBCIMBAR_ROOT}/src/lib/fountain"
    "${LIBCIMBAR_ROOT}/src/third_party_lib"
)
target_link_libraries(fuzz_frame_sequence PRIVATE wirehair)

add_libcimbar_fuzzer(
    fuzz_hardened_ipc
    fuzz_hardened_ipc.cpp
    "${LIBCIMBAR_ROOT}/src/profile/hardened_transport/HardenedTransportIpc.cpp"
)

target_include_directories(fuzz_hardened_ipc PRIVATE
    "${LIBCIMBAR_ROOT}/src/profile/hardened_transport"
)

option(LIBCIMBAR_FUZZ_RAW_FRAME "Build the OpenCV-backed raw-frame geometry fuzzer" ON)

if(LIBCIMBAR_FUZZ_RAW_FRAME)
    find_package(OpenCV QUIET)
    if(OpenCV_FOUND)
        enable_language(C)
        set(DISABLE_TESTS true)
        include_directories(
            ${OpenCV_INCLUDE_DIRS}
            "${LIBCIMBAR_ROOT}/src/lib"
            "${LIBCIMBAR_ROOT}/src/third_party_lib"
        )
        add_subdirectory(
            "${LIBCIMBAR_ROOT}/src/third_party_lib/base91"
            "${CMAKE_CURRENT_BINARY_DIR}/base91"
        )
        add_subdirectory(
            "${LIBCIMBAR_ROOT}/src/third_party_lib/libcorrect"
            "${CMAKE_CURRENT_BINARY_DIR}/libcorrect"
        )
        add_subdirectory(
            "${LIBCIMBAR_ROOT}/src/third_party_lib/zstd"
            "${CMAKE_CURRENT_BINARY_DIR}/zstd"
        )
        add_subdirectory(
            "${LIBCIMBAR_ROOT}/src/lib/cimb_translator"
            "${CMAKE_CURRENT_BINARY_DIR}/cimb_translator"
        )
        add_subdirectory(
            "${LIBCIMBAR_ROOT}/src/lib/extractor"
            "${CMAKE_CURRENT_BINARY_DIR}/extractor"
        )
        foreach(dependency_target
            correct-convolutional
            correct-convolutional-sse
            correct-reed-solomon
            correct_static
            zstd-common
            zstd-compress
            zstd-decompress
            zstd
            cimb_translator
            extractor
        )
            add_libcimbar_sanitizers(${dependency_target})
        endforeach()

        add_libcimbar_fuzzer(
            fuzz_raw_frame
            fuzz_raw_frame.cpp
            "${LIBCIMBAR_ROOT}/src/lib/cimbar_js/cimbar_recv_js.cpp"
        )
        target_include_directories(fuzz_raw_frame PRIVATE
            ${OpenCV_INCLUDE_DIRS}
            "${LIBCIMBAR_ROOT}/src/lib"
            "${LIBCIMBAR_ROOT}/src/third_party_lib"
        )
        target_link_libraries(fuzz_raw_frame PRIVATE
            cimb_translator
            extractor
            base91
            correct_static
            wirehair
            zstd
            ${OpenCV_LIBS}
        )
        if(NOT MSVC)
            target_link_libraries(fuzz_raw_frame PRIVATE stdc++fs)
        endif()
    else()
        message(WARNING "OpenCV not found; skipping fuzz_raw_frame")
    endif()
endif()
