cmake_minimum_required(VERSION 3.10)

project(fountain_test)

set (SOURCES
	test.cpp
	FountainEncodingTest.cpp
	FountainMetadataTest.cpp
	concurrent_fountain_decoder_sinkTest.cpp
	fountain_sinkTest.cpp
	fountain_sinkSpecialTest.cpp
	fountain_streamTest.cpp
)

include_directories(
	${libcimbar_SOURCE_DIR}/test
	${libcimbar_SOURCE_DIR}/test/lib
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable (
	fountain_test
	${SOURCES}
)

add_test(fountain_test fountain_test)

add_executable(
	fountain_init_concurrency_test
	FountainInitConcurrencyTest.cpp
	FountainInitConcurrencyPeer.cpp
)

add_test(fountain_init_concurrency_test fountain_init_concurrency_test)

target_compile_definitions(fountain_test PRIVATE
	CIMBAR_ENABLE_SESSION_TEST_FAULTS=1
)

find_package(Threads REQUIRED)

target_link_libraries(fountain_test
	wirehair
	zstd
	Threads::Threads

	${CPPFILESYSTEM}
)

target_link_libraries(fountain_init_concurrency_test
	wirehair
	Threads::Threads
)

