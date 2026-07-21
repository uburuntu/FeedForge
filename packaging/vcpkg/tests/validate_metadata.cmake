cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR PROJECT_VERSION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "validate_metadata.cmake requires -D${_required}=...")
  endif()
endforeach()

set(_baseline "3ddaad9be959816602453ecb05533f8732464ef4")
set(_packaging "${SOURCE_DIR}/packaging/vcpkg")

function(require_equal actual expected description)
  if(NOT "${actual}" STREQUAL "${expected}")
    message(
      FATAL_ERROR
      "${description}: expected '${expected}', found '${actual}'"
    )
  endif()
endfunction()

file(READ "${_packaging}/ports/feedforge/vcpkg.json" _port)
string(JSON _port_name GET "${_port}" name)
string(JSON _port_version GET "${_port}" version)
string(JSON _port_license GET "${_port}" license)
string(JSON _compiler_support GET "${_port}" features compiler supports)
string(JSON _compiler_dependency GET "${_port}" features compiler dependencies 0)
require_equal("${_port_name}" "feedforge" "overlay port name")
require_equal("${_port_version}" "${PROJECT_VERSION}" "overlay port version")
require_equal("${_port_license}" "Apache-2.0" "overlay port license")
require_equal("${_compiler_support}" "native" "compiler feature support expression")
require_equal(
  "${_compiler_dependency}"
  "tomlplusplus"
  "compiler feature dependency"
)

foreach(_manifest IN ITEMS runtime compiler)
  file(READ "${_packaging}/tests/${_manifest}/vcpkg.json" _contents)
  string(JSON _manifest_baseline GET "${_contents}" builtin-baseline)
  string(JSON _dependency_name GET "${_contents}" dependencies 0 name)
  string(JSON _default_features GET "${_contents}" dependencies 0 default-features)
  require_equal(
    "${_manifest_baseline}"
    "${_baseline}"
    "${_manifest} validation baseline"
  )
  require_equal(
    "${_dependency_name}"
    "feedforge"
    "${_manifest} validation dependency"
  )
  require_equal(
    "${_default_features}"
    "OFF"
    "${_manifest} validation default-features"
  )
endforeach()

file(READ "${_packaging}/tests/compiler/vcpkg.json" _compiler_manifest)
string(JSON _compiler_feature GET "${_compiler_manifest}" dependencies 0 features 0)
require_equal("${_compiler_feature}" "compiler" "compiler validation feature")

file(READ "${_packaging}/ports/feedforge/portfile.cmake" _portfile)
foreach(_required_text IN ITEMS
    "CURRENT_PORT_DIR}/../../../.."
    "VCPKG_BUILD_TYPE release"
    "compiler FEEDFORGE_BUILD_COMPILER"
    "FEEDFORGE_TOML_EXCEPTIONS=ON"
    "FEEDFORGE_BUILD_TESTS=OFF"
    "FEEDFORGE_BUILD_EXAMPLES=OFF"
    "FEEDFORGE_BUILD_FUZZERS=OFF"
    "FEEDFORGE_BUILD_BENCHMARKS=OFF"
    "FETCHCONTENT_FULLY_DISCONNECTED=ON"
    "vcpkg_cmake_config_fixup"
    "vcpkg_copy_tools"
    "vcpkg_install_copyright")
  string(FIND "${_portfile}" "${_required_text}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR "overlay portfile is missing: ${_required_text}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/docs/vcpkg.md" _documentation)
foreach(_required_text IN ITEMS
    "${_baseline}"
    "VCPKG_BINARY_SOURCES=clear"
    "packaging/vcpkg/ports"
    "\"compiler\"")
  string(FIND "${_documentation}" "${_required_text}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR "vcpkg documentation is missing: ${_required_text}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/.github/workflows/ci.yml" _workflow)
foreach(_required_text IN ITEMS
    "${_baseline}"
    "  vcpkg-overlay:"
    "      - vcpkg-overlay"
    "VCPKG_BINARY_SOURCES: clear")
  string(FIND "${_workflow}" "${_required_text}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR "CI workflow is missing: ${_required_text}")
  endif()
endforeach()
