cmake_minimum_required (VERSION 3.18)
project (demo VERSION 0.1.0)

include ("cmake/get_cpm.cmake")
find_package (
  Dapper
  REQUIRED
  COMPONENTS CPMIntegration
  CONFIG PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../.." NO_DEFAULT_PATHS
)

set (DEMO_DIR "${CMAKE_CURRENT_BINARY_DIR}")
find_package (Git REQUIRED)

function (BUILD_REPOSITORY -name)
  set (-repoDir "${DEMO_DIR}/${-name}")
  if (EXISTS "${-repoDir}")
    return ()
  endif ()
  file (MAKE_DIRECTORY "${-repoDir}")
  execute_process (COMMAND "${GIT_EXECUTABLE}" -C "${-repoDir}" init)
  file (
    GLOB -patches RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${-name}"
      "${CMAKE_CURRENT_SOURCE_DIR}/${-name}/*.patch.in"
  )
  foreach (-patch IN LISTS -patches)
    string (REGEX REPLACE "\.in$" "" -fileName "${-patch}")
    string (REGEX REPLACE "\.patch$" "" -tag "${-fileName}")
    configure_file (
      "${-name}/${-patch}" "patches/${-name}/${-fileName}"
      @ONLY NEWLINE_STYLE LF
    )
    execute_process (
      COMMAND
        "${GIT_EXECUTABLE}" -C "${-repoDir}"
        am "${CMAKE_CURRENT_BINARY_DIR}/patches/${-name}/${-fileName}"
    )
    execute_process (COMMAND "${GIT_EXECUTABLE}" -C "${-repoDir}" tag "${-tag}")
  endforeach ()
endfunction ()

BUILD_REPOSITORY(sub-a)
BUILD_REPOSITORY(sub-b)
BUILD_REPOSITORY(sub-c)

configure_file (
  DependencyAwareness.yml.in
  "${CMAKE_CURRENT_SOURCE_DIR}/DependencyAwareness.yml"
  @ONLY NEWLINE_STYLE LF
)

set_property (GLOBAL PROPERTY USE_FOLDERS true)
DAPPER_INTEGRATE_WITH(CPM)

CPMGetPackage(sub-a)
CPMGetPackage(sub-b)
