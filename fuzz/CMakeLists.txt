if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$")
  message(FATAL_ERROR "FeedForge libFuzzer targets require Clang")
endif()

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_QUIET TRUE)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=fuzzer,address,undefined")
check_cxx_source_compiles(
  [=[
    #include <cstddef>
    #include <cstdint>
    extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) {
      return 0;
    }
  ]=]
  FEEDFORGE_HAS_LIBFUZZER_RUNTIME
)
if(NOT FEEDFORGE_HAS_LIBFUZZER_RUNTIME)
  message(
    FATAL_ERROR
    "The selected Clang does not provide a usable libFuzzer/ASan/UBSan runtime. "
    "Use upstream Clang on Linux; normal test builds still provide standalone "
    "arbitrary-input smoke targets."
  )
endif()

file(
  GLOB _feedforge_reviewed_fixtures
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/tests/fixtures/itch50/*.toml"
)
file(
  GLOB_RECURSE _feedforge_source_corpus
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/corpus/*.hex"
)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_corpus.cmake"
    ${_feedforge_reviewed_fixtures}
    ${_feedforge_source_corpus}
)

set(_feedforge_fuzz_corpus_dir "${CMAKE_CURRENT_BINARY_DIR}/corpus")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DFIXTURE_DIR=${PROJECT_SOURCE_DIR}/tests/fixtures/itch50"
    "-DSOURCE_CORPUS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/corpus"
    "-DOUTPUT_DIR=${_feedforge_fuzz_corpus_dir}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_corpus.cmake"
  RESULT_VARIABLE _feedforge_corpus_result
  OUTPUT_VARIABLE _feedforge_corpus_stdout
  ERROR_VARIABLE _feedforge_corpus_stderr
)
if(NOT _feedforge_corpus_result EQUAL 0)
  message(
    FATAL_ERROR
    "Generating deterministic fuzz corpora failed "
    "(${_feedforge_corpus_result})\n"
    "${_feedforge_corpus_stdout}${_feedforge_corpus_stderr}"
  )
endif()
message(
  STATUS
  "FeedForge fuzz corpora generated at ${_feedforge_fuzz_corpus_dir}"
)

function(feedforge_add_fuzzer target source)
  add_executable("${target}" "${source}")
  target_compile_features("${target}" PRIVATE cxx_std_20)
  target_compile_options(
    "${target}"
    PRIVATE
      -fsanitize=fuzzer,address,undefined
      -fno-omit-frame-pointer
  )
  target_link_options(
    "${target}"
    PRIVATE -fsanitize=fuzzer,address,undefined
  )
  feedforge_configure_project_target("${target}")
endfunction()

feedforge_add_fuzzer(fuzz_binary_file fuzz_binary_file.cpp)
target_link_libraries(fuzz_binary_file PRIVATE FeedForge::runtime)

feedforge_add_fuzzer(fuzz_decode_one fuzz_decode_one.cpp)
target_link_libraries(
  fuzz_decode_one
  PRIVATE FeedForge::generated::itch50_all
)

feedforge_add_fuzzer(fuzz_replay fuzz_replay.cpp)
target_link_libraries(
  fuzz_replay
  PRIVATE FeedForge::generated::itch50_all
)
