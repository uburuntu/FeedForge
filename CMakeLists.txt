cmake_minimum_required(VERSION 3.25)

project(
  FeedForge
  VERSION 0.5.0
  DESCRIPTION "Ahead-of-time compiler for checked market-data decode pipelines"
  HOMEPAGE_URL "https://github.com/uburuntu/FeedForge"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckCXXSourceCompiles)

option(FEEDFORGE_BUILD_COMPILER "Build the feedforgec host compiler" ${PROJECT_IS_TOP_LEVEL})
option(FEEDFORGE_BUILD_TESTS "Build FeedForge tests" ${PROJECT_IS_TOP_LEVEL})
option(FEEDFORGE_BUILD_EXAMPLES "Build FeedForge examples" ${PROJECT_IS_TOP_LEVEL})
option(FEEDFORGE_BUILD_FUZZERS "Build libFuzzer targets" OFF)
option(FEEDFORGE_BUILD_BENCHMARKS "Build benchmark evaluation tools" OFF)
option(FEEDFORGE_WARNINGS_AS_ERRORS "Treat warnings in FeedForge targets as errors" OFF)
option(FEEDFORGE_ENABLE_ASAN "Enable AddressSanitizer for FeedForge targets" OFF)
option(FEEDFORGE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for FeedForge targets" OFF)
option(FEEDFORGE_ENABLE_RTSAN "Enable RealtimeSanitizer for FeedForge targets" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FeedForgeWarnings)
feedforge_detect_rtsan()

if(PROJECT_IS_TOP_LEVEL)
  set(
    FEEDFORGE_CONFORMANCE_OUTPUT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/conformance"
    CACHE PATH
    "Output directory for generated synthetic conformance bundles"
  )
  find_package(Python3 3.11 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    set(
      FEEDFORGE_CONFORMANCE_BUNDLE_ROOT
      "${FEEDFORGE_CONFORMANCE_OUTPUT_DIR}/feedforge-itch50-conformance-v1"
    )
    set(
      _feedforge_conformance_tar
      "${FEEDFORGE_CONFORMANCE_OUTPUT_DIR}/feedforge-itch50-conformance-v1.tar.gz"
    )
    set(
      _feedforge_conformance_zip
      "${FEEDFORGE_CONFORMANCE_OUTPUT_DIR}/feedforge-itch50-conformance-v1.zip"
    )
    set(
      _feedforge_conformance_schema
      "${CMAKE_CURRENT_SOURCE_DIR}/schemas/nasdaq/totalview_itch_5_0.toml"
    )
    set(
      _feedforge_conformance_all_pipeline
      "${CMAKE_CURRENT_SOURCE_DIR}/pipelines/all_messages.toml"
    )
    set(
      _feedforge_conformance_order_pipeline
      "${CMAKE_CURRENT_SOURCE_DIR}/pipelines/order_events.toml"
    )
    file(
      GLOB _feedforge_conformance_fixtures
      CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/itch50/*.toml"
    )
    add_custom_command(
      OUTPUT
        "${FEEDFORGE_CONFORMANCE_BUNDLE_ROOT}/manifest.json"
        "${_feedforge_conformance_tar}"
        "${_feedforge_conformance_zip}"
      COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_conformance_bundle.py"
        --fixtures-dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/itch50"
        --source-lock "${CMAKE_CURRENT_SOURCE_DIR}/schemas/sources.lock.toml"
        --license "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        --schema "${_feedforge_conformance_schema}"
        --all-messages-pipeline "${_feedforge_conformance_all_pipeline}"
        --order-events-pipeline "${_feedforge_conformance_order_pipeline}"
        --output-dir "${FEEDFORGE_CONFORMANCE_OUTPUT_DIR}"
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_conformance_bundle.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/schemas/sources.lock.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${_feedforge_conformance_schema}"
        "${_feedforge_conformance_all_pipeline}"
        "${_feedforge_conformance_order_pipeline}"
        ${_feedforge_conformance_fixtures}
      COMMENT "Generating deterministic synthetic ITCH 5.0 conformance archives"
      VERBATIM
    )
    add_custom_target(
      feedforge_conformance_bundle_assets
      DEPENDS
        "${FEEDFORGE_CONFORMANCE_BUNDLE_ROOT}/manifest.json"
        "${_feedforge_conformance_tar}"
        "${_feedforge_conformance_zip}"
    )
    add_custom_target(conformance-bundle)
    add_dependencies(
      conformance-bundle
      feedforge_conformance_bundle_assets
    )
  else()
    message(
      STATUS
      "Python 3.11 or newer was not found; conformance-bundle will provide setup guidance when built"
    )
    add_custom_target(
      conformance-bundle
      COMMAND
        "${CMAKE_COMMAND}" -E echo
        "FeedForge conformance-bundle requires Python 3.11 or newer. Install it and reconfigure this build tree."
      COMMAND "${CMAKE_COMMAND}" -E false
      COMMENT "Cannot generate the conformance bundle without Python 3.11 or newer"
      VERBATIM
    )
  endif()
endif()

function(feedforge_check_runtime_features)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED YES)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_compiles(
    [=[
      #include <bit>
      #include <concepts>
      #include <cstddef>
      #include <span>

      template <class T>
      concept byte_span = requires(T value) {
        { std::span<const std::byte>{value} };
      };

      static_assert(std::endian::native == std::endian::little ||
                    std::endian::native == std::endian::big);

      int main() {
        const std::byte bytes[1]{};
        static_assert(byte_span<decltype(bytes)&>);
        return std::span<const std::byte>{bytes}.empty() ? 1 : 0;
      }
    ]=]
    FEEDFORGE_HAS_REQUIRED_CXX20_LIBRARY
  )
  if(NOT FEEDFORGE_HAS_REQUIRED_CXX20_LIBRARY)
    message(
      FATAL_ERROR
      "FeedForge::runtime requires a C++20 compiler and standard library with "
      "concepts, std::byte, std::endian, and std::span"
    )
  endif()
endfunction()

feedforge_check_runtime_features()

add_library(feedforge_runtime INTERFACE)
add_library(FeedForge::runtime ALIAS feedforge_runtime)
set_target_properties(
  feedforge_runtime
  PROPERTIES
    EXPORT_NAME runtime
    CXX_EXTENSIONS OFF
)
target_compile_features(feedforge_runtime INTERFACE cxx_std_20)
target_include_directories(
  feedforge_runtime
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set(_feedforge_canonical_generated_targets)
foreach(_pipeline IN ITEMS itch50_all itch50_order_events)
  set(_target "feedforge_generated_${_pipeline}")
  add_library("${_target}" INTERFACE)
  add_library("FeedForge::generated::${_pipeline}" ALIAS "${_target}")
  set_target_properties(
    "${_target}"
    PROPERTIES
      EXPORT_NAME "generated::${_pipeline}"
      CXX_EXTENSIONS OFF
  )
  target_compile_features("${_target}" INTERFACE cxx_std_20)
  target_include_directories(
    "${_target}"
    INTERFACE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )
  target_link_libraries("${_target}" INTERFACE FeedForge::runtime)
  list(APPEND _feedforge_canonical_generated_targets "${_target}")
endforeach()

set(FeedForge_SCHEMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/schemas")
set_property(
  GLOBAL PROPERTY FEEDFORGE_SCHEMA_DIR "${FeedForge_SCHEMA_DIR}"
)
include(FeedForgeGenerate)

if(FEEDFORGE_BUILD_COMPILER)
  add_subdirectory(src/feedforgec)

  file(
    GLOB FeedForge_GENERATOR_INPUTS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/feedforgec/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/feedforgec/*.hpp"
  )
  list(
    APPEND FeedForge_GENERATOR_INPUTS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/feedforge/version.hpp"
  )

  set(_feedforge_schema
      "${CMAKE_CURRENT_SOURCE_DIR}/schemas/nasdaq/totalview_itch_5_0.toml")
  set(_feedforge_generated_dir
      "${CMAKE_CURRENT_SOURCE_DIR}/generated/include/feedforge/generated/nasdaq")
  set(_feedforge_check_dir
      "${CMAKE_CURRENT_BINARY_DIR}/check-generated/feedforge/generated/nasdaq")
  set(_feedforge_all_pipeline
      "${CMAKE_CURRENT_SOURCE_DIR}/pipelines/all_messages.toml")
  set(_feedforge_order_pipeline
      "${CMAKE_CURRENT_SOURCE_DIR}/pipelines/order_events.toml")

  add_custom_target(
    regenerate
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_feedforge_generated_dir}"
    COMMAND
      "$<TARGET_FILE:FeedForge::compiler>" compile
      --schema "${_feedforge_schema}"
      --pipeline "${_feedforge_all_pipeline}"
      --output "${_feedforge_generated_dir}/itch50_all.hpp"
    COMMAND
      "$<TARGET_FILE:FeedForge::compiler>" compile
      --schema "${_feedforge_schema}"
      --pipeline "${_feedforge_order_pipeline}"
      --output "${_feedforge_generated_dir}/itch50_order_events.hpp"
    DEPENDS
      ${FeedForge_GENERATOR_INPUTS}
      "${_feedforge_schema}"
      "${_feedforge_all_pipeline}"
      "${_feedforge_order_pipeline}"
    COMMENT "Regenerating committed canonical FeedForge headers"
    USES_TERMINAL
    VERBATIM
  )
  add_dependencies(regenerate feedforgec)

  add_custom_target(
    check-generated
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_feedforge_check_dir}"
    COMMAND
      "$<TARGET_FILE:FeedForge::compiler>" compile
      --schema "${_feedforge_schema}"
      --pipeline "${_feedforge_all_pipeline}"
      --output "${_feedforge_check_dir}/itch50_all.hpp"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${_feedforge_check_dir}/itch50_all.hpp"
      "${_feedforge_generated_dir}/itch50_all.hpp"
    COMMAND
      "$<TARGET_FILE:FeedForge::compiler>" compile
      --schema "${_feedforge_schema}"
      --pipeline "${_feedforge_order_pipeline}"
      --output "${_feedforge_check_dir}/itch50_order_events.hpp"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${_feedforge_check_dir}/itch50_order_events.hpp"
      "${_feedforge_generated_dir}/itch50_order_events.hpp"
    DEPENDS
      ${FeedForge_GENERATOR_INPUTS}
      "${_feedforge_schema}"
      "${_feedforge_all_pipeline}"
      "${_feedforge_order_pipeline}"
      "${_feedforge_generated_dir}/itch50_all.hpp"
      "${_feedforge_generated_dir}/itch50_order_events.hpp"
    COMMENT "Checking committed generated headers byte-for-byte"
    USES_TERMINAL
    VERBATIM
  )
  add_dependencies(check-generated feedforgec)
endif()

if(FEEDFORGE_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()

if(FEEDFORGE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(FEEDFORGE_BUILD_FUZZERS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/CMakeLists.txt")
    add_subdirectory(fuzz)
  else()
    message(STATUS "FEEDFORGE_BUILD_FUZZERS is enabled, but no fuzz targets exist yet")
  endif()
endif()

if(FEEDFORGE_BUILD_BENCHMARKS)
  include(CTest)
  add_subdirectory(benchmarks)
endif()

install(
  TARGETS
    feedforge_runtime
    ${_feedforge_canonical_generated_targets}
  EXPORT FeedForgeTargets
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
  install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Development
    FILES_MATCHING PATTERN "*.hpp"
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/include")
  install(
    DIRECTORY generated/include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Development
    FILES_MATCHING PATTERN "*.hpp"
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/schemas")
  install(
    DIRECTORY schemas/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/feedforge/schemas"
    COMPONENT Development
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pipelines")
  install(
    DIRECTORY pipelines/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/feedforge/pipelines"
    COMPONENT Development
  )
endif()

set(_feedforge_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/FeedForge")
configure_package_config_file(
  cmake/FeedForgeConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/FeedForgeConfig.cmake"
  INSTALL_DESTINATION "${_feedforge_package_dir}"
  PATH_VARS CMAKE_INSTALL_DATADIR
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FeedForgeConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)

install(
  EXPORT FeedForgeTargets
  FILE FeedForgeTargets.cmake
  NAMESPACE FeedForge::
  DESTINATION "${_feedforge_package_dir}"
  COMPONENT Development
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FeedForgeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FeedForgeConfigVersion.cmake"
    cmake/FeedForgeGenerate.cmake
  DESTINATION "${_feedforge_package_dir}"
  COMPONENT Development
)
install(
  FILES
    LICENSE
    README.md
    RELEASE_NOTES.md
    CONTRIBUTING.md
    SECURITY.md
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge"
  COMPONENT Development
)
install(
  DIRECTORY docs/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge/docs"
  COMPONENT Development
)
# Preserve source-relative links in the installed documentation bundle.
install(
  FILES CMakePresets.json
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge"
  COMPONENT Development
)
install(
  DIRECTORY
    pipelines
    schemas
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge"
  COMPONENT Development
)
install(
  DIRECTORY tests/fixtures/itch50
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge/tests/fixtures"
  COMPONENT Development
)
install(
  DIRECTORY .github/workflows
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge/.github"
  COMPONENT Development
)
install(
  DIRECTORY
    .cursor/skills/feedforge-adoption
    .cursor/skills/feedforge-pipeline-authoring
  DESTINATION "${CMAKE_INSTALL_DATADIR}/feedforge/skills"
  COMPONENT Development
  FILES_MATCHING
    PATTERN "*.md"
    PATTERN ".*" EXCLUDE
)
install(
  DIRECTORY examples/consumer-template
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/feedforge/examples"
  COMPONENT Development
)
