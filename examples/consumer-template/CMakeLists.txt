cmake_minimum_required(VERSION 3.25)

project(FeedForgeConsumerTemplate LANGUAGES CXX)

option(
  FEEDFORGE_TEMPLATE_CUSTOM_PIPELINE
  "Generate and consume the template custom pipeline"
  OFF
)

find_package(FeedForge CONFIG REQUIRED)

if(FEEDFORGE_TEMPLATE_CUSTOM_PIPELINE)
  if(NOT TARGET FeedForge::compiler)
    message(
      FATAL_ERROR
      "FEEDFORGE_TEMPLATE_CUSTOM_PIPELINE requires a compiler-enabled "
      "FeedForge package; reinstall FeedForge with "
      "-DFEEDFORGE_BUILD_COMPILER=ON"
    )
  endif()
  feedforge_generate(
    NAME consumer_custom
    SCHEMA nasdaq_totalview_itch_5_0
    PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/custom_pipeline.toml"
  )
  set(_consumer_source custom_main.cpp)
  set(_feedforge_target FeedForge::generated::consumer_custom)
else()
  set(_consumer_source main.cpp)
  set(_feedforge_target FeedForge::generated::itch50_order_events)
endif()

add_executable(feedforge_consumer "${_consumer_source}")
target_compile_features(feedforge_consumer PRIVATE cxx_std_20)
set_target_properties(
  feedforge_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(feedforge_consumer PRIVATE "${_feedforge_target}")

add_custom_target(
  run-feedforge-consumer
  COMMAND "$<TARGET_FILE:feedforge_consumer>"
  DEPENDS feedforge_consumer
  VERBATIM
)
