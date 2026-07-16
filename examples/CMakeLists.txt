file(
  GLOB _feedforge_example_sources
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

foreach(_source IN LISTS _feedforge_example_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  if(_name STREQUAL "replay_binary_file")
    set(_target feedforge-replay)
  else()
    set(_target "feedforge_example_${_name}")
  endif()
  add_executable("${_target}" "${_source}")
  target_compile_features("${_target}" PRIVATE cxx_std_20)
  if(_name STREQUAL "replay_binary_file")
    target_link_libraries(
      "${_target}"
      PRIVATE FeedForge::generated::itch50_order_events
    )
  else()
    target_link_libraries("${_target}" PRIVATE FeedForge::runtime)
  endif()
  feedforge_configure_project_target("${_target}")
endforeach()
