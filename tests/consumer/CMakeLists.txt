cmake_minimum_required(VERSION 3.25)

project(FeedForgeConsumer LANGUAGES CXX)

set(
  FEEDFORGE_CONSUMER_KIND
  canonical
  CACHE STRING
  "Consumer scenario: canonical, generated, or a negative generation case"
)
set(
  FEEDFORGE_CONSUMER_ACQUISITION
  package
  CACHE STRING
  "FeedForge acquisition: package, subdirectory, or fetchcontent"
)
option(
  FEEDFORGE_REQUIRE_COMPILER_ABSENT
  "Require a canonical-only dependency graph"
  OFF
)

if(FEEDFORGE_CONSUMER_ACQUISITION STREQUAL "package")
  find_package(FeedForge CONFIG REQUIRED)

  foreach(_variable IN ITEMS
      FeedForge_SCHEMA_DIR
      FeedForge_DOC_DIR
      FeedForge_SKILL_DIR)
    if(NOT DEFINED ${_variable} OR NOT IS_ABSOLUTE "${${_variable}}")
      message(FATAL_ERROR "${_variable} must be an absolute package path")
    endif()
  endforeach()
  foreach(_doc IN ITEMS
      adoption.md
      adding-a-backend.md
      architecture.md
      generated-api.md
      pipeline-format.md
      schema-format.md
      testing.md)
    if(NOT EXISTS "${FeedForge_DOC_DIR}/${_doc}")
      message(FATAL_ERROR "installed FeedForge document is missing: ${_doc}")
    endif()
  endforeach()
  foreach(_skill_file IN ITEMS
      feedforge-adoption/SKILL.md
      feedforge-adoption/examples.md
      feedforge-adoption/reference.md
      feedforge-pipeline-authoring/SKILL.md
      feedforge-pipeline-authoring/examples.md
      feedforge-pipeline-authoring/reference.md)
    if(NOT EXISTS "${FeedForge_SKILL_DIR}/${_skill_file}")
      message(
        FATAL_ERROR
        "installed FeedForge Agent Skill file is missing: ${_skill_file}"
      )
    endif()
  endforeach()
  foreach(_template_file IN ITEMS
      CMakeLists.txt
      README.md
      custom_main.cpp
      custom_pipeline.toml
      main.cpp)
    if(NOT EXISTS
       "${FeedForge_DOC_DIR}/examples/consumer-template/${_template_file}")
      message(
        FATAL_ERROR
        "installed consumer template file is missing: ${_template_file}"
      )
    endif()
  endforeach()
elseif(FEEDFORGE_CONSUMER_ACQUISITION STREQUAL "subdirectory" OR
       FEEDFORGE_CONSUMER_ACQUISITION STREQUAL "fetchcontent")
  if(NOT DEFINED FEEDFORGE_SOURCE_DIR OR
     NOT EXISTS "${FEEDFORGE_SOURCE_DIR}/CMakeLists.txt")
    message(
      FATAL_ERROR
      "source acquisition requires -DFEEDFORGE_SOURCE_DIR=/path/to/FeedForge"
    )
  endif()
  if(FEEDFORGE_CONSUMER_KIND MATCHES "^generated")
    set(FEEDFORGE_BUILD_COMPILER ON)
  endif()
  if(FEEDFORGE_CONSUMER_ACQUISITION STREQUAL "subdirectory")
    add_subdirectory(
      "${FEEDFORGE_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/feedforge"
    )
  else()
    include(FetchContent)
    FetchContent_Declare(
      feedforge
      SOURCE_DIR "${FEEDFORGE_SOURCE_DIR}"
    )
    FetchContent_MakeAvailable(feedforge)
  endif()
else()
  message(
    FATAL_ERROR
    "FEEDFORGE_CONSUMER_ACQUISITION must be package, subdirectory, or "
    "fetchcontent"
  )
endif()

if(FEEDFORGE_REQUIRE_COMPILER_ABSENT)
  if(TARGET FeedForge::compiler)
    message(FATAL_ERROR "canonical-only consumer unexpectedly sees the compiler")
  endif()
  if(TARGET tomlplusplus::tomlplusplus)
    message(FATAL_ERROR "canonical-only consumer unexpectedly sees toml++")
  endif()
endif()

