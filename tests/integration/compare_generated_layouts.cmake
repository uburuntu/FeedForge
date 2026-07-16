cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS CPP20 CPP23)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "compare_generated_layouts.cmake requires ${_required}")
  endif()
endforeach()

execute_process(
  COMMAND "${CPP20}"
  RESULT_VARIABLE _cpp20_result
  OUTPUT_VARIABLE _cpp20_layout
  ERROR_VARIABLE _cpp20_error
)
if(NOT _cpp20_result EQUAL 0)
  message(
    FATAL_ERROR
    "C++20 generated layout probe failed (${_cpp20_result}): ${_cpp20_error}"
  )
endif()

execute_process(
  COMMAND "${CPP23}"
  RESULT_VARIABLE _cpp23_result
  OUTPUT_VARIABLE _cpp23_layout
  ERROR_VARIABLE _cpp23_error
)
if(NOT _cpp23_result EQUAL 0)
  message(
    FATAL_ERROR
    "C++23 generated layout probe failed (${_cpp23_result}): ${_cpp23_error}"
  )
endif()

if(NOT _cpp20_layout STREQUAL _cpp23_layout)
  message(
    FATAL_ERROR
    "Generated event layout changed between C++20 and C++23\n"
    "C++20: ${_cpp20_layout}\n"
    "C++23: ${_cpp23_layout}"
  )
endif()
