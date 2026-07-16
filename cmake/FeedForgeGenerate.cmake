include_guard(GLOBAL)

function(feedforge_generate)
  set(_options)
  set(_one_value_args NAME SCHEMA PIPELINE OUTPUT)
  cmake_parse_arguments(PARSE_ARGV 0 FF "${_options}" "${_one_value_args}" "")

  if(FF_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "feedforge_generate: unknown arguments: ${FF_UNPARSED_ARGUMENTS}")
  endif()
  if(FF_KEYWORDS_MISSING_VALUES)
    message(
      FATAL_ERROR
      "feedforge_generate: missing values for: ${FF_KEYWORDS_MISSING_VALUES}"
    )
  endif()
  if(NOT DEFINED FF_NAME OR "${FF_NAME}" STREQUAL "")
    message(FATAL_ERROR "feedforge_generate: NAME is required")
  endif()
  if(NOT FF_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
    message(FATAL_ERROR "feedforge_generate: NAME must be a C++ identifier")
  endif()
  if(NOT DEFINED FF_SCHEMA OR "${FF_SCHEMA}" STREQUAL "")
    message(FATAL_ERROR "feedforge_generate: SCHEMA is required")
  endif()
  if(NOT DEFINED FF_PIPELINE OR "${FF_PIPELINE}" STREQUAL "")
    message(FATAL_ERROR "feedforge_generate: PIPELINE is required")
  endif()
  if(NOT TARGET FeedForge::compiler)
    message(
      FATAL_ERROR
      "feedforge_generate requires FeedForge::compiler. Install or enable the "
      "FeedForge host compiler, or use an installed canonical generated target."
    )
  endif()

  set(_public_target "FeedForge::generated::${FF_NAME}")
  if(TARGET "${_public_target}")
    message(FATAL_ERROR "feedforge_generate: target ${_public_target} already exists")
  endif()

  if(FF_SCHEMA STREQUAL "nasdaq_totalview_itch_5_0")
    if(DEFINED FeedForge_SCHEMA_DIR)
      set(_schema_dir "${FeedForge_SCHEMA_DIR}")
    else()
      get_property(
        _schema_dir GLOBAL PROPERTY FEEDFORGE_SCHEMA_DIR
      )
    endif()
    if(NOT _schema_dir)
      message(FATAL_ERROR "feedforge_generate: FeedForge_SCHEMA_DIR is not available")
    endif()
    set(_schema "${_schema_dir}/nasdaq/totalview_itch_5_0.toml")
  elseif(IS_ABSOLUTE "${FF_SCHEMA}")
    set(_schema "${FF_SCHEMA}")
  else()
    get_filename_component(
      _schema "${FF_SCHEMA}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )
  endif()

  if(IS_ABSOLUTE "${FF_PIPELINE}")
    set(_pipeline "${FF_PIPELINE}")
  else()
    get_filename_component(
      _pipeline "${FF_PIPELINE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )
  endif()

  if(NOT EXISTS "${_schema}")
    message(FATAL_ERROR "feedforge_generate: schema does not exist: ${_schema}")
  endif()
  if(NOT EXISTS "${_pipeline}")
    message(FATAL_ERROR "feedforge_generate: pipeline does not exist: ${_pipeline}")
  endif()

  set(
    _include_root
    "${CMAKE_CURRENT_BINARY_DIR}/feedforge-generated/${FF_NAME}/include"
  )
  if(DEFINED FF_OUTPUT)
    if(FF_OUTPUT MATCHES "\\$<" OR FF_OUTPUT MATCHES ";")
      message(
        FATAL_ERROR
        "feedforge_generate: OUTPUT must be one literal path without generator "
        "expressions or list separators: ${FF_OUTPUT}"
      )
    endif()
    if(IS_ABSOLUTE "${FF_OUTPUT}")
      set(_output "${FF_OUTPUT}")
    else()
      get_filename_component(
        _output "${FF_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
      )
    endif()
  else()
    set(_output "${_include_root}/feedforge/generated/${FF_NAME}.hpp")
  endif()
  cmake_path(NORMAL_PATH _output)

  if(DEFINED FF_OUTPUT)
    cmake_path(GET _output FILENAME _output_filename)
    if(NOT _output_filename STREQUAL "${FF_NAME}.hpp")
      message(
        FATAL_ERROR
        "feedforge_generate: OUTPUT filename must be ${FF_NAME}.hpp so its "
        "public include is unambiguous; resolved path: ${_output}. Use a path "
        "ending in feedforge/generated/${FF_NAME}.hpp for "
        "#include <feedforge/generated/${FF_NAME}.hpp>, or a flat "
        "${FF_NAME}.hpp path for the legacy #include <${FF_NAME}.hpp> form."
      )
    endif()

    cmake_path(GET _output PARENT_PATH _output_directory)
    cmake_path(GET _output_directory FILENAME _output_parent_name)
    if(_output_parent_name STREQUAL "generated")
      cmake_path(GET _output_directory PARENT_PATH _possible_feedforge_dir)
      cmake_path(
        GET _possible_feedforge_dir FILENAME _possible_feedforge_name
      )
    endif()
    if(_output_parent_name STREQUAL "generated" AND
       _possible_feedforge_name STREQUAL "feedforge")
      cmake_path(GET _possible_feedforge_dir PARENT_PATH _include_root)
    else()
      set(_include_root "${_output_directory}")
    endif()
  endif()

  set(_binary_root "${CMAKE_BINARY_DIR}")
  cmake_path(NORMAL_PATH _binary_root)
  cmake_path(
    IS_PREFIX _binary_root "${_output}" NORMALIZE _output_is_in_build_tree
  )
  if(NOT _output_is_in_build_tree)
    message(
      FATAL_ERROR
      "feedforge_generate: OUTPUT must be inside the build tree; resolved path: "
      "${_output}"
    )
  endif()
  if(IS_DIRECTORY "${_output}")
    message(
      FATAL_ERROR
      "feedforge_generate: OUTPUT names a directory, not a header: ${_output}"
    )
  endif()
  foreach(_input IN ITEMS "${_schema}" "${_pipeline}")
    set(_normalized_input "${_input}")
    cmake_path(NORMAL_PATH _normalized_input)
    if("${_output}" STREQUAL "${_normalized_input}")
      message(
        FATAL_ERROR
        "feedforge_generate: OUTPUT must not overwrite an input file: ${_output}"
      )
    endif()
  endforeach()
  get_property(
    _feedforge_generated_outputs
    GLOBAL PROPERTY FEEDFORGE_GENERATED_OUTPUTS
  )
  if(_output IN_LIST _feedforge_generated_outputs)
    message(
      FATAL_ERROR
      "feedforge_generate: output already belongs to another generated target: "
      "${_output}"
    )
  endif()
  set_property(
    GLOBAL APPEND PROPERTY FEEDFORGE_GENERATED_OUTPUTS "${_output}"
  )
  cmake_path(GET _output PARENT_PATH _output_directory)

  set(_target "feedforge_generated_${FF_NAME}")
  set(_generate_target "${_target}_generate")
  set(
    _generator_dependencies
    "${_schema}"
    "${_pipeline}"
    "$<TARGET_FILE:FeedForge::compiler>"
  )
  if(DEFINED FeedForge_GENERATOR_INPUTS)
    list(APPEND _generator_dependencies ${FeedForge_GENERATOR_INPUTS})
  endif()

  add_custom_command(
    OUTPUT "${_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_directory}"
    COMMAND
      "$<TARGET_FILE:FeedForge::compiler>" compile
      --schema "${_schema}"
      --pipeline "${_pipeline}"
      --output "${_output}"
    DEPENDS ${_generator_dependencies}
    COMMENT "Generating FeedForge pipeline ${FF_NAME}"
    VERBATIM
  )
  add_custom_target("${_generate_target}" DEPENDS "${_output}")
  add_library("${_target}" INTERFACE)
  add_library("${_public_target}" ALIAS "${_target}")
  add_dependencies("${_target}" "${_generate_target}")
  target_compile_features("${_target}" INTERFACE cxx_std_20)
  target_include_directories("${_target}" INTERFACE "${_include_root}")
  target_link_libraries("${_target}" INTERFACE FeedForge::runtime)
  set_target_properties(
    "${_target}"
    PROPERTIES
      CXX_EXTENSIONS OFF
      FEEDFORGE_GENERATED_HEADER "${_output}"
  )
endfunction()