foreach(_target IN ITEMS
    FeedForge::runtime
    FeedForge::generated::itch50_all
    FeedForge::generated::itch50_order_events)
  if(NOT TARGET "${_target}")
    message(FATAL_ERROR "required canonical target is missing: ${_target}")
  endif()
  get_target_property(_features "${_target}" INTERFACE_COMPILE_FEATURES)
  if(NOT cxx_std_20 IN_LIST _features)
    message(FATAL_ERROR "${_target} does not advertise C++20")
  endif()
  if(cxx_std_23 IN_LIST _features)
    message(FATAL_ERROR "${_target} unexpectedly requires C++23")
  endif()
  foreach(_property IN ITEMS
      INTERFACE_COMPILE_OPTIONS
      INTERFACE_LINK_OPTIONS)
    get_target_property(_options "${_target}" "${_property}")
    if(_options)
      message(FATAL_ERROR "${_target} leaks ${_property}: ${_options}")
    endif()
  endforeach()
  get_target_property(_links "${_target}" INTERFACE_LINK_LIBRARIES)
  if("${_links}" MATCHES "tomlplusplus|FeedForge::compiler")
    message(FATAL_ERROR "${_target} leaks a host-tool dependency: ${_links}")
  endif()
endforeach()

if(FEEDFORGE_CONSUMER_KIND STREQUAL "canonical")
  add_executable(feedforge_consumer main.cpp)
  target_link_libraries(
    feedforge_consumer
    PRIVATE
      FeedForge::generated::itch50_all
      FeedForge::generated::itch50_order_events
  )
elseif(FEEDFORGE_CONSUMER_KIND STREQUAL "generated")
  set(
    _custom_include_root
    "${CMAKE_CURRENT_BINARY_DIR}/custom-generated/include"
  )
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_pipeline.toml"
    OUTPUT
      "custom-generated/include/feedforge/generated/consumer_custom.hpp"
  )
  get_target_property(
    _custom_include_dirs
    feedforge_generated_consumer_custom
    INTERFACE_INCLUDE_DIRECTORIES
  )
  if(NOT _custom_include_dirs STREQUAL _custom_include_root)
    message(
      FATAL_ERROR
      "installed custom OUTPUT exposed '${_custom_include_dirs}', expected "
      "'${_custom_include_root}'"
    )
  endif()
  add_executable(feedforge_consumer generated_main.cpp)
  target_link_libraries(
    feedforge_consumer
    PRIVATE FeedForge::generated::consumer_custom
  )
elseif(FEEDFORGE_CONSUMER_KIND STREQUAL "generated_legacy")
  set(
    _legacy_include_root
    "${CMAKE_CURRENT_BINARY_DIR}/legacy-generated"
  )
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_pipeline.toml"
    OUTPUT "${_legacy_include_root}/consumer_custom.hpp"
  )
  get_target_property(
    _legacy_include_dirs
    feedforge_generated_consumer_custom
    INTERFACE_INCLUDE_DIRECTORIES
  )
  if(NOT _legacy_include_dirs STREQUAL _legacy_include_root)
    message(
      FATAL_ERROR
      "legacy custom OUTPUT exposed '${_legacy_include_dirs}', expected "
      "'${_legacy_include_root}'"
    )
  endif()
  add_executable(feedforge_consumer generated_main.cpp)
  target_compile_definitions(
    feedforge_consumer
    PRIVATE FEEDFORGE_TEST_LEGACY_GENERATED_INCLUDE=1
  )
  target_link_libraries(
    feedforge_consumer
    PRIVATE FeedForge::generated::consumer_custom
  )
elseif(FEEDFORGE_CONSUMER_KIND STREQUAL "invalid_output")
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_pipeline.toml"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/custom-generated/wrong.hpp"
  )
  message(FATAL_ERROR "invalid generated output was unexpectedly accepted")
elseif(FEEDFORGE_CONSUMER_KIND STREQUAL "unsafe_output")
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_pipeline.toml"
    OUTPUT
      "${CMAKE_CURRENT_SOURCE_DIR}/feedforge/generated/consumer_custom.hpp"
  )
  message(FATAL_ERROR "unsafe generated output was unexpectedly accepted")
else()
  message(
    FATAL_ERROR
    "unknown FEEDFORGE_CONSUMER_KIND: ${FEEDFORGE_CONSUMER_KIND}"
  )
endif()

set_target_properties(
  feedforge_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

add_custom_target(
  run-feedforge-consumer
  COMMAND "$<TARGET_FILE:feedforge_consumer>"
  DEPENDS feedforge_consumer
  VERBATIM
)
