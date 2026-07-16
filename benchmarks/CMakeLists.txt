set(_feedforge_bench_config_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_feedforge_bench_config_dir}")

string(TOUPPER "${CMAKE_BUILD_TYPE}" _feedforge_bench_build_type_upper)
set(
  _feedforge_bench_config_flags
  "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_feedforge_bench_build_type_upper}}"
)
string(STRIP "${_feedforge_bench_config_flags}" _feedforge_bench_config_flags)

set(_feedforge_bench_target_flags)
if(MSVC)
  set(_feedforge_bench_target_flags "/W4 /permissive-")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
  set(
    _feedforge_bench_target_flags
    "-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion"
  )
endif()
if(FEEDFORGE_WARNINGS_AS_ERRORS)
  string(APPEND _feedforge_bench_target_flags " warnings-as-errors")
endif()

set(FEEDFORGE_BENCH_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
set(FEEDFORGE_BENCH_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
set(FEEDFORGE_BENCH_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
set(FEEDFORGE_BENCH_COMPILER_PATH "${CMAKE_CXX_COMPILER}")
set(FEEDFORGE_BENCH_GENERATOR "${CMAKE_GENERATOR}")
set(FEEDFORGE_BENCH_CONFIG_FLAGS "${_feedforge_bench_config_flags}")
set(FEEDFORGE_BENCH_TARGET_FLAGS "${_feedforge_bench_target_flags}")
set(FEEDFORGE_BENCH_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}")
set(FEEDFORGE_BENCH_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
set(FEEDFORGE_BENCH_IPO "${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
set(
  FEEDFORGE_BENCH_FIXTURE_DIR
  "${PROJECT_SOURCE_DIR}/tests/fixtures/itch50"
)
configure_file(
  build_config.hpp.in
  "${_feedforge_bench_config_dir}/feedforge_benchmark_build_config.hpp"
  @ONLY
)

add_executable(
  feedforge_benchmark
  feedforge_benchmark.cpp
  benchmark_support.cpp
)
target_compile_features(feedforge_benchmark PRIVATE cxx_std_20)
target_include_directories(
  feedforge_benchmark
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${_feedforge_bench_config_dir}"
)
target_link_libraries(
  feedforge_benchmark
  PRIVATE
    FeedForge::generated::itch50_all
    FeedForge::generated::itch50_order_events
)
feedforge_configure_project_target(feedforge_benchmark)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(
    WARNING
    "FeedForge benchmark results from '${CMAKE_BUILD_TYPE}' are diagnostic only; "
    "use the bench preset for the frozen Release baseline"
  )
endif()

if(BUILD_TESTING)
  add_test(
    NAME benchmark.smoke
    COMMAND "$<TARGET_FILE:feedforge_benchmark>" --smoke
  )
  set_tests_properties(
    benchmark.smoke
    PROPERTIES
      LABELS benchmark
      TIMEOUT 30
  )
endif()
