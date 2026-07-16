foreach(_export_target IN ITEMS
    feedforge_runtime
    feedforge_generated_itch50_all
    feedforge_generated_itch50_order_events)
  foreach(_property IN ITEMS
      INTERFACE_COMPILE_OPTIONS
      INTERFACE_LINK_OPTIONS)
    get_target_property(_exported_options "${_export_target}" "${_property}")
    if(_exported_options)
      message(
        FATAL_ERROR
        "${_export_target} leaks project-only ${_property}: "
        "${_exported_options}"
      )
    endif()
  endforeach()
endforeach()

if(TARGET feedforgec)
  add_test(
    NAME compiler.version
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCOMPILER=$<TARGET_FILE:feedforgec>"
      "-DEXPECTED_VERSION=${PROJECT_VERSION}"
      -P
      "${CMAKE_CURRENT_SOURCE_DIR}/compiler/version.cmake"
  )
  set_tests_properties(
    compiler.version
    PROPERTIES
      TIMEOUT 10
  )

  add_test(NAME compiler.help COMMAND "$<TARGET_FILE:feedforgec>" --help)
  set_tests_properties(
    compiler.help
    PROPERTIES
      PASS_REGULAR_EXPRESSION "Usage:"
      TIMEOUT 10
  )

  add_test(
    NAME compiler.validate_schema
    COMMAND
      "$<TARGET_FILE:feedforgec>"
      validate
      --schema
      "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler/valid_schema.toml"
  )
  set_tests_properties(compiler.validate_schema PROPERTIES TIMEOUT 10)

  add_test(
    NAME compiler.validate_pipeline
    COMMAND
      "$<TARGET_FILE:feedforgec>"
      validate
      --schema
      "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler/valid_schema.toml"
      --pipeline
      "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler/valid_pipeline.toml"
  )
  set_tests_properties(compiler.validate_pipeline PROPERTIES TIMEOUT 10)

  add_test(
    NAME compiler.emit_atomic_deterministic
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCOMPILER=$<TARGET_FILE:feedforgec>"
      "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/compiler_atomic"
      -P
      "${CMAKE_CURRENT_SOURCE_DIR}/compiler/cli_atomic.cmake"
  )
  set_tests_properties(
    compiler.emit_atomic_deterministic
    PROPERTIES TIMEOUT 10
  )
endif()

