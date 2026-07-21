cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    FEEDFORGE_SOURCE_DIR
    FEEDFORGE_BINARY_DIR
    CONSUMER_SOURCE_DIR
    WORK_DIR
    KIND
    GENERATOR
    CXX_COMPILER)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "run_installed.cmake requires ${_required}")
  endif()
endforeach()

function(run_checked description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(
      FATAL_ERROR
      "${description} failed (${_result})\nstdout:\n${_stdout}\nstderr:\n${_stderr}"
    )
  endif()
endfunction()

function(run_expected_failure description expected)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(_result EQUAL 0)
    message(FATAL_ERROR "${description} unexpectedly succeeded")
  endif()
  string(
    FIND
    "${_stdout}\n${_stderr}"
    "${expected}"
    _expected_message
  )
  if(_expected_message EQUAL -1)
    message(
      FATAL_ERROR
      "${description} did not report '${expected}'\n"
      "stdout:\n${_stdout}\nstderr:\n${_stderr}"
    )
  endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(_prefix "${WORK_DIR}/prefix")
set(_build_config_args)
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  list(APPEND _build_config_args --config "${CONFIG}")
endif()

if(KIND STREQUAL "canonical")
  set(_package_build "${WORK_DIR}/feedforge-build")
  set(_configure_args
      -S "${FEEDFORGE_SOURCE_DIR}"
      -B "${_package_build}"
      -G "${GENERATOR}"
      "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
      "-DCMAKE_INSTALL_PREFIX=${_prefix}"
      -DFEEDFORGE_BUILD_COMPILER=OFF
      -DFEEDFORGE_BUILD_TESTS=OFF
      -DFEEDFORGE_BUILD_EXAMPLES=OFF
      -DFEEDFORGE_BUILD_FUZZERS=OFF
      -DFEEDFORGE_BUILD_BENCHMARKS=OFF)
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
  endif()
  run_checked(
    "runtime-only FeedForge configure"
    "${CMAKE_COMMAND}" ${_configure_args}
  )
  run_checked(
    "runtime-only FeedForge install"
    "${CMAKE_COMMAND}" --build "${_package_build}"
    ${_build_config_args} --target install
  )

  run_expected_failure(
    "compiler-disabled feedforge_generate"
    "feedforge_generate requires FeedForge::compiler"
    "${CMAKE_COMMAND}"
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${WORK_DIR}/compiler-unavailable-build"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DFEEDFORGE_CONSUMER_KIND=generated
  )
elseif(KIND STREQUAL "generated")
  run_checked(
    "compiler-enabled FeedForge install"
    "${CMAKE_COMMAND}" --install "${FEEDFORGE_BINARY_DIR}"
    ${_build_config_args} --prefix "${_prefix}"
  )
  run_expected_failure(
    "ambiguous custom output"
    "OUTPUT filename must be consumer_custom.hpp"
    "${CMAKE_COMMAND}"
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${WORK_DIR}/invalid-output-build"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DFEEDFORGE_CONSUMER_KIND=invalid_output
  )
  run_expected_failure(
    "out-of-tree custom output"
    "OUTPUT must be inside the build tree"
    "${CMAKE_COMMAND}"
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${WORK_DIR}/unsafe-output-build"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DFEEDFORGE_CONSUMER_KIND=unsafe_output
  )
else()
  message(FATAL_ERROR "KIND must be canonical or generated")
endif()

run_expected_failure(
  "pre-1.0 incompatible package request"
  "compatible with requested version \"0.1\""
  "${CMAKE_COMMAND}"
  -S "${CONSUMER_SOURCE_DIR}"
  -B "${WORK_DIR}/incompatible-version-build"
  -G "${GENERATOR}"
  "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DCMAKE_PREFIX_PATH=${_prefix}"
  -DFEEDFORGE_REQUEST_VERSION=0.1
)

set(_consumer_build "${WORK_DIR}/consumer-build")
set(_consumer_configure_args
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${_consumer_build}"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    -DCMAKE_CXX_STANDARD=20
    -DCMAKE_CXX_EXTENSIONS=OFF
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DFEEDFORGE_REQUEST_VERSION=0.3
    "-DFEEDFORGE_CONSUMER_KIND=${KIND}")
if(KIND STREQUAL "canonical")
  list(
    APPEND _consumer_configure_args
    -DFEEDFORGE_REQUIRE_COMPILER_ABSENT=ON
  )
endif()
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  list(APPEND _consumer_configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()
run_checked(
  "${KIND} consumer configure"
  "${CMAKE_COMMAND}" ${_consumer_configure_args}
)
run_checked(
  "${KIND} consumer build and run"
  "${CMAKE_COMMAND}" --build "${_consumer_build}"
  ${_build_config_args} --target run-feedforge-consumer
)

if(KIND STREQUAL "generated")
  set(_legacy_consumer_build "${WORK_DIR}/consumer-legacy-build")
  set(
    _legacy_configure_args
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${_legacy_consumer_build}"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    -DCMAKE_CXX_STANDARD=20
    -DCMAKE_CXX_EXTENSIONS=OFF
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DFEEDFORGE_CONSUMER_KIND=generated_legacy
  )
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _legacy_configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
  endif()
  run_checked(
    "legacy custom output consumer configure"
    "${CMAKE_COMMAND}" ${_legacy_configure_args}
  )
  run_checked(
    "legacy custom output consumer build and run"
    "${CMAKE_COMMAND}" --build "${_legacy_consumer_build}"
    ${_build_config_args} --target run-feedforge-consumer
  )

  set(_source_generated_build
      "${WORK_DIR}/consumer-fetchcontent-generated-build")
  set(
    _source_generated_args
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${_source_generated_build}"
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    -DCMAKE_CXX_STANDARD=20
    -DCMAKE_CXX_EXTENSIONS=OFF
    "-DFEEDFORGE_SOURCE_DIR=${FEEDFORGE_SOURCE_DIR}"
    -DFEEDFORGE_CONSUMER_ACQUISITION=fetchcontent
    -DFEEDFORGE_CONSUMER_KIND=generated
  )
  if(EXISTS "${FEEDFORGE_BINARY_DIR}/_deps/tomlplusplus-src/CMakeLists.txt")
    list(
      APPEND _source_generated_args
      "-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=${FEEDFORGE_BINARY_DIR}/_deps/tomlplusplus-src"
    )
  endif()
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _source_generated_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
  endif()
  run_checked(
    "FetchContent custom generation configure"
    "${CMAKE_COMMAND}" ${_source_generated_args}
  )
  run_checked(
    "FetchContent custom generation build and run"
    "${CMAKE_COMMAND}" --build "${_source_generated_build}"
    ${_build_config_args} --target run-feedforge-consumer
  )
endif()

if(KIND STREQUAL "canonical")
  foreach(_acquisition IN ITEMS subdirectory fetchcontent)
    set(_source_consumer_build
        "${WORK_DIR}/consumer-${_acquisition}-build")
    set(
      _source_configure_args
      -S "${CONSUMER_SOURCE_DIR}"
      -B "${_source_consumer_build}"
      -G "${GENERATOR}"
      "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
      -DCMAKE_CXX_STANDARD=20
      -DCMAKE_CXX_EXTENSIONS=OFF
      "-DFEEDFORGE_SOURCE_DIR=${FEEDFORGE_SOURCE_DIR}"
      "-DFEEDFORGE_CONSUMER_ACQUISITION=${_acquisition}"
      -DFEEDFORGE_CONSUMER_KIND=canonical
      -DFEEDFORGE_REQUIRE_COMPILER_ABSENT=ON
    )
    if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
      list(APPEND _source_configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
    endif()
    run_checked(
      "${_acquisition} canonical consumer configure"
      "${CMAKE_COMMAND}" ${_source_configure_args}
    )
    run_checked(
      "${_acquisition} canonical consumer build and run"
      "${CMAKE_COMMAND}" --build "${_source_consumer_build}"
      ${_build_config_args} --target run-feedforge-consumer
    )
  endforeach()
endif()
