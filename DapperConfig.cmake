# Copyright (c) 2024 Flokart World, Inc.
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
#    1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
#
#    2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
#
#    3. This notice may not be removed or altered from any source distribution.

cmake_minimum_required (VERSION 3.18)

function (DAPPER_USE)
  cmake_parse_arguments (
    PARSE_ARGV 0 -arg "" "NAME;VERSION;SOURCE_DIR;REVISION" ""
  )

  set (-name "${-arg_NAME}")
  set ("dapperUse_${-name}_Version" "${-arg_VERSION}" PARENT_SCOPE)
  set ("dapperUse_${-name}_SourceDir" "${-arg_SOURCE_DIR}" PARENT_SCOPE)
  set ("dapperUse_${-name}_Revision" "${-arg_REVISION}" PARENT_SCOPE)

  set (-names "${dapperUsedNames}")
  list (APPEND -names "${-name}")
  set (dapperUsedNames "${-names}" PARENT_SCOPE)
endfunction ()

function (_DAPPER_RESOLVE_DEPENDENCIES -outDepsFile -integration)
  cmake_language (
    CALL
      "_DAPPER_REPOSITORIES_DIR_FOR_${-integration}"
      -reposDir
  )

  set (-depsFile "${CMAKE_CURRENT_BINARY_DIR}/ResolvedDependencies.cmake")

  set (-configFile "${DAPPER_CONFIG_FILE}")
  if (NOT -configFile)
    set (-configFile "$ENV{DAPPER_CONFIG_FILE}")
  endif ()

  if (NOT EXISTS "${-depsFile}" OR DAPPER_INSTALL)
    set (DAPPER_ROOT_DIR "${Dapper_DIR}")
    set (DAPPER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set (DAPPER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    set (DAPPER_PROJECT_NAME "${PROJECT_NAME}")
    set (DAPPER_PROJECT_VERSION "${PROJECT_VERSION}")
    set (DAPPER_REPOSITORIES_DIR "${-reposDir}")
    set (DAPPER_CONFIG_FILE "${-configFile}")
    include ("${Dapper_DIR}/Scripts/ResolveDependencies.cmake")
  endif ()

  set (
    DAPPER_INSTALL OFF
    CACHE BOOL "Set ON to invoke the dependency resolution." FORCE
  )

  set ("${-outDepsFile}" "${-depsFile}" PARENT_SCOPE)
endfunction ()

macro (DAPPER_INTEGRATE_WITH -integration)
  if (NOT "${-integration}" IN_LIST dapperAvailableIntegrations)
    message (FATAL_ERROR "No such integration: ${-integration}")
  endif ()

  if (DAPPER_TOP_LEVEL_DIR)
    return ()
  endif ()

  set (DAPPER_TOP_LEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  _DAPPER_RESOLVE_DEPENDENCIES(dapperDepsFile "${-integration}")

  set_property (
    GLOBAL APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${dapperDepsFile}"
  )

  # This script should call DAPPER_USE for each dependency.
  include ("${dapperDepsFile}")

  cmake_language (
    CALL
      "_DAPPER_FINALIZE_DEPENDENCIES_FOR_${-integration}"
      "${dapperUsedNames}"
  )
endmacro ()

find_package (Git REQUIRED)

foreach (-component IN LISTS Dapper_FIND_COMPONENTS)
  set (-listFile "${CMAKE_CURRENT_LIST_DIR}/Components/${-component}.cmake")
  if (EXISTS "${-listFile}")
    include ("${-listFile}")
  elseif (Dapper_FIND_REQUIRED_${-component})
    set (Dapper_FOUND false)
    set (Dapper_NOT_FOUND_MESSAGE "Component ${-component} not found")
    break ()
  endif ()
endforeach ()
