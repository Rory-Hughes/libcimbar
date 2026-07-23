if(NOT DEFINED TEST_BINARY_DIR OR NOT DEFINED VERIFIER)
	message(FATAL_ERROR "Verifier rejection test is missing required paths")
endif()

set(FORBIDDEN_FIXTURE "${TEST_BINARY_DIR}/forbidden-hardened-artifact.txt")
file(WRITE "${FORBIDDEN_FIXTURE}" "synthetic zstd dependency marker")

execute_process(
	COMMAND ${CMAKE_COMMAND}
		-DARTIFACT=${FORBIDDEN_FIXTURE}
		-P ${VERIFIER}
	RESULT_VARIABLE VERIFY_RESULT
	OUTPUT_QUIET
	ERROR_QUIET
)

file(REMOVE "${FORBIDDEN_FIXTURE}")

if(VERIFY_RESULT EQUAL 0)
	message(FATAL_ERROR "Hardened artifact verifier accepted a forbidden zstd marker")
endif()

message(STATUS "Hardened artifact verifier rejected the forbidden fixture")
