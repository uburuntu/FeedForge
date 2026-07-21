cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    FIXTURE_DIR
    COMPILER_FIXTURE_DIR
    SOURCE_CORPUS_DIR
    OUTPUT_DIR)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "generate_corpus.cmake requires ${_required}")
  endif()
endforeach()

function(byte_hex value output)
  math(EXPR _hex "${value}" OUTPUT_FORMAT HEXADECIMAL)
  string(REGEX REPLACE "^0x" "" _hex "${_hex}")
  string(LENGTH "${_hex}" _length)
  if(_length EQUAL 1)
    string(PREPEND _hex "0")
  endif()
  string(TOLOWER "${_hex}" _hex)
  set("${output}" "${_hex}" PARENT_SCOPE)
endfunction()

function(copy_hex_seeds source_directory destination_directory)
  file(GLOB _seeds LIST_DIRECTORIES false "${source_directory}/*.hex")
  list(SORT _seeds)
  foreach(_seed IN LISTS _seeds)
    get_filename_component(_name "${_seed}" NAME)
    file(
      COPY_FILE
      "${_seed}"
      "${destination_directory}/${_name}"
      ONLY_IF_DIFFERENT
    )
  endforeach()
endfunction()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
foreach(_kind IN ITEMS
    binary_file
    compiler_compile
    compiler_pipeline
    compiler_schema
    decode_one
    differential_decode
    replay)
  file(MAKE_DIRECTORY "${OUTPUT_DIR}/${_kind}")
endforeach()

copy_hex_seeds(
  "${SOURCE_CORPUS_DIR}/binary_file"
  "${OUTPUT_DIR}/binary_file"
)
copy_hex_seeds(
  "${SOURCE_CORPUS_DIR}/binary_file"
  "${OUTPUT_DIR}/replay"
)
copy_hex_seeds(
  "${SOURCE_CORPUS_DIR}/decode_one"
  "${OUTPUT_DIR}/decode_one"
)
copy_hex_seeds(
  "${SOURCE_CORPUS_DIR}/decode_one"
  "${OUTPUT_DIR}/differential_decode"
)
copy_hex_seeds(
  "${SOURCE_CORPUS_DIR}/replay"
  "${OUTPUT_DIR}/replay"
)

foreach(_name IN ITEMS
    invalid_schema_unknown_key.toml
    valid_schema.toml
    valid_schema_reordered.toml)
  file(
    COPY_FILE
    "${COMPILER_FIXTURE_DIR}/${_name}"
    "${OUTPUT_DIR}/compiler_schema/${_name}"
    ONLY_IF_DIFFERENT
  )
endforeach()
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/malformed.toml"
  "format_version = [\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/invalid-table-header.toml"
  "[[\nname = \"unreachable\"\n"
)
string(ASCII 239 187 191 _utf8_bom)
string(ASCII 255 _invalid_utf8)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/invalid-bom-table-header.toml"
  "${_utf8_bom}[!\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/invalid-utf8.toml"
  "name = \"${_invalid_utf8}\"\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/invalid-array-terminator.toml"
  "allowed = [}\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_schema/multiline-basic-transition.toml"
  "description = \"\"\"quoted\"\"\"\"\n[!\n"
)

foreach(_name IN ITEMS
    invalid_pipeline_wildcard.toml
    valid_pipeline.toml
    valid_pipeline_reordered.toml)
  file(
    COPY_FILE
    "${COMPILER_FIXTURE_DIR}/${_name}"
    "${OUTPUT_DIR}/compiler_pipeline/${_name}"
    ONLY_IF_DIFFERENT
  )
endforeach()
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/malformed.toml"
  "format_version = [\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/invalid-table-header.toml"
  "[[\nname = \"unreachable\"\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/invalid-bom-table-header.toml"
  "${_utf8_bom}[!\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/invalid-utf8.toml"
  "name = \"${_invalid_utf8}\"\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/invalid-array-terminator.toml"
  "fields = [}\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_pipeline/multiline-literal-transition.toml"
  "name = '''quoted''''\n[!\n"
)

file(READ "${COMPILER_FIXTURE_DIR}/valid_schema.toml" _compiler_schema)
file(READ "${COMPILER_FIXTURE_DIR}/valid_schema_reordered.toml" _compiler_schema_reordered)
file(READ "${COMPILER_FIXTURE_DIR}/valid_pipeline.toml" _compiler_pipeline)
file(READ "${COMPILER_FIXTURE_DIR}/valid_pipeline_reordered.toml" _compiler_pipeline_reordered)
file(READ "${COMPILER_FIXTURE_DIR}/invalid_pipeline_wildcard.toml" _compiler_pipeline_invalid)
set(_compiler_separator "\n# --- FEEDFORGE FUZZ PIPELINE ---\n")
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/valid.ffcase"
  "${_compiler_schema}${_compiler_separator}${_compiler_pipeline}"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/valid-reordered.ffcase"
  "${_compiler_schema_reordered}${_compiler_separator}${_compiler_pipeline_reordered}"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/invalid-pipeline.ffcase"
  "${_compiler_schema}${_compiler_separator}${_compiler_pipeline_invalid}"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/malformed-pipeline.ffcase"
  "${_compiler_schema}${_compiler_separator}format_version = [\n"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/invalid-schema-header.ffcase"
  "[[\n${_compiler_separator}${_compiler_pipeline}"
)
file(
  WRITE
  "${OUTPUT_DIR}/compiler_compile/multiline-schema-transition.ffcase"
  "description = \"\"\"quoted\"\"\"\"\n[!\n${_compiler_separator}${_compiler_pipeline}"
)

