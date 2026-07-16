if(NOT DEFINED COMPILER OR NOT DEFINED EXPECTED_VERSION)
  message(FATAL_ERROR "COMPILER and EXPECTED_VERSION are required")
endif()

execute_process(
  COMMAND "${COMPILER}" --version
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "feedforgec --version exited ${result}: ${stderr}")
endif()
if(NOT stderr STREQUAL "")
  message(FATAL_ERROR "feedforgec --version wrote stderr: ${stderr}")
endif()
if(NOT stdout STREQUAL "feedforgec ${EXPECTED_VERSION}\n")
  message(
    FATAL_ERROR
    "feedforgec version identity mismatch: expected exactly "
    "'feedforgec ${EXPECTED_VERSION}\\n', got '${stdout}'"
  )
endif()
