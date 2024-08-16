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

function (_DAPPER_DEFINE_PROPERTY)
  define_property (
    ${ARGN}
    BRIEF_DOCS "Used internally by Dapper"
    FULL_DOCS "Used internally by Dapper"
  )
endfunction ()

function (_DAPPER_INIT)
  get_property (-initialized GLOBAL PROPERTY Dapper::Initialized DEFINED)
  if (-initialized)
    return ()
  endif ()

  _DAPPER_DEFINE_PROPERTY(GLOBAL PROPERTY Dapper::Initialized)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Declaration::From)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Declaration::Name)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Declaration::RequiredVersion)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Declaration::KnownLocations)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::DAP::Fragment)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::DAP::Name)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::DAP::Version)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::DAP::SourceDir)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::DAP::Declarations)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Repository::URL)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Repository::SourceDir)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Repository::ExposedRevisions)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Name::SelectedPackage)
  _DAPPER_DEFINE_PROPERTY(TARGET PROPERTY Dapper::Name::KnownPackages)
endfunction ()

function (_DAPPER_NEW_DECLARATION -id)
  add_library ("Dapper::Declarations::${-id}" INTERFACE IMPORTED)
endfunction ()

function (_DAPPER_NEW_DAP -dapId)
  add_library ("Dapper::DAPs::${-dapId}" INTERFACE IMPORTED)
  set_target_properties (
    "Dapper::DAPs::${-dapId}"
    PROPERTIES
      Dapper::DAP::Declarations ""
  )
endfunction ()

function (_DAPPER_NEW_NAME -name)
  add_library ("Dapper::Names::${-name}" INTERFACE IMPORTED)
  set_target_properties (
    "Dapper::Names::${-name}"
    PROPERTIES
      Dapper::Name::KnownPackages ""
  )
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

function (DAP_INFO)
  cmake_parse_arguments (PARSE_ARGV 0 -arg "" "NAME;VERSION" "")
  set_target_properties (
    "Dapper::DAPs::${dapperCurrentPackageId}"
    PROPERTIES
      Dapper::DAP::Name "${-arg_NAME}"
      Dapper::DAP::Version "${-arg_VERSION}"
  )
endfunction ()

function (DAP)
  cmake_parse_arguments (PARSE_ARGV 0 -arg "" "NAME;REQUIRE" "LOCATION")

  if (NOT DEFINED -arg_NAME AND NOT DEFINED -arg_LOCATION)
    message (FATAL_ERROR "Either NAME or LOCATION is needed.")
  endif ()
  if (NOT DEFINED -arg_REQUIRE AND NOT DEFINED -arg_LOCATION)
    message (FATAL_ERROR "Either REQUIRE or LOCATION is needed.")
  endif ()

  get_target_property (
    -declarations
    "Dapper::DAPs::${dapperCurrentPackageId}"
    Dapper::DAP::Declarations
  )
  list (LENGTH -declarations -len)

  _DAPPER_NEW_DECLARATION("${dapperCurrentPackageId}_${-len}")
  set_target_properties (
    "Dapper::Declarations::${dapperCurrentPackageId}_${-len}"
    PROPERTIES
      Dapper::Declaration::From
        "${CMAKE_CURRENT_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE}"
      Dapper::Declaration::Name
        "${-arg_NAME}"
      Dapper::Declaration::RequiredVersion
        "${-arg_REQUIRE}"
      Dapper::Declaration::KnownLocations
        "${-arg_LOCATION}"
  )

  list (APPEND -declarations "${dapperCurrentPackageId}_${-len}")
  set_target_properties (
    "Dapper::DAPs::${dapperCurrentPackageId}"
    PROPERTIES
      Dapper::DAP::Declarations "${-declarations}"
  )
endfunction ()

function (_DAPPER_LOAD_DA -dapId -dapDir)
  set (-daFile "${-dapDir}/DependencyAwareness.cmake")
  set (dapperDeclarations)
  if (EXISTS "${-daFile}")
    set (dapperCurrentPackageId "${-dapId}")
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
    cmake_language (EVAL CODE "${-daBody}")
  endif ()
endfunction ()

function (DAPPI_SELECT -name -dapId)
  get_target_property (
    -dapVersion "Dapper::DAPs::${-dapId}" Dapper::DAP::Version
  )

  get_target_property (
    -selected "Dapper::Names::${-name}" Dapper::Name::SelectedPackage
  )
  if (NOT -selected STREQUAL -dapId)
    set_target_properties (
      "Dapper::Names::${-name}"
      PROPERTIES
        Dapper::Name::SelectedPackage "${-dapId}"
    )
