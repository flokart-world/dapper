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

list (FIND dapperAvailableIntegrations CPM -foundAt)
if (-foundAt GREATER_EQUAL 0)
  return ()
endif ()

if (NOT COMMAND CPMAddPackage)
  message (FATAL_ERROR "CPM must be included before loading this file.")
endif ()

function (_DAPPER_FETCH_BY_CPM -outSourceDir -url)
  string (SHA256 -hash "${-url}")
  if (CPM_SOURCE_CACHE)
    set (-cacheDir "${CPM_SOURCE_CACHE}")
  elseif (DEFINED ENV{CPM_SOURCE_CACHE})
    set (-cacheDir "$ENV{CPM_SOURCE_CACHE}")
  else ()
    set (-cacheDir "${CMAKE_BINARY_DIR}")
  endif ()
  set (-sourceDir "${-cacheDir}/dap-repos/${-hash}")

  _DAPPER_CLONE_OR_PULL("${-url}" "${-sourceDir}")
  set (${-outSourceDir} "${-sourceDir}" PARENT_SCOPE)
endfunction ()

macro (_DAPPER_FINALIZE_DEPENDENCIES_FOR_CPM)
  _DAPPER_ALL_RELEVANT_NAMES(-allNames)
  foreach (-name IN LISTS -allNames)
    get_target_property (
      -dapId "Dapper::Names::${-name}" Dapper::Name::SelectedPackage
    )
    get_target_property (
      -version "Dapper::DAPs::${-dapId}" Dapper::DAP::Version
    )
    get_target_property (
      -sourceDir "Dapper::DAPs::${-dapId}" Dapper::DAP::SourceDir
    )
    get_target_property (
      -revision "Dapper::DAPs::${-dapId}" Dapper::DAP::Fragment
    )
    CPMDeclarePackage(
      "${-name}"
      NAME "${-name}"
      VERSION "${-version}"
      GIT_REPOSITORY "${-sourceDir}"
      GIT_TAG "${-revision}"
      GIT_SHALLOW true
    )
    # FIXME : Don't touch internals.
    set (CPM_DECLARATION_${-name} "${CPM_DECLARATION_${-name}}" PARENT_SCOPE)
  endforeach ()
endmacro ()

list (APPEND dapperAvailableIntegrations CPM)
