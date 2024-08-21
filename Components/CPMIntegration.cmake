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

if ("CPM" IN_LIST dapperAvailableIntegrations)
  return ()
endif ()

if (NOT COMMAND CPMAddPackage)
  message (FATAL_ERROR "CPM must be included before loading this file.")
endif ()

function (_DAPPER_REPOSITORIES_DIR_FOR_CPM -outSourceDir)
  if (CPM_SOURCE_CACHE)
    set (-cacheDir "${CPM_SOURCE_CACHE}/cpm/plugins")
  elseif (DEFINED ENV{CPM_SOURCE_CACHE})
    set (-cacheDir "$ENV{CPM_SOURCE_CACHE}/cpm/plugins")
  else ()
    set (-cacheDir "${CMAKE_BINARY_DIR}")
  endif ()
  set (${-outSourceDir} "${-cacheDir}/dapper/repositories" PARENT_SCOPE)
endfunction ()

macro (_DAPPER_FINALIZE_DEPENDENCIES_FOR_CPM -allUsedNames)
  foreach (-name IN ITEMS ${-allUsedNames})
    CPMDeclarePackage(
      "${-name}"
      NAME "${-name}"
      VERSION "${dapperUse_${-name}_Version}"
      GIT_REPOSITORY "${dapperUse_${-name}_SourceDir}"
      GIT_TAG "${dapperUse_${-name}_Revision}"
      GIT_SHALLOW true
    )
  endforeach ()
endmacro ()

list (APPEND dapperAvailableIntegrations CPM)
