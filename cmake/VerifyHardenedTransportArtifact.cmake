if(NOT DEFINED ARTIFACT OR NOT EXISTS "${ARTIFACT}")
	message(FATAL_ERROR "Hardened transport artifact does not exist: ${ARTIFACT}")
endif()

set(ARTIFACT_TEXT "")
if(DEFINED NM_TOOL AND EXISTS "${NM_TOOL}")
	execute_process(
		COMMAND "${NM_TOOL}" -C "${ARTIFACT}"
		RESULT_VARIABLE NM_RESULT
		OUTPUT_VARIABLE NM_OUTPUT
		ERROR_VARIABLE NM_ERROR
	)
	if(NOT NM_RESULT EQUAL 0)
		message(FATAL_ERROR "Unable to inspect hardened transport symbols: ${NM_ERROR}")
	endif()
	string(APPEND ARTIFACT_TEXT "\n${NM_OUTPUT}")
endif()

if(DEFINED STRINGS_TOOL AND EXISTS "${STRINGS_TOOL}")
	execute_process(
		COMMAND "${STRINGS_TOOL}" "${ARTIFACT}"
		RESULT_VARIABLE STRINGS_RESULT
		OUTPUT_VARIABLE STRINGS_OUTPUT
		ERROR_VARIABLE STRINGS_ERROR
	)
	if(NOT STRINGS_RESULT EQUAL 0)
		message(FATAL_ERROR "Unable to inspect hardened transport strings: ${STRINGS_ERROR}")
	endif()
	string(APPEND ARTIFACT_TEXT "\n${STRINGS_OUTPUT}")
else()
	file(STRINGS "${ARTIFACT}" STRINGS_OUTPUT)
	list(JOIN STRINGS_OUTPUT "\n" STRINGS_OUTPUT)
	string(APPEND ARTIFACT_TEXT "\n${STRINGS_OUTPUT}")
endif()

string(TOLOWER "${ARTIFACT_TEXT}" ARTIFACT_TEXT)
set(FORBIDDEN_ARTIFACT_TERMS
	zstd
	zstd_header_check
	decompress_on_store
	write_on_store
	std::filesystem
	basic_ifstream
	basic_ofstream
	file::basename
	cimbard_get_filename
	cimbard_decompress
	cimbar_enable_session_test_faults
	inject_test_fault
	testfault
)

foreach(TERM IN LISTS FORBIDDEN_ARTIFACT_TERMS)
	string(FIND "${ARTIFACT_TEXT}" "${TERM}" TERM_POSITION)
	if(NOT TERM_POSITION EQUAL -1)
		message(FATAL_ERROR
			"Forbidden term '${TERM}' is present in hardened transport artifact ${ARTIFACT}"
		)
	endif()
endforeach()

message(STATUS "Verified hardened transport artifact: ${ARTIFACT}")
