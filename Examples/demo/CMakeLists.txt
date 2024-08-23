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
      "${CMAKE_CURRENT_SOURCE_DIR}/${-name}/*.patch"
  )
  foreach (-patch IN LISTS -patches)
    string (REGEX REPLACE "\.patch$" "" -tag "${-patch}")
    execute_process (
      COMMAND
        "${GIT_EXECUTABLE}" -C "${-repoDir}"
        am "${CMAKE_CURRENT_SOURCE_DIR}/${-name}/${-patch}"
    )
    execute_process (COMMAND "${GIT_EXECUTABLE}" -C "${-repoDir}" tag "${-tag}")
  endforeach ()
endfunction ()

BUILD_REPOSITORY(sub-a)
BUILD_REPOSITORY(sub-b)
BUILD_REPOSITORY(sub-c)

set_property (GLOBAL PROPERTY USE_FOLDERS true)
set (DAPPER_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Hosts.cmake")
DAPPER_INTEGRATE_WITH(CPM OPTIONS "DEMO_DIR=${DEMO_DIR}")

CPMGetPackage(sub-a)
CPMGetPackage(sub-b)
