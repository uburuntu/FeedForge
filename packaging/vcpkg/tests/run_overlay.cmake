cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR VCPKG_ROOT BUILD_ROOT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "run_overlay.cmake requires -D${_required}=...")
  endif()
endforeach()

if(NOT "$ENV{VCPKG_BINARY_SOURCES}" STREQUAL "clear")
  message(
    FATAL_ERROR
    "Set VCPKG_BINARY_SOURCES=clear so overlay validation cannot reuse binary packages"
  )
endif()

if(CMAKE_HOST_WIN32)
  set(_vcpkg_executable "${VCPKG_ROOT}/vcpkg.exe")
else()
  set(_vcpkg_executable "${VCPKG_ROOT}/vcpkg")
endif()

foreach(_required_file IN ITEMS
    "${SOURCE_DIR}/CMakeLists.txt"
    "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    "${_vcpkg_executable}")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "Required validation input does not exist: ${_required_file}")
  endif()
endforeach()

set(_overlay "${SOURCE_DIR}/packaging/vcpkg/ports")
set(_consumer "${SOURCE_DIR}/tests/consumer")

function(run_checked description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    COMMAND_ECHO STDOUT
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "${description} failed with exit code ${_result}")
  endif()
endfunction()

function(validate_consumer scenario consumer_kind require_compiler_absent)
  set(_build "${BUILD_ROOT}/${scenario}")
  file(REMOVE_RECURSE "${_build}")

  set(_configure
      "${CMAKE_COMMAND}"
      -S "${_consumer}"
      -B "${_build}"
      -G Ninja
      "-DCMAKE_BUILD_TYPE=Release"
      "-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      "-DVCPKG_MANIFEST_DIR=${SOURCE_DIR}/packaging/vcpkg/tests/${scenario}"
      "-DVCPKG_OVERLAY_PORTS=${_overlay}"
      "-DVCPKG_INSTALLED_DIR=${_build}/vcpkg_installed"
      "-DFEEDFORGE_CONSUMER_KIND=${consumer_kind}"
      "-DFEEDFORGE_REQUIRE_COMPILER_ABSENT=${require_compiler_absent}")
  if(DEFINED TARGET_TRIPLET AND NOT "${TARGET_TRIPLET}" STREQUAL "")
    list(APPEND _configure "-DVCPKG_TARGET_TRIPLET=${TARGET_TRIPLET}")
  endif()

  run_checked("Configuring the ${scenario} overlay consumer" ${_configure})
  run_checked(
    "Building and running the ${scenario} overlay consumer"
    "${CMAKE_COMMAND}" --build "${_build}" --target run-feedforge-consumer
    --parallel 2
  )
endfunction()

validate_consumer(runtime canonical ON)
validate_consumer(compiler generated OFF)
