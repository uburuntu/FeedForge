cmake_minimum_required(VERSION 3.25)

foreach(
  required_variable
  IN ITEMS
    FEEDFORGE_PROJECT_ROOT
    FEEDFORGE_BUILD_ROOT
    FEEDFORGE_OUTPUT_PATH
)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

set(project_root "${FEEDFORGE_PROJECT_ROOT}")
cmake_path(ABSOLUTE_PATH project_root NORMALIZE OUTPUT_VARIABLE project_root)
if(NOT IS_DIRECTORY "${project_root}")
  message(FATAL_ERROR "Project root is not a directory: ${project_root}")
endif()
file(REAL_PATH "${project_root}" real_project_root)

set(expected_build_root "${project_root}/build")
cmake_path(NORMAL_PATH expected_build_root OUTPUT_VARIABLE expected_build_root)
set(build_root "${FEEDFORGE_BUILD_ROOT}")
cmake_path(
  ABSOLUTE_PATH
  build_root
  BASE_DIRECTORY "${project_root}"
  NORMALIZE
  OUTPUT_VARIABLE build_root
)
if(NOT build_root STREQUAL expected_build_root)
  message(FATAL_ERROR "Build root must be ${expected_build_root}")
endif()

set(output_path "${FEEDFORGE_OUTPUT_PATH}")
cmake_path(
  ABSOLUTE_PATH
  output_path
  BASE_DIRECTORY "${project_root}"
  NORMALIZE
  OUTPUT_VARIABLE output_path
)
cmake_path(IS_PREFIX build_root "${output_path}" NORMALIZE inside_build)
if(NOT inside_build OR output_path STREQUAL build_root)
  message(FATAL_ERROR "Output must be a file below ${build_root}: ${output_path}")
endif()

set(
  protected_paths
  "${build_root}/bench/private-holdout"
  "${build_root}/bench/results"
)
foreach(protected_path IN LISTS protected_paths)
  cmake_path(IS_PREFIX protected_path "${output_path}" NORMALIZE inside_protected)
  if(inside_protected)
    message(FATAL_ERROR "Output is inside protected benchmark data: ${output_path}")
  endif()
endforeach()

if(IS_SYMLINK "${build_root}")
  message(FATAL_ERROR "Build root must not be a symlink: ${build_root}")
endif()
if(EXISTS "${build_root}" AND NOT IS_DIRECTORY "${build_root}")
  message(FATAL_ERROR "Build root is not a directory: ${build_root}")
endif()
if(NOT EXISTS "${build_root}")
  file(MAKE_DIRECTORY "${build_root}")
endif()

file(REAL_PATH "${build_root}" real_build_root)
set(expected_real_build_root "${real_project_root}/build")
cmake_path(
  NORMAL_PATH
  expected_real_build_root
  OUTPUT_VARIABLE expected_real_build_root
)
if(NOT real_build_root STREQUAL expected_real_build_root)
  message(FATAL_ERROR "Build root resolves outside ${real_project_root}")
endif()

function(reject_protected_real_path candidate description)
  foreach(protected_path IN LISTS protected_paths)
    if(EXISTS "${protected_path}" OR IS_SYMLINK "${protected_path}")
      file(REAL_PATH "${protected_path}" real_protected_path)
      cmake_path(
        IS_PREFIX
        real_protected_path
        "${candidate}"
        NORMALIZE
        inside_real_protected
      )
      if(inside_real_protected)
        message(FATAL_ERROR "${description} resolves inside protected benchmark data")
      endif()
    endif()
  endforeach()
endfunction()

cmake_path(GET output_path PARENT_PATH output_directory)
set(existing_ancestor "${output_directory}")
while(NOT EXISTS "${existing_ancestor}" AND NOT IS_SYMLINK "${existing_ancestor}")
  set(parent "${existing_ancestor}")
  cmake_path(GET parent PARENT_PATH parent)
  if(parent STREQUAL existing_ancestor)
    message(FATAL_ERROR "Cannot locate an existing output ancestor")
  endif()
  set(existing_ancestor "${parent}")
endwhile()

if(NOT IS_DIRECTORY "${existing_ancestor}")
  message(FATAL_ERROR "Output ancestor is not a directory: ${existing_ancestor}")
endif()
file(REAL_PATH "${existing_ancestor}" real_existing_ancestor)
cmake_path(
  IS_PREFIX
  real_build_root
  "${real_existing_ancestor}"
  NORMALIZE
  real_ancestor_inside_build
)
if(NOT real_ancestor_inside_build)
  message(FATAL_ERROR "Output ancestor resolves outside ${real_build_root}")
endif()
reject_protected_real_path("${real_existing_ancestor}" "Output ancestor")

file(MAKE_DIRECTORY "${output_directory}")
file(REAL_PATH "${output_directory}" real_output_directory)
cmake_path(
  IS_PREFIX
  real_build_root
  "${real_output_directory}"
  NORMALIZE
  real_directory_inside_build
)
if(NOT real_directory_inside_build)
  message(FATAL_ERROR "Output directory resolves outside ${real_build_root}")
endif()
reject_protected_real_path("${real_output_directory}" "Output directory")

if(EXISTS "${output_path}" OR IS_SYMLINK "${output_path}")
  file(REAL_PATH "${output_path}" real_output_path)
  cmake_path(
    IS_PREFIX
    real_build_root
    "${real_output_path}"
    NORMALIZE
    real_output_inside_build
  )
  if(NOT real_output_inside_build)
    message(FATAL_ERROR "Existing output resolves outside ${real_build_root}")
  endif()
  reject_protected_real_path("${real_output_path}" "Existing output")
endif()
