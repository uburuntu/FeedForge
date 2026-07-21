include(CheckCXXSourceCompiles)
include(FetchContent)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_VERSION LESS 1938)
  message(
    FATAL_ERROR
    "feedforgec requires MSVC 19.38 or newer (Visual Studio 2022 17.8)"
  )
endif()

function(feedforge_check_host_features)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED YES)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_compiles(
    [=[
      #include <bit>
      #include <cstdint>
      #include <expected>
      #include <filesystem>
      #include <source_location>
      #include <utility>

      enum class value : unsigned { one = 1 };

      int main() {
        std::expected<int, int> result{1};
        const auto swapped = std::byteswap(std::uint32_t{1});
        const auto underlying = std::to_underlying(value::one);
        const auto location = std::source_location::current();
        const std::filesystem::path path{"."};
        return result.value() + static_cast<int>(swapped + underlying) +
               static_cast<int>(location.line()) + static_cast<int>(path.empty());
      }
    ]=]
    FEEDFORGE_HAS_REQUIRED_CXX23_LIBRARY
  )
  if(NOT FEEDFORGE_HAS_REQUIRED_CXX23_LIBRARY)
    message(
      FATAL_ERROR
      "feedforgec requires a C++23 compiler and standard library with "
      "std::expected, std::byteswap, std::to_underlying, std::filesystem, "
      "and std::source_location"
    )
  endif()
endfunction()

feedforge_check_host_features()

find_package(tomlplusplus 3.4.0 EXACT QUIET CONFIG)
if(NOT TARGET tomlplusplus::tomlplusplus)
  set(TOMLPLUSPLUS_BUILD_TESTS OFF)
  set(TOMLPLUSPLUS_BUILD_EXAMPLES OFF)
  set(TOMLPLUSPLUS_INSTALL OFF)
  FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG 30172438cee64926dc41fdd9c11fb3ba5b2ba9de
    SYSTEM
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(tomlplusplus)
endif()

add_library(
  feedforge_compiler_frontend
  STATIC
  diagnostics.cpp
  emit_cpp.cpp
  ir.cpp
  lower.cpp
  model.cpp
  parse_toml.cpp
  sha256.cpp
)
target_compile_features(feedforge_compiler_frontend PUBLIC cxx_std_23)
target_include_directories(
  feedforge_compiler_frontend
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
  feedforge_compiler_frontend
  PRIVATE
    TOML_EXCEPTIONS=0
    TOML_MAX_NESTED_VALUES=32
)
target_link_libraries(
  feedforge_compiler_frontend
  PRIVATE tomlplusplus::tomlplusplus
)
feedforge_configure_project_target(feedforge_compiler_frontend)

add_executable(feedforgec main.cpp)
add_executable(FeedForge::compiler ALIAS feedforgec)
set_target_properties(feedforgec PROPERTIES EXPORT_NAME compiler)
target_compile_features(feedforgec PRIVATE cxx_std_23)
target_link_libraries(
  feedforgec
  PRIVATE
    feedforge_compiler_frontend
    FeedForge::runtime
)
feedforge_configure_project_target(feedforgec)

install(
  TARGETS feedforgec
  EXPORT FeedForgeTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Compiler
)
