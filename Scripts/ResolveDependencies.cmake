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

function (_DAPPER_DECLARATION_PREFIX -outPrefix -id)
  set ("${-outPrefix}" "Dapper::Declarations::${-id}::" PARENT_SCOPE)
endfunction ()

function (_DAPPER_NEW_DECLARATION -id)
  set_property (GLOBAL APPEND PROPERTY "Dapper::Declarations" "${-id}")
endfunction ()

function (_DAPPER_DAP_PREFIX -outPrefix -dapId)
  set ("${-outPrefix}" "Dapper::DAPs::${-dapId}::" PARENT_SCOPE)
endfunction ()

function (_DAPPER_NEW_DAP -dapId)
  _DAPPER_DAP_PREFIX(-prefix "${-dapId}")
  set_property (GLOBAL PROPERTY "${-prefix}Declarations" "")
  set_property (GLOBAL APPEND PROPERTY "Dapper::DAPs" "${-dapId}")
endfunction ()

function (_DAPPER_NAME_PREFIX -outPrefix -name)
  set ("${-outPrefix}" "Dapper::Names::${-name}::" PARENT_SCOPE)
endfunction ()

function (_DAPPER_NEW_NAME -name)
  _DAPPER_NAME_PREFIX(-prefix "${-name}")
  set_property (GLOBAL PROPERTY "${-prefix}KnownPackages" "")
  set_property (GLOBAL APPEND PROPERTY "Dapper::Names" "${-name}")
endfunction ()

function (_DAPPER_ARRAY_TO_JSON -ret)
  string (JOIN "," -joinedItems ${ARGN})
  set (${-ret} "[${-joinedItems}]" PARENT_SCOPE)
endfunction ()

function (_DAPPER_LIST_TO_JSON -ret)
  set (-quotedItems)
  foreach (-arg IN LISTS ARGN)
    string (CONFIGURE [["@-arg@"]] -jsonString @ONLY ESCAPE_QUOTES)
    list (APPEND -quotedItems "${-jsonString}")
  endforeach ()
  string (JOIN "," -joinedItems ${-quotedItems})
  set (${-ret} "[${-joinedItems}]" PARENT_SCOPE)
endfunction ()

function (_DAPPER_KV_PAIRS_TO_JSON -ret)
  set (-pairs)
  while (true)
    list (LENGTH ARGN -len)
    if (-len LESS 2)
      break ()
    endif ()
    list (POP_FRONT ARGN -key)
    list (POP_FRONT ARGN -jsonValue)
    string (CONFIGURE [["@-key@"]] -jsonKey @ONLY ESCAPE_QUOTES)
    string (CONFIGURE [[@-jsonKey@:@-jsonValue@]] -pair @ONLY)
    list (APPEND -pairs "${-pair}")
  endwhile ()
  string (JOIN "," -joinedPairs ${-pairs})
  set (${-ret} "{${-joinedPairs}}" PARENT_SCOPE)
endfunction ()

function (_DAPPER_VISIT_DAP_INFO)
  cmake_parse_arguments (PARSE_ARGV 0 -arg "" "NAME;VERSION" "")
  _DAPPER_DAP_PREFIX(-prefix "${dapperCurrentPackageId}")
  set_property (GLOBAL PROPERTY "${-prefix}Name" "${-arg_NAME}")
  set_property (GLOBAL PROPERTY "${-prefix}Version" "${-arg_VERSION}")
endfunction ()