#   message (STATUS "Selecting ${-dapId} as ${-name} (was ${-selected})")
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

  if (TARGET "Dapper::Repositories::${-urlHash}")
    get_target_property (
      -sourceDir
      "Dapper::Repositories::${-urlHash}" Dapper::Repository::SourceDir
    )
    get_target_property (
      -exposedRevisions
      "Dapper::Repositories::${-urlHash}" Dapper::Repository::ExposedRevisions
    )
    if (NOT "${-arg_GIT_TAG}" IN_LIST -exposedRevisions)
      list (APPEND -exposedRevisions "${-arg_GIT_TAG}")
    endif ()
  else ()
    cmake_language (
      CALL
        "_DAPPER_FETCH_BY_${dapperCurrentIntegration}"
        -sourceDir "${-arg_GIT_REPOSITORY}"
    )
    execute_process (
      COMMAND
        "${GIT_EXECUTABLE}" -C "${-sourceDir}" tag
      OUTPUT_VARIABLE
        -tags
    )
    string (REPLACE "\n" ";" -tags "${-tags}")
    list (REMOVE_ITEM -tags "")

    set (-exposedRevisions)
    foreach (-tag IN LISTS -tags)
      # Ignoring pre-releases right now
      if (-tag MATCHES "^v[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
        list (APPEND -exposedRevisions "${-tag}")
      endif ()
    endforeach ()
    list (APPEND -exposedRevisions "${-arg_GIT_TAG}")
    list (REMOVE_DUPLICATES -exposedRevisions)

    add_library ("Dapper::Repositories::${-urlHash}" INTERFACE IMPORTED)
    set_target_properties (
      "Dapper::Repositories::${-urlHash}"
      PROPERTIES
        Dapper::Repository::URL "${-arg_GIT_REPOSITORY}"
        Dapper::Repository::SourceDir "${-sourceDir}"
        Dapper::Repository::ExposedRevisions "${-exposedRevisions}"
    )
  endif ()

  set (-hashes)
  foreach (-revision IN LISTS -exposedRevisions)
    string (SHA256 -hash "${-arg_GIT_REPOSITORY}#${-revision}")
    if (NOT TARGET "Dapper::DAPs::${-hash}")
      _DAPPER_NEW_DAP("${-hash}")
      _DAPPER_PEEK_DA("${-hash}" "${-sourceDir}" "${-revision}")

      set_target_properties (
        "Dapper::DAPs::${-hash}"
        PROPERTIES
          Dapper::DAP::Fragment "${-revision}"
          Dapper::DAP::SourceDir "${-sourceDir}"
      )
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
    get_target_property (
      -declarations "Dapper::DAPs::${-dapId}" Dapper::DAP::Declarations
    )
    foreach (-decl IN LISTS -declarations)
      get_target_property (
        -name "Dapper::Declarations::${-decl}" Dapper::Declaration::Name
      )
      if (NOT -name IN_LIST -names)
        list (APPEND -names "${-name}")
      endif ()
      get_target_property (
        -selectedDap "Dapper::Names::${-name}" Dapper::Name::SelectedPackage
      )
      if (NOT -selectedDap IN_LIST -daps)
        list (APPEND -daps "${-selectedDap}")
        list (APPEND -queue "${-selectedDap}")
      endif ()
    endforeach ()
  endwhile ()
  set (${-outNames} "${-names}" PARENT_SCOPE)
endfunction ()

function (DAPPER_RESOLVE_DEPENDENCIES dapperCurrentIntegration)
  if (NOT dapperCurrentIntegration IN_LIST dapperAvailableIntegrations)
    message (FATAL_ERROR "No such integration: ${dapperCurrentIntegration}")
  endif ()

  if (DAPPER_TOP_LEVEL_DIR)
    return ()
  endif ()

  set (DAPPER_TOP_LEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  if (NOT DAPPI_EXECUTABLE)
    set (-dappiBinDir "${CMAKE_CURRENT_BINARY_DIR}/dappi")
    message (STATUS "Configuring dappi...")
    execute_process (
      COMMAND
        ${CMAKE_COMMAND}
        -S "${Dapper_DIR}/Tools/dappi"
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
      dappi PATHS "${-dappiBinDir}/install/bin" NO_DEFAULT_PATHS
    )
  endif ()

  message (STATUS "Resolving dependencies...")

  _DAPPER_NEW_DAP(ROOT)
  set_target_properties (
    Dapper::DAPs::ROOT
    PROPERTIES
      Dapper::DAP::Name "${PROJECT_NAME}"
      Dapper::DAP::Version "${PROJECT_VERSION}"
      Dapper::DAP::SourceDir "${CMAKE_CURRENT_SOURCE_DIR}"
  )

  _DAPPER_NEW_NAME("${PROJECT_NAME}")
  set_target_properties (
    "Dapper::Names::${PROJECT_NAME}"
    PROPERTIES
      Dapper::Name::SelectedPackage ROOT
      Dapper::Name::KnownPackages ROOT
  )

  _DAPPER_LOAD_DA(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

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
      get_target_property (
        -declarations
        "Dapper::DAPs::${-dapId}"
        Dapper::DAP::Declarations
      )

      foreach (-decl IN LISTS -declarations)
        get_target_property (
          -nameFromDecl
          "Dapper::Declarations::${-decl}"
          Dapper::Declaration::Name
        )
        get_target_property (
          -requiredVersion
          "Dapper::Declarations::${-decl}"
          Dapper::Declaration::RequiredVersion
        )
        get_target_property (
          -knownLocations
          "Dapper::Declarations::${-decl}"
          Dapper::Declaration::KnownLocations
        )

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
            get_target_property (
              -name "Dapper::DAPs::${-hash}" Dapper::DAP::Name
            )
            if (-name)
              list (APPEND -names "${-name}")
            else ()
              set (-name "${-nameFromDecl}")
            endif ()

            if (NOT TARGET "Dapper::Names::${-name}")
              _DAPPER_NEW_NAME("${-name}")
              if (NOT -name IN_LIST -allNames)
                list (APPEND -allNames "${-name}")
              endif ()
            endif ()
            get_target_property (
              -knownPackages
              "Dapper::Names::${-name}"
              Dapper::Name::KnownPackages
            )
            if (NOT -hash IN_LIST -knownPackages)
              list (APPEND -knownPackages "${-hash}")
              set_target_properties (
                "Dapper::Names::${-name}"
                PROPERTIES
                  Dapper::Name::KnownPackages "${-knownPackages}"
              )
            endif ()

            if (NOT -hash IN_LIST -allDaps)
              list (APPEND -allDaps "${-hash}")
            endif ()
          endforeach ()
        endforeach ()

        list (REMOVE_DUPLICATES -names)
        list (LENGTH -names -namesLen)
        if (-namesLen EQUAL 0)
          message (
            FATAL_ERROR
            "DAP name not provided by ${dapperDaps_${-decl}_Locations} "
            "at ${dapperDaps_${-decl}_From}"
          )
        elseif (-namesLen GREATER 1)
          message (
            FATAL_ERROR
            "DAP name mismatching: ${-names} "
            "at ${dapperDaps_${-decl}_From}"
          )
        endif ()

        if (NOT -nameFromDecl)
          set_target_properties (
            "Dapper::Declarations::${-decl}"
            PROPERTIES
              Dapper::Declaration::Name "${-names}"
          )
        endif ()

        if (TARGET "Dapper::Names::${-names}")
          get_target_property (
            -selectedDapId
            "Dapper::Names::${-names}"
            Dapper::Name::SelectedPackage
          )
          if (-selectedDapId AND NOT -selectedDapId IN_LIST -processedDaps)
            list (APPEND -unprocessedDaps "${-selectedDapId}")
            list (REMOVE_DUPLICATES -unprocessedDaps)
          endif ()
        endif ()
      endforeach ()
    endwhile ()

    set (-namePairs)
    foreach (-name IN LISTS -allNames)
      if (TARGET "Dapper::Names::${-name}")
        set (-members)

        get_target_property (
          -selectedDapId
          "Dapper::Names::${-name}"
          Dapper::Name::SelectedPackage
        )
        if (-selectedDapId)
          string (
            CONFIGURE [["@-selectedDapId@"]] -jsonDapId @ONLY ESCAPE_QUOTES
          )
          list (APPEND -members selected "${-jsonDapId}")
        endif ()

        get_target_property (
          -knownDapIds
          "Dapper::Names::${-name}"
          Dapper::Name::KnownPackages
        )
        list (APPEND -relevantDaps ${-knownDapIds})
        _DAPPER_LIST_TO_JSON(-jsonKnown ${-knownDapIds})
        list (APPEND -members known "${-jsonKnown}")

        _DAPPER_KV_PAIRS_TO_JSON(-jsonName ${-members})
        list (APPEND -namePairs "${-name}" "${-jsonName}")
      endif ()
    endforeach ()
    _DAPPER_KV_PAIRS_TO_JSON(-jsonNames ${-namePairs})

    foreach (-dapId IN LISTS -allDaps)
      get_target_property (
        -version "Dapper::DAPs::${-dapId}" Dapper::DAP::Version
      )
      string (
        CONFIGURE [[{"version":"@-version@"}]] -jsonDap @ONLY ESCAPE_QUOTES
      )
      get_target_property (
        -version
        "Dapper::DAPs::${-dapId}"
        Dapper::DAP::Version
      )
      string (CONFIGURE [["@-version@"]] -jsonVersion @ONLY ESCAPE_QUOTES)

      set (-dependencies)
      get_target_property (
        -declarations
        "Dapper::DAPs::${-dapId}"
        Dapper::DAP::Declarations
      )
      foreach (-decl IN LISTS -declarations)
        get_target_property (
          -name
          "Dapper::Declarations::${-decl}"
          Dapper::Declaration::Name
        )
        if (-name IN_LIST -allNames)
          get_target_property (
            -requiredVersion
            "Dapper::Declarations::${-decl}"
            Dapper::Declaration::RequiredVersion
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
    set (-inputJsonFile "${CMAKE_CURRENT_BINARY_DIR}/dappi.json")
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
  cmake_language (
    CALL "_DAPPER_FINALIZE_DEPENDENCIES_FOR_${dapperCurrentIntegration}"
  )
  set (DAPPER_TOP_LEVEL_DIR "${DAPPER_TOP_LEVEL_DIR}" PARENT_SCOPE)
endfunction ()

find_package (Git REQUIRED)

_DAPPER_INIT()

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
