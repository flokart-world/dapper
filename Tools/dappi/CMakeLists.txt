cmake_minimum_required (VERSION 3.14)
project (dappi VERSION 0.1.0)

include ("cmake/get_cpm.cmake")

CPMAddPackage(
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.11.3
)
CPMAddPackage(
  GITHUB_REPOSITORY Neargye/semver
  VERSION 0.3.1
)
CPMAddPackage(
  GITHUB_REPOSITORY flokart-world/minisat
  GIT_TAG a62df68a01f4ee41e1bdb34c83dcff079c6dc19b
  VERSION 2.2.0
)

add_executable (dappi src/main.cpp)
target_compile_features (dappi PRIVATE cxx_std_17)
target_link_libraries (dappi nlohmann_json semver minisat-lib-static)

install (TARGETS dappi RUNTIME DESTINATION bin)
