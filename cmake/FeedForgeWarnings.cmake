include_guard(GLOBAL)

include(CheckCXXSourceCompiles)

function(feedforge_detect_rtsan)
  set(
    FEEDFORGE_RTSAN_SUPPORTED
    OFF
    CACHE INTERNAL
    "Whether the selected Clang provides RealtimeSanitizer"
    FORCE
  )
  if(NOT FEEDFORGE_ENABLE_RTSAN)
    return()
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(
      STATUS
      "RealtimeSanitizer smoke disabled: the selected compiler is not "
      "upstream Clang"
    )
    return()
  endif()

  set(CMAKE_REQUIRED_QUIET TRUE)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=realtime")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=realtime")
  unset(FEEDFORGE_RTSAN_PROBE CACHE)
  check_cxx_source_compiles(
    [=[
      #if !defined(__has_cpp_attribute)
      #error "Clang attribute probing is unavailable"
      #elif !__has_cpp_attribute(clang::nonblocking)
      #error "clang::nonblocking is unavailable"
      #endif
      void hot_path() noexcept [[clang::nonblocking]] {}
      int main() {
        hot_path();
        return 0;
      }
    ]=]
    FEEDFORGE_RTSAN_PROBE
  )
  if(FEEDFORGE_RTSAN_PROBE)
    set(
      FEEDFORGE_RTSAN_SUPPORTED
      ON
      CACHE INTERNAL
      "Whether the selected Clang provides RealtimeSanitizer"
      FORCE
    )
    message(STATUS "RealtimeSanitizer smoke enabled")
  else()
    message(
      STATUS
      "RealtimeSanitizer smoke disabled: this Clang lacks a usable "
      "-fsanitize=realtime runtime"
    )
  endif()
endfunction()

function(feedforge_enable_warnings target)
  if(MSVC)
    target_compile_options(
      "${target}"
      PRIVATE /W4 /permissive- /Zc:__cplusplus
    )
    if(FEEDFORGE_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE /WX)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(
      "${target}"
      PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
    if(FEEDFORGE_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE -Werror)
    endif()
  endif()
endfunction()

function(feedforge_enable_sanitizers target)
  set(_sanitizers)
  if(FEEDFORGE_ENABLE_ASAN)
    list(APPEND _sanitizers address)
  endif()
  if(FEEDFORGE_ENABLE_UBSAN)
    list(APPEND _sanitizers undefined)
  endif()

  if(NOT _sanitizers)
    return()
  endif()
  if(MSVC)
    message(FATAL_ERROR "The requested FeedForge sanitizer combination is not supported by MSVC")
  endif()

  string(JOIN "," _sanitizer_list ${_sanitizers})
  target_compile_options(
    "${target}" PRIVATE "-fsanitize=${_sanitizer_list}" -fno-omit-frame-pointer
  )
  target_link_options("${target}" PRIVATE "-fsanitize=${_sanitizer_list}")
endfunction()

function(feedforge_enable_rtsan target)
  if(NOT FEEDFORGE_RTSAN_SUPPORTED)
    message(
      FATAL_ERROR
      "feedforge_enable_rtsan called without a successful RTSan probe"
    )
  endif()
  target_compile_options(
    "${target}"
    PRIVATE
      -fsanitize=realtime
      -fno-omit-frame-pointer
  )
  target_link_options("${target}" PRIVATE -fsanitize=realtime)
endfunction()

function(feedforge_configure_project_target target)
  set_target_properties(
    "${target}"
    PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED YES
  )
  feedforge_enable_warnings("${target}")
  feedforge_enable_sanitizers("${target}")
endfunction()