function (_DAPPER_VISIT_DAP)
  set (-from "${CMAKE_CURRENT_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE}")
  cmake_parse_arguments (PARSE_ARGV 0 -arg "" "NAME;REQUIRE" "LOCATION")

  if (NOT DEFINED -arg_NAME AND NOT DEFINED -arg_LOCATION)
    message (FATAL_ERROR "Either NAME or LOCATION is needed.")
  endif ()
  if (NOT DEFINED -arg_REQUIRE AND NOT DEFINED -arg_LOCATION)
    message (FATAL_ERROR "Either REQUIRE or LOCATION is needed.")
  endif ()

  _DAPPER_DAP_PREFIX(-prefix "${dapperCurrentPackageId}")
  get_property (-declarations GLOBAL PROPERTY "${-prefix}Declarations")
  list (LENGTH -declarations -len)

  set (-id "${dapperCurrentPackageId}_${-len}")
  _DAPPER_NEW_DECLARATION("${-id}")
  _DAPPER_DECLARATION_PREFIX(-decl "${-id}")
  set_property (GLOBAL PROPERTY "${-decl}From" "${-from}")
  set_property (GLOBAL PROPERTY "${-decl}Name" "${-arg_NAME}")
  set_property (GLOBAL PROPERTY "${-decl}RequiredVersion" "${-arg_REQUIRE}")
  set_property (GLOBAL PROPERTY "${-decl}KnownLocations" "${-arg_LOCATION}")

  set_property (GLOBAL APPEND PROPERTY "${-prefix}Declarations" "${-id}")
endfunction ()

function (DAP_INFO)
  cmake_language (CALL "${dapperCurrentHandler}_DAP_INFO" ${ARGN})
endfunction ()

function (DAP)
  cmake_language (CALL "${dapperCurrentHandler}_DAP" ${ARGN})
endfunction ()

function (_DAPPER_LOAD_DA -dapId -dapDir)
  set (-daFile "${-dapDir}/DependencyAwareness.cmake")
  set (dapperDeclarations)
  if (EXISTS "${-daFile}")
    set (dapperCurrentPackageId "${-dapId}")
    set (dapperCurrentHandler _DAPPER_VISIT)
    include ("${-daFile}")
  endif ()
endfunction ()

function (_DAPPER_PEEK_DA -dapId -dapDir -tag)
  execute_process (
    COMMAND
      "${GIT_EXECUTABLE}" -C "${-sourceDir}"
      show "${-tag}:DependencyAwareness.cmake"
    RESULT_VARIABLE
      -code
    OUTPUT_VARIABLE
      -daBody
  )
  set (dapperDeclarations)
  if (-code EQUAL 0)
    set (dapperCurrentPackageId "${-dapId}")
    set (dapperCurrentHandler _DAPPER_VISIT)
    cmake_language (EVAL CODE "${-daBody}")
  endif ()
endfunction ()

function (DAPPI_SELECT -name -dapId)
  _DAPPER_NAME_PREFIX(-namePrefix "${-name}")
  get_property (-selected GLOBAL PROPERTY "${-namePrefix}SelectedPackage")
  if (NOT -selected STREQUAL -dapId)
    set_property (GLOBAL PROPERTY "${-namePrefix}SelectedPackage" "${-dapId}")
    message (DEBUG "Selecting ${-dapId} as ${-name} (was ${-selected})")
    set (dappiFinished false PARENT_SCOPE)
  endif ()
endfunction ()

function (_DAPPER_CLONE_OR_PULL -url -sourceDir)
  if (EXISTS "${-sourceDir}")
    message (STATUS "Synchronizing ${-sourceDir} with ${-url}")
    execute_process (
      COMMAND "${GIT_EXECUTABLE}" -C "${-sourceDir}" pull --tags
      RESULT_VARIABLE -code
      ERROR_QUIET
      OUTPUT_QUIET
    )
    if (NOT -code EQUAL 0)
      message (FATAL "git pull failed.")
    endif ()
  else ()
    message (STATUS "Cloning ${-url} into ${-sourceDir}")
    execute_process (
      COMMAND "${GIT_EXECUTABLE}" clone "${-url}" "${-sourceDir}"
      RESULT_VARIABLE -code
      ERROR_QUIET
      OUTPUT_QUIET
    )
    if (NOT -code EQUAL 0)
      message (FATAL "git clone failed.")
    endif ()
  endif ()
endfunction ()

