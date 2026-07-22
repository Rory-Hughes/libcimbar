cmake_minimum_required(VERSION 3.10)

add_library(cimbar_hardened_transport STATIC
	HardenedFountainTransport.cpp
	HardenedFountainTransport.h
)

target_include_directories(cimbar_hardened_transport PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${libcimbar_SOURCE_DIR}/src/lib
)

target_link_libraries(cimbar_hardened_transport PUBLIC
	wirehair
)

get_target_property(HARDENED_TRANSPORT_LINKS cimbar_hardened_transport LINK_LIBRARIES)
if(NOT "${HARDENED_TRANSPORT_LINKS}" STREQUAL "wirehair")
	message(FATAL_ERROR
		"cimbar_hardened_transport must link only Wirehair; got: ${HARDENED_TRANSPORT_LINKS}"
	)
endif()

find_program(LIBCIMBAR_STRINGS_TOOL NAMES llvm-strings strings)
set(HARDENED_ARTIFACT_VERIFIER
	${libcimbar_SOURCE_DIR}/cmake/VerifyHardenedTransportArtifact.cmake
)

add_custom_command(TARGET cimbar_hardened_transport POST_BUILD
	COMMAND ${CMAKE_COMMAND}
		-DARTIFACT=$<TARGET_FILE:cimbar_hardened_transport>
		-DNM_TOOL=${CMAKE_NM}
		-DSTRINGS_TOOL=${LIBCIMBAR_STRINGS_TOOL}
		-P ${HARDENED_ARTIFACT_VERIFIER}
	VERBATIM
)

if(NOT DEFINED DISABLE_TESTS)
	add_executable(hardened_transport_profile_test
		test/HardenedFountainTransportTest.cpp
	)
	target_link_libraries(hardened_transport_profile_test
		cimbar_hardened_transport
	)
	add_test(hardened_transport_profile_test hardened_transport_profile_test)
	add_test(
		NAME hardened_transport_verifier_rejects_forbidden
		COMMAND ${CMAKE_COMMAND}
			-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
			-DVERIFIER=${HARDENED_ARTIFACT_VERIFIER}
			-P ${libcimbar_SOURCE_DIR}/cmake/TestHardenedTransportVerifierRejects.cmake
	)

	add_custom_command(TARGET hardened_transport_profile_test POST_BUILD
		COMMAND ${CMAKE_COMMAND}
			-DARTIFACT=$<TARGET_FILE:hardened_transport_profile_test>
			-DNM_TOOL=${CMAKE_NM}
			-DSTRINGS_TOOL=${LIBCIMBAR_STRINGS_TOOL}
			-P ${HARDENED_ARTIFACT_VERIFIER}
		VERBATIM
	)

	add_custom_target(verify_hardened_transport ALL
		COMMAND ${CMAKE_COMMAND}
			-DARTIFACT=$<TARGET_FILE:cimbar_hardened_transport>
			-DNM_TOOL=${CMAKE_NM}
			-DSTRINGS_TOOL=${LIBCIMBAR_STRINGS_TOOL}
			-P ${HARDENED_ARTIFACT_VERIFIER}
		COMMAND ${CMAKE_COMMAND}
			-DARTIFACT=$<TARGET_FILE:hardened_transport_profile_test>
			-DNM_TOOL=${CMAKE_NM}
			-DSTRINGS_TOOL=${LIBCIMBAR_STRINGS_TOOL}
			-P ${HARDENED_ARTIFACT_VERIFIER}
		DEPENDS
			cimbar_hardened_transport
			hardened_transport_profile_test
			${HARDENED_ARTIFACT_VERIFIER}
		VERBATIM
	)
else()
	add_custom_target(verify_hardened_transport ALL
		COMMAND ${CMAKE_COMMAND}
			-DARTIFACT=$<TARGET_FILE:cimbar_hardened_transport>
			-DNM_TOOL=${CMAKE_NM}
			-DSTRINGS_TOOL=${LIBCIMBAR_STRINGS_TOOL}
			-P ${HARDENED_ARTIFACT_VERIFIER}
		DEPENDS
			cimbar_hardened_transport
			${HARDENED_ARTIFACT_VERIFIER}
		VERBATIM
	)
endif()
