cmake_minimum_required(VERSION 3.10)

project(cimbar_js_test)

set (SOURCES
	test.cpp
	cimbar_jsTest.cpp
	cimbar_recv_jsTest.cpp
)

include_directories(
	${libcimbar_SOURCE_DIR}/test
	${libcimbar_SOURCE_DIR}/test/lib
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable (
	cimbar_js_test
	${SOURCES}
)

add_test(cimbar_js_test cimbar_js_test)

target_link_libraries(cimbar_js_test

	cimbar_js
	cimb_translator
	extractor

	correct_static
	wirehair
	zstd
	${OPENCV_LIBS}
)

add_executable(
	cimbar_raw_frame_cpu_budget
	raw_frame_cpu_budget.cpp
)

target_link_libraries(cimbar_raw_frame_cpu_budget
	cimbar_js
	cimb_translator
	extractor

	correct_static
	wirehair
	zstd
	${OPENCV_LIBS}
)