file(GLOB _fixtures LIST_DIRECTORIES false "${FIXTURE_DIR}/*.toml")
list(SORT _fixtures)
list(LENGTH _fixtures _fixture_count)
if(NOT _fixture_count EQUAL 23)
  message(FATAL_ERROR "Expected 23 reviewed ITCH fixtures, found ${_fixture_count}")
endif()

string(CONCAT _manifest
    "Compiler valid and paired seeds use tests/fixtures/compiler; "
    "synthetic malformed and transition seeds are defined in fuzz/generate_corpus.cmake.\n"
    "Runtime seeds are generated from reviewed raw_hex fields in tests/fixtures/itch50.\n")
set(_all_framed "hex:")
foreach(_fixture IN LISTS _fixtures)
  file(READ "${_fixture}" _contents)
  string(
    REGEX MATCH
    "raw_hex[ \t]*=[ \t]*\"([0-9A-Fa-f \t]+)\""
    _raw_hex_match
    "${_contents}"
  )
  set(_raw_hex "${CMAKE_MATCH_1}")
  if(_raw_hex STREQUAL "")
    message(FATAL_ERROR "Fixture has no auditable raw_hex field: ${_fixture}")
  endif()
  string(
    REGEX MATCH
    "raw_size[ \t]*=[ \t]*([0-9]+)"
    _raw_size_match
    "${_contents}"
  )
  set(_raw_size "${CMAKE_MATCH_1}")
  if(_raw_size STREQUAL "")
    message(FATAL_ERROR "Fixture has no raw_size field: ${_fixture}")
  endif()

  string(REGEX REPLACE "[ \t\r\n]" "" _compact_hex "${_raw_hex}")
  string(LENGTH "${_compact_hex}" _hex_length)
  math(EXPR _decoded_size "${_hex_length} / 2")
  math(EXPR _odd_hex_length "${_hex_length} % 2")
  if(NOT _odd_hex_length EQUAL 0 OR NOT _decoded_size EQUAL _raw_size)
    message(
      FATAL_ERROR
      "Fixture raw_hex/raw_size mismatch in ${_fixture}: "
      "${_decoded_size} decoded bytes versus ${_raw_size}"
    )
  endif()

  math(EXPR _prefix_high "${_raw_size} / 256")
  math(EXPR _prefix_low "${_raw_size} % 256")
  byte_hex("${_prefix_high}" _prefix_high_hex)
  byte_hex("${_prefix_low}" _prefix_low_hex)

  get_filename_component(_stem "${_fixture}" NAME_WE)
  get_filename_component(_fixture_name "${_fixture}" NAME)
  set(_decode_seed "reviewed-${_stem}.hex")
  set(_replay_seed "reviewed-${_stem}-complete.hex")
  file(
    WRITE
    "${OUTPUT_DIR}/decode_one/${_decode_seed}"
    "hex:${_raw_hex}\n"
  )
  file(
    WRITE
    "${OUTPUT_DIR}/differential_decode/${_decode_seed}"
    "hex:${_raw_hex}\n"
  )
  file(
    WRITE
    "${OUTPUT_DIR}/replay/${_replay_seed}"
    "hex:${_prefix_high_hex} ${_prefix_low_hex} ${_raw_hex} 00 00\n"
  )
  file(
    WRITE
    "${OUTPUT_DIR}/binary_file/${_replay_seed}"
    "hex:${_prefix_high_hex} ${_prefix_low_hex} ${_raw_hex} 00 00\n"
  )
  string(
    APPEND
    _all_framed
    " ${_prefix_high_hex} ${_prefix_low_hex} ${_raw_hex}"
  )
  string(
    APPEND
    _manifest
    "${_decode_seed} and ${_replay_seed} <- ${_fixture_name}\n"
  )
endforeach()

string(APPEND _all_framed " 00 00\n")
file(
  WRITE
  "${OUTPUT_DIR}/binary_file/all-reviewed-fixtures-complete.hex"
  "${_all_framed}"
)
file(
  WRITE
  "${OUTPUT_DIR}/replay/all-reviewed-fixtures-complete.hex"
  "${_all_framed}"
)
file(WRITE "${OUTPUT_DIR}/MANIFEST.txt" "${_manifest}")