function (_DAPPER_FETCH -outHashes)
  cmake_parse_arguments (PARSE_ARGV 1 -arg "" "NAME;GIT_REPOSITORY;GIT_TAG" "")
  string (SHA256 -urlHash "${-arg_GIT_REPOSITORY}")
  set (-prefix "Dapper::Repositories::${-urlHash}")

  get_property (-repositories GLOBAL PROPERTY "Dapper::Repositories")
  if (-urlHash IN_LIST -repositories)
    get_property (-sourceDir GLOBAL PROPERTY "${-prefix}SourceDir")
    get_property (-exposed GLOBAL PROPERTY "${-prefix}ExposedRevisions")
    if (NOT "${-arg_GIT_TAG}" IN_LIST -exposed)
      list (APPEND -exposed "${-arg_GIT_TAG}")
    endif ()
  else ()
    set (-sourceDir "${DAPPER_REPOSITORIES_DIR}/${-urlHash}")
    _DAPPER_CLONE_OR_PULL("${-arg_GIT_REPOSITORY}" "${-sourceDir}")
    execute_process (
      COMMAND
        "${GIT_EXECUTABLE}" -C "${-sourceDir}" tag
      OUTPUT_VARIABLE
        -tags
    )
    string (REPLACE "\n" ";" -tags "${-tags}")
    list (REMOVE_ITEM -tags "")

    set (-exposed)
    foreach (-tag IN LISTS -tags)
      # Ignoring pre-releases right now
      if (-tag MATCHES "^v[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
        list (APPEND -exposed "${-tag}")
      endif ()
    endforeach ()
    list (APPEND -exposed "${-arg_GIT_TAG}")
    list (REMOVE_DUPLICATES -exposed)

    set_property (GLOBAL PROPERTY "${-prefix}URL" "${-arg_GIT_REPOSITORY}")
    set_property (GLOBAL PROPERTY "${-prefix}SourceDir" "${-sourceDir}")
    set_property (GLOBAL PROPERTY "${-prefix}ExposedRevisions" "${-exposed}")
    set_property (GLOBAL APPEND PROPERTY "Dapper::Repositories" "${-urlHash}")
  endif ()

  set (-hashes)
  foreach (-revision IN LISTS -exposed)
    string (SHA256 -hash "${-arg_GIT_REPOSITORY}#${-revision}")
    if (NOT TARGET "Dapper::DAPs::${-hash}")
      _DAPPER_NEW_DAP("${-hash}")
      _DAPPER_PEEK_DA("${-hash}" "${-sourceDir}" "${-revision}")

      _DAPPER_DAP_PREFIX(-dapPrefix "${-hash}")
      set_property (GLOBAL PROPERTY "${-dapPrefix}URL" "${-arg_GIT_REPOSITORY}")
      set_property (GLOBAL PROPERTY "${-dapPrefix}Fragment" "${-revision}")
      set_property (GLOBAL PROPERTY "${-dapPrefix}SourceDir" "${-sourceDir}")
    endif ()
    list (APPEND -hashes "${-hash}")
  endforeach ()

  set (${-outHashes} "${-hashes}" PARENT_SCOPE)
endfunction ()

function (_DAPPER_ALL_RELEVANT_NAMES -outNames)
  set (-names)
  set (-daps ROOT)
  set (-queue "${-daps}")
  while (-queue)
    list (POP_FRONT -queue -dapId)

    set (-dependencies)
    _DAPPER_DAP_PREFIX(-dapPrefix "${-dapId}")
    get_property (-declarations GLOBAL PROPERTY "${-dapPrefix}Declarations")
    foreach (-decl IN LISTS -declarations)
      _DAPPER_DECLARATION_PREFIX(-declPrefix "${-decl}")
      get_property (-name GLOBAL PROPERTY "${-declPrefix}Name")
      if (NOT -name IN_LIST -names)
        list (APPEND -names "${-name}")
      endif ()
      _DAPPER_NAME_PREFIX(-namePrefix "${-name}")
      get_property (
        -selectedDap GLOBAL PROPERTY "${-namePrefix}SelectedPackage"
      )
      if (NOT -selectedDap IN_LIST -daps)
        list (APPEND -daps "${-selectedDap}")
        list (APPEND -queue "${-selectedDap}")
      endif ()
    endforeach ()
  endwhile ()
  set (${-outNames} "${-names}" PARENT_SCOPE)
endfunction ()

find_package (Git REQUIRED)

set (-dappiBinDir "${DAPPER_BINARY_DIR}/dappi")
find_program (
  DAPPI_EXECUTABLE
  dappi PATHS "${-dappiBinDir}/install/bin" NO_DEFAULT_PATHS
)

# TODO : Perform auto-upgrading
if (NOT DAPPI_EXECUTABLE)
  message (STATUS "Configuring dappi...")
  execute_process (
    COMMAND
      ${CMAKE_COMMAND}
      -S "${DAPPER_ROOT_DIR}/Tools/dappi"
      -B "${-dappiBinDir}/build"
      -D "CMAKE_BUILD_TYPE=Release"
      -D "CMAKE_CONFIGURATION_TYPES=Release"
      -D "CMAKE_INSTALL_PREFIX=${-dappiBinDir}/install"
      --no-warn-unused-cli
    RESULT_VARIABLE -code
    OUTPUT_QUIET
  )
  if (NOT -code EQUAL 0)
    message (FATAL_ERROR "Configuring dappi failed.")
  endif ()
  message (STATUS "Building dappi...")
  execute_process (
    COMMAND
      ${CMAKE_COMMAND} --build . --config Release
    WORKING_DIRECTORY "${-dappiBinDir}/build"
    RESULT_VARIABLE -code
    OUTPUT_QUIET
  )
  if (NOT -code EQUAL 0)
    message (FATAL_ERROR "Building dappi failed.")
  endif ()
  message (STATUS "Installing dappi...")
  execute_process (
    COMMAND
      ${CMAKE_COMMAND} --install . --config Release
    WORKING_DIRECTORY "${-dappiBinDir}/build"
    RESULT_VARIABLE -code
    OUTPUT_QUIET
  )
  if (NOT -code EQUAL 0)
    message (FATAL_ERROR "Installing dappi failed.")
  endif ()
  find_program (
    DAPPI_EXECUTABLE
    dappi PATHS "${-dappiBinDir}/install/bin" REQUIRED NO_DEFAULT_PATHS
  )
endif ()

message (STATUS "Resolving dependencies...")

_DAPPER_NEW_DAP(ROOT)
_DAPPER_DAP_PREFIX(-prefix ROOT)
set_property (GLOBAL PROPERTY "${-prefix}Name" "${DAPPER_PROJECT_NAME}")
set_property (GLOBAL PROPERTY "${-prefix}Version" "${DAPPER_PROJECT_VERSION}")
set_property (GLOBAL PROPERTY "${-prefix}SourceDir" "${DAPPER_SOURCE_DIR}")

_DAPPER_LOAD_DA(ROOT "${DAPPER_SOURCE_DIR}")

set (dappiFinished false)
set (-iteration 0)
set (-allDaps ROOT)
set (-allNames)
while (-iteration LESS 100)
  set (-processedDaps)
  set (-unprocessedDaps ROOT)
  set (-knownDapPairs)

  while (-unprocessedDaps)
    list (POP_FRONT -unprocessedDaps -dapId)
    list (APPEND -processedDaps "${-dapId}")

    set (-dependencies)
    _DAPPER_DAP_PREFIX(-prefix "${-dapId}")
    get_property (-declarations GLOBAL PROPERTY "${-prefix}Declarations")

    foreach (-decl IN LISTS -declarations)
      _DAPPER_DECLARATION_PREFIX(-declPrefix "${-decl}")
      get_property (-nameFromDecl GLOBAL PROPERTY "${-declPrefix}Name")
      get_property (
        -requiredVersion GLOBAL PROPERTY "${-declPrefix}RequiredVersion"
      )
      get_property (
        -knownLocations GLOBAL PROPERTY "${-declPrefix}KnownLocations"
      )

      if (NOT -nameFromDecl IN_LIST -allNames)
        _DAPPER_NEW_NAME("${-nameFromDecl}")
        list (APPEND -allNames "${-nameFromDecl}")
      endif ()

      set (-names "${-nameFromDecl}")
      foreach (-location IN LISTS -knownLocations)
        string (REGEX REPLACE "#(.*)$" ";\\1" -parsedLocation "${-location}")
        list (GET -parsedLocation 0 -url)
        list (GET -parsedLocation 1 -fragment)

        _DAPPER_FETCH(
          -hashes
          NAME "${-nameFromDecl}"
          GIT_REPOSITORY "${-url}"
          GIT_TAG "${-fragment}"
        )

        foreach (-hash IN LISTS -hashes)
          _DAPPER_DAP_PREFIX(-dapPrefix "${-hash}")
          get_property (-name GLOBAL PROPERTY "${-dapPrefix}Name")
          if (-name)
            # We don't append this DAP as candidate if name mismatches.
            if (NOT -nameFromDecl)
              list (APPEND -names "${-name}")
            endif ()

            if (NOT -name IN_LIST -allNames)
              _DAPPER_NEW_NAME("${-name}")
              list (APPEND -allNames "${-name}")
            endif ()
          else ()
            set (-name "${-nameFromDecl}")
          endif ()

          if (-name)
            _DAPPER_NAME_PREFIX(-namePrefix "${-name}")
            get_property (
              -knownPackages
              GLOBAL PROPERTY "${-namePrefix}KnownPackages"
            )
            if (NOT -hash IN_LIST -knownPackages)
              set_property (
                GLOBAL APPEND PROPERTY "${-namePrefix}KnownPackages" "${-hash}"
              )
            endif ()
          endif ()

          if (NOT -hash IN_LIST -allDaps)
            list (APPEND -allDaps "${-hash}")
          endif ()
        endforeach ()
      endforeach ()

      list (REMOVE_DUPLICATES -names)
      list (LENGTH -names -namesLen)
      get_property (-from GLOBAL PROPERTY "${-declPrefix}From")
      get_property (-locations GLOBAL PROPERTY "${-declPrefix}Locations")
      if (-namesLen EQUAL 0)
        message (
          FATAL_ERROR "DAP name not provided by ${-locations} at ${-from}"
        )
      elseif (-namesLen GREATER 1)
        # If a name is not provided by the declaration, all DAPs in the given
        # locations must be defined with the same name.
        message (
          FATAL_ERROR "DAP name mismatching: ${-names} at ${-from}"
        )
      endif ()

      if (NOT -nameFromDecl)
        set_property (GLOBAL PROPERTY "${-declPrefix}Name" "${-names}")
      endif ()

      _DAPPER_NAME_PREFIX(-namePrefix "${-names}")
      get_property (
        -selectedDapId GLOBAL PROPERTY "${-namePrefix}SelectedPackage"
      )
      if (-selectedDapId AND NOT -selectedDapId IN_LIST -processedDaps)
        list (APPEND -unprocessedDaps "${-selectedDapId}")
        list (REMOVE_DUPLICATES -unprocessedDaps)
      endif ()
    endforeach ()
  endwhile ()

  set (-namePairs)
  foreach (-name IN LISTS -allNames)
    _DAPPER_NAME_PREFIX(-namePrefix "${-name}")
    set (-members)

    get_property (
      -selectedDapId GLOBAL PROPERTY "${-namePrefix}SelectedPackage"
    )
    if (-selectedDapId)
      string (
        CONFIGURE [["@-selectedDapId@"]] -jsonDapId @ONLY ESCAPE_QUOTES
      )
      list (APPEND -members selected "${-jsonDapId}")
    endif ()

    get_property (-knownDapIds GLOBAL PROPERTY "${-namePrefix}KnownPackages")
    list (APPEND -relevantDaps ${-knownDapIds})
    _DAPPER_LIST_TO_JSON(-jsonKnown ${-knownDapIds})
    list (APPEND -members known "${-jsonKnown}")

    _DAPPER_KV_PAIRS_TO_JSON(-jsonName ${-members})
    list (APPEND -namePairs "${-name}" "${-jsonName}")
  endforeach ()
  _DAPPER_KV_PAIRS_TO_JSON(-jsonNames ${-namePairs})

  foreach (-dapId IN LISTS -allDaps)
    _DAPPER_DAP_PREFIX(-dapPrefix "${-dapId}")

    get_property (-version GLOBAL PROPERTY "${-dapPrefix}Version")
    string (CONFIGURE [["@-version@"]] -jsonVersion @ONLY ESCAPE_QUOTES)

    set (-dependencies)
    get_property (-declarations GLOBAL PROPERTY "${-dapPrefix}Declarations")
    foreach (-decl IN LISTS -declarations)
      _DAPPER_DECLARATION_PREFIX(-declPrefix "${-decl}")
      get_property (-name GLOBAL PROPERTY "${-declPrefix}Name")
      if (-name IN_LIST -allNames)
        get_property (
          -requiredVersion GLOBAL PROPERTY "${-declPrefix}RequiredVersion"
        )
        string (
          CONFIGURE
            [[{"name":"@-name@","requiredVersion":"@-requiredVersion@"}]]
            -jsonDependency @ONLY ESCAPE_QUOTES
        )
        list (APPEND -dependencies "${-jsonDependency}")
      endif ()
    endforeach ()
    _DAPPER_ARRAY_TO_JSON(-jsonDependencies ${-dependencies})

    string (
      CONFIGURE
        [[{"version":@-jsonVersion@,"dependencies":@-jsonDependencies@}]]
        -jsonDap @ONLY
    )
    list (APPEND -knownDapPairs "${-dapId}" "${-jsonDap}")
  endforeach ()
  _DAPPER_KV_PAIRS_TO_JSON(-jsonKnownDaps ${-knownDapPairs})

  string (
    CONFIGURE
      [[{"entry":"ROOT","names":@-jsonNames@,"daps":@-jsonKnownDaps@}]]
      -json @ONLY
  )

  set (dappiFinished true)
  set (-inputJsonFile "${DAPPER_BINARY_DIR}/dappi.json")
  file (WRITE "${-inputJsonFile}" "${-json}")
  execute_process (
    COMMAND "${DAPPI_EXECUTABLE}"
    RESULT_VARIABLE -code
    OUTPUT_VARIABLE -dappiInsts
    INPUT_FILE "${-inputJsonFile}"
  )
  if (NOT -code EQUAL 0)
    message (FATAL_ERROR "dappi failed.")
  endif ()
  cmake_language (EVAL CODE "${-dappiInsts}")

  if (dappiFinished)
    break ()
  endif ()
  math (EXPR -iteration "${-iteration} + 1")
endwhile ()

if (NOT dappiFinished)
  message (FATAL_ERROR "Number of iterations reached maximum count.")
endif ()

message (STATUS "Resolving dependencies: done.")

_DAPPER_ALL_RELEVANT_NAMES(-allNames)
list (SORT -allNames)
set (-useFileLines)
foreach (-name IN LISTS -allNames)
  _DAPPER_NAME_PREFIX(-namePrefix "${-name}")
  get_property (-dapId GLOBAL PROPERTY "${-namePrefix}SelectedPackage")
  _DAPPER_DAP_PREFIX(-dapPrefix "${-dapId}")
  get_property (-version GLOBAL PROPERTY "${-dapPrefix}Version")
  get_property (-sourceDir GLOBAL PROPERTY "${-dapPrefix}SourceDir")
  get_property (-url GLOBAL PROPERTY "${-dapPrefix}URL")
  get_property (-revision GLOBAL PROPERTY "${-dapPrefix}Fragment")
  list (
    APPEND
    -useFileLines
    "DAPPER_USE(\n"
    "  NAME \"${-name}\"\n"
    "  VERSION \"${-version}\"\n"
    "  SOURCE_DIR \"${-sourceDir}\"\n"
    "  REVISION \"${-revision}\"\n"
    ")\n"
  )
endforeach ()

string (JOIN "" -fileBody ${-useFileLines})
file (WRITE "${DAPPER_BINARY_DIR}/ResolvedDependencies.cmake" "${-fileBody}")