if(TARGET FeedForge::compiler)
  set(
    _feedforge_source_tree_generated_root
    "${CMAKE_CURRENT_BINARY_DIR}/consumer-custom-output/include"
  )
  set(
    _feedforge_source_tree_generated_header
    "${_feedforge_source_tree_generated_root}/feedforge/generated/consumer_custom.hpp"
  )
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/consumer/custom_pipeline.toml"
    OUTPUT "${_feedforge_source_tree_generated_header}"
  )
  get_target_property(
    _feedforge_source_tree_include_dirs
    feedforge_generated_consumer_custom
    INTERFACE_INCLUDE_DIRECTORIES
  )
  if(NOT _feedforge_source_tree_include_dirs STREQUAL
     _feedforge_source_tree_generated_root)
    message(
      FATAL_ERROR
      "custom OUTPUT exposed '${_feedforge_source_tree_include_dirs}', expected "
      "'${_feedforge_source_tree_generated_root}'"
    )
  endif()
  get_target_property(
    _feedforge_source_tree_header
    feedforge_generated_consumer_custom
    FEEDFORGE_GENERATED_HEADER
  )
  if(NOT _feedforge_source_tree_header STREQUAL
     _feedforge_source_tree_generated_header)
    message(
      FATAL_ERROR
      "custom OUTPUT recorded '${_feedforge_source_tree_header}', expected "
      "'${_feedforge_source_tree_generated_header}'"
    )
  endif()
  add_executable(
    feedforge_test_source_tree_generation
    "${CMAKE_CURRENT_SOURCE_DIR}/consumer/generated_main.cpp"
  )
  set_target_properties(
    feedforge_test_source_tree_generation
    PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED YES
  )
  target_link_libraries(
    feedforge_test_source_tree_generation
    PRIVATE FeedForge::generated::consumer_custom
  )
  feedforge_configure_project_target(feedforge_test_source_tree_generation)
  add_test(
    NAME consumer.source_tree_generation
    COMMAND feedforge_test_source_tree_generation
  )
  set_tests_properties(
    consumer.source_tree_generation
    PROPERTIES TIMEOUT 30
  )

  feedforge_generate(
    NAME hardening_unknown_skip
    SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler/valid_schema.toml"
    PIPELINE
      "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hardening/unknown_skip_pipeline.toml"
  )
  add_executable(
    feedforge_test_hardening_unknown_skip
    "${CMAKE_CURRENT_SOURCE_DIR}/hardening/unknown_skip.cpp"
  )
  target_compile_features(
    feedforge_test_hardening_unknown_skip
    PRIVATE cxx_std_20
  )
  target_include_directories(
    feedforge_test_hardening_unknown_skip
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(
    feedforge_test_hardening_unknown_skip
    PRIVATE FeedForge::generated::hardening_unknown_skip
  )
  feedforge_configure_project_target(
    feedforge_test_hardening_unknown_skip
  )
  add_test(
    NAME hardening.unknown_skip
    COMMAND feedforge_test_hardening_unknown_skip
  )
  set_tests_properties(
    hardening.unknown_skip
    PROPERTIES
      LABELS hardening
      TIMEOUT 30
  )
endif()

set(_feedforge_consumer_kinds canonical)
if(TARGET FeedForge::compiler)
  list(APPEND _feedforge_consumer_kinds generated)
endif()
foreach(_consumer_kind IN LISTS _feedforge_consumer_kinds)
  add_test(
    NAME "consumer.installed_${_consumer_kind}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DFEEDFORGE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DFEEDFORGE_BINARY_DIR=${PROJECT_BINARY_DIR}"
      "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/installed-${_consumer_kind}"
      "-DKIND=${_consumer_kind}"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DCONFIG=$<CONFIG>"
      -P
      "${CMAKE_CURRENT_SOURCE_DIR}/consumer/run_installed.cmake"
  )
  set_tests_properties(
    "consumer.installed_${_consumer_kind}"
    PROPERTIES
      LABELS "integration;installed"
      TIMEOUT 180
  )
endforeach()

set(_feedforge_generated_standards 20)
if(TARGET FeedForge::compiler)
  list(APPEND _feedforge_generated_standards 23)
endif()
foreach(_standard IN LISTS _feedforge_generated_standards)
  set(_target "feedforge_test_generated_cpp${_standard}")
  add_executable(
    "${_target}"
    "${CMAKE_CURRENT_SOURCE_DIR}/golden/generated_semantics.cpp"
  )
  set_target_properties(
    "${_target}"
    PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_STANDARD "${_standard}"
      CXX_STANDARD_REQUIRED YES
  )
  target_include_directories(
    "${_target}"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/golden"
  )
  target_link_libraries("${_target}" PRIVATE FeedForge::runtime)
  feedforge_configure_project_target("${_target}")
  if(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(
      "${_target}"
      PRIVATE
        -Werror
        -fno-exceptions
        -fno-rtti
    )
  elseif(MSVC)
    target_compile_options("${_target}" PRIVATE /WX)
  endif()
  add_test(NAME "compiler.generated_cpp${_standard}" COMMAND "${_target}")
  set_tests_properties(
    "compiler.generated_cpp${_standard}"
    PROPERTIES TIMEOUT 30
  )
endforeach()

function(feedforge_expect_generated_compile_failure name source)
  if(MSVC)
    set(_runtime_include_option "/I${PROJECT_SOURCE_DIR}/include")
  else()
    set(_runtime_include_option "-I${PROJECT_SOURCE_DIR}/include")
  endif()
  try_compile(
    _compiled
    "${CMAKE_CURRENT_BINARY_DIR}/compile_fail_${name}"
    "${CMAKE_CURRENT_SOURCE_DIR}/golden/${source}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_STANDARD=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED=YES"
      "-DCMAKE_CXX_EXTENSIONS=OFF"
    COMPILE_DEFINITIONS "${_runtime_include_option}"
    OUTPUT_VARIABLE _output
  )
  if(_compiled)
    message(
      FATAL_ERROR
      "Generated compile-fail case ${name} unexpectedly compiled"
    )
  endif()
  if(NOT _output MATCHES
     "sink_for_all_selected_events|constraints not satisfied|associated constraints")
    message(
      FATAL_ERROR
      "Generated compile-fail case ${name} did not report a useful sink "
      "constraint diagnostic:\n${_output}"
    )
  endif()
endfunction()

feedforge_expect_generated_compile_failure(
  missing_sink
  compile_fail_missing_sink.cpp
)
feedforge_expect_generated_compile_failure(
  throwing_sink
  compile_fail_throwing_sink.cpp
)
feedforge_expect_generated_compile_failure(
  wrong_result_sink
  compile_fail_wrong_result_sink.cpp
)
feedforge_expect_generated_compile_failure(
  ambiguous_sink
  compile_fail_ambiguous_sink.cpp
)

if(TARGET feedforge_compiler_frontend)
  file(
    GLOB _feedforge_compiler_test_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/compiler/*.cpp"
  )
  foreach(_source IN LISTS _feedforge_compiler_test_sources)
    get_filename_component(_test_stem "${_source}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${_test_stem}" _target_suffix)
    set(_target "feedforge_test_compiler_${_target_suffix}")

    add_executable("${_target}" "${_source}")
    target_compile_features("${_target}" PRIVATE cxx_std_23)
    target_compile_definitions(
      "${_target}"
      PRIVATE
        "FEEDFORGE_COMPILER_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/compiler\""
        "FEEDFORGE_COMPILER_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\""
    )
    target_link_libraries(
      "${_target}"
      PRIVATE
        feedforge_compiler_frontend
        FeedForge::runtime
    )
    feedforge_configure_project_target("${_target}")
    add_test(NAME "compiler.${_test_stem}" COMMAND "${_target}")
    set_tests_properties("compiler.${_test_stem}" PROPERTIES TIMEOUT 30)
  endforeach()
endif()

file(
  GLOB_RECURSE _feedforge_test_sources
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/hardening/*.cpp"
)
list(
  FILTER _feedforge_test_sources
  EXCLUDE REGEX "/integration/generated_headers_compile\\.cpp$"
)
list(
  FILTER _feedforge_test_sources
  EXCLUDE REGEX
    "/hardening/(no_exceptions_rtti|rtsan_smoke|unknown_skip)\\.cpp$"
)

foreach(_source IN LISTS _feedforge_test_sources)
  file(RELATIVE_PATH _relative_source "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
  string(REGEX REPLACE "\\.cpp$" "" _test_name "${_relative_source}")
  string(REPLACE "/" "." _test_name "${_test_name}")
  string(MAKE_C_IDENTIFIER "${_test_name}" _target_suffix)
  set(_target "feedforge_test_${_target_suffix}")

  add_executable("${_target}" "${_source}")
  target_compile_features("${_target}" PRIVATE cxx_std_20)
  target_include_directories("${_target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  if(_relative_source MATCHES "^integration/")
    target_compile_definitions(
      "${_target}"
      PRIVATE
        "FEEDFORGE_ITCH50_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/itch50\""
    )
  endif()
  if(_relative_source MATCHES "^(hardening|integration)/")
    target_link_libraries(
      "${_target}"
      PRIVATE
        FeedForge::generated::itch50_all
        FeedForge::generated::itch50_order_events
    )
  else()
    target_link_libraries("${_target}" PRIVATE FeedForge::runtime)
  endif()
  feedforge_configure_project_target("${_target}")
  add_test(NAME "${_test_name}" COMMAND "${_target}")
  set_tests_properties("${_test_name}" PROPERTIES TIMEOUT 30)
  if(_relative_source MATCHES "^hardening/")
    set_tests_properties("${_test_name}" PROPERTIES LABELS hardening)
  endif()
  if(_test_name STREQUAL "hardening.allocation")
    set_tests_properties(
      "${_test_name}"
      PROPERTIES LABELS "allocation;hardening"
    )
  endif()
endforeach()

function(feedforge_add_standalone_fuzz_smoke name source)
  set(_target "feedforge_test_arbitrary_input_${name}")
  add_executable(
    "${_target}"
    "${PROJECT_SOURCE_DIR}/fuzz/${source}"
  )
  target_compile_features("${_target}" PRIVATE cxx_std_20)
  target_compile_definitions(
    "${_target}"
    PRIVATE FEEDFORGE_FUZZ_STANDALONE=1
  )
  target_link_libraries("${_target}" PRIVATE ${ARGN})
  feedforge_configure_project_target("${_target}")
  add_test(NAME "hardening.arbitrary_input.${name}" COMMAND "${_target}")
  set_tests_properties(
    "hardening.arbitrary_input.${name}"
    PROPERTIES
      LABELS "hardening;arbitrary-input"
      TIMEOUT 30
  )
endfunction()

feedforge_add_standalone_fuzz_smoke(
  binary_file
  fuzz_binary_file.cpp
  FeedForge::runtime
)
feedforge_add_standalone_fuzz_smoke(
  decode_one
  fuzz_decode_one.cpp
  FeedForge::generated::itch50_all
)
feedforge_add_standalone_fuzz_smoke(
  replay
  fuzz_replay.cpp
  FeedForge::generated::itch50_all
)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
  add_executable(
    feedforge_test_no_exceptions_rtti
    "${CMAKE_CURRENT_SOURCE_DIR}/hardening/no_exceptions_rtti.cpp"
  )
  target_compile_features(feedforge_test_no_exceptions_rtti PRIVATE cxx_std_20)
  target_link_libraries(
    feedforge_test_no_exceptions_rtti
    PRIVATE FeedForge::generated::itch50_all
  )
  feedforge_configure_project_target(feedforge_test_no_exceptions_rtti)
  target_compile_options(
    feedforge_test_no_exceptions_rtti
    PRIVATE
      -fno-exceptions
      -fno-rtti
  )
  add_test(
    NAME hardening.no_exceptions_rtti
    COMMAND feedforge_test_no_exceptions_rtti
  )
  set_tests_properties(
    hardening.no_exceptions_rtti
    PROPERTIES
      LABELS "hardening;no-exceptions-rtti"
      TIMEOUT 30
  )
endif()

if(FEEDFORGE_ENABLE_RTSAN AND FEEDFORGE_RTSAN_SUPPORTED)
  add_executable(
    feedforge_test_rtsan_smoke
    "${CMAKE_CURRENT_SOURCE_DIR}/hardening/rtsan_smoke.cpp"
  )
  target_compile_features(feedforge_test_rtsan_smoke PRIVATE cxx_std_20)
  target_link_libraries(
    feedforge_test_rtsan_smoke
    PRIVATE FeedForge::generated::itch50_all
  )
  feedforge_configure_project_target(feedforge_test_rtsan_smoke)
  feedforge_enable_rtsan(feedforge_test_rtsan_smoke)
  add_test(
    NAME hardening.rtsan_smoke
    COMMAND feedforge_test_rtsan_smoke
  )
  set_tests_properties(
    hardening.rtsan_smoke
    PROPERTIES
      LABELS "hardening;rtsan"
      TIMEOUT 30
  )
endif()

foreach(_standard IN LISTS _feedforge_generated_standards)
  set(_target "feedforge_test_canonical_headers_cpp${_standard}")
  add_executable(
    "${_target}"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/generated_headers_compile.cpp"
  )
  set_target_properties(
    "${_target}"
    PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_STANDARD "${_standard}"
      CXX_STANDARD_REQUIRED YES
  )
  target_link_libraries(
    "${_target}"
    PRIVATE
      FeedForge::generated::itch50_all
      FeedForge::generated::itch50_order_events
  )
  feedforge_configure_project_target("${_target}")
  if(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(
      "${_target}"
      PRIVATE
        -Werror
        -fno-exceptions
        -fno-rtti
    )
  elseif(MSVC)
    target_compile_options("${_target}" PRIVATE /WX)
  endif()
  add_test(NAME "integration.canonical_headers_cpp${_standard}" COMMAND "${_target}")
  set_tests_properties(
    "integration.canonical_headers_cpp${_standard}"
    PROPERTIES TIMEOUT 30
  )
endforeach()

if(TARGET feedforge_test_canonical_headers_cpp20 AND
   TARGET feedforge_test_canonical_headers_cpp23)
  add_test(
    NAME integration.generated_layout_cpp20_cpp23
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCPP20=$<TARGET_FILE:feedforge_test_canonical_headers_cpp20>"
      "-DCPP23=$<TARGET_FILE:feedforge_test_canonical_headers_cpp23>"
      -P
      "${CMAKE_CURRENT_SOURCE_DIR}/integration/compare_generated_layouts.cmake"
  )
  set_tests_properties(
    integration.generated_layout_cpp20_cpp23
    PROPERTIES TIMEOUT 30
  )
endif()
