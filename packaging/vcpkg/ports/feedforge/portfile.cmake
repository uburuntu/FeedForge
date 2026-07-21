get_filename_component(
    SOURCE_PATH
    "${CURRENT_PORT_DIR}/../../../.."
    ABSOLUTE
)

set(VCPKG_BUILD_TYPE release)

if(NOT EXISTS "${SOURCE_PATH}/CMakeLists.txt" OR
   NOT EXISTS "${SOURCE_PATH}/include/feedforge/version.hpp")
    message(FATAL_ERROR "The FeedForge overlay must remain inside its source tree")
endif()

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        compiler FEEDFORGE_BUILD_COMPILER
)
if("compiler" IN_LIST FEATURES)
    list(APPEND FEATURE_OPTIONS -DFEEDFORGE_TOML_EXCEPTIONS=ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DFEEDFORGE_BUILD_TESTS=OFF
        -DFEEDFORGE_BUILD_EXAMPLES=OFF
        -DFEEDFORGE_BUILD_FUZZERS=OFF
        -DFEEDFORGE_BUILD_BENCHMARKS=OFF
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME FeedForge
    CONFIG_PATH lib/cmake/FeedForge
)

if("compiler" IN_LIST FEATURES)
    vcpkg_copy_tools(TOOL_NAMES feedforgec AUTO_CLEAN)
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug" "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
