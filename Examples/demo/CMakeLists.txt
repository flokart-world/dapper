cmake_minimum_required (VERSION 3.18)
project (demo VERSION 0.1.0)

include ("cmake/get_cpm.cmake")
find_package (
  Dapper
  REQUIRED
  COMPONENTS CPMIntegration
  CONFIG PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../.." NO_DEFAULT_PATHS
)

set_property (GLOBAL PROPERTY USE_FOLDERS true)
DAPPER_INTEGRATE_WITH(CPM OPTIONS "DEMO_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

CPMGetPackage(sub-a)
CPMGetPackage(sub-b)
