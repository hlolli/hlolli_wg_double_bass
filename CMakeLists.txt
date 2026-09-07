cmake_minimum_required(VERSION 3.16)

set(_hlolli_wg_double_bass_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(hlolli_wg_double_bass VERSION 1.0.0 LANGUAGES C)
  set(_hlolli_wg_double_bass_top_level ON)
endif()

option(HLOLLI_WG_DOUBLE_BASS_STRICT_WARNINGS
       "Enable strict compiler warnings for double-bass targets" ON)
option(HLOLLI_WG_DOUBLE_BASS_WARNINGS_AS_ERRORS
       "Treat double-bass compiler warnings as errors" OFF)
option(HLOLLI_WG_DOUBLE_BASS_BUILD_TESTS
       "Add the native Csound tests" ${_hlolli_wg_double_bass_top_level})
option(HLOLLI_WG_DOUBLE_BASS_INSTALL
       "Install the Csound runtime module"
       ${_hlolli_wg_double_bass_top_level})

set(HLOLLI_WG_DOUBLE_BASS_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/src/hlolli_wg_double_bass.c")
set(HLOLLI_WG_DOUBLE_BASS_MODEL
    "${CMAKE_CURRENT_LIST_DIR}/model/double_bass-v1.json")
set(HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR
    "${CMAKE_CURRENT_LIST_DIR}/tools/generate_model.py")
set(HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER
    "${CMAKE_CURRENT_LIST_DIR}/tools/prepare_wasm_source.py")
set(HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER
    "${CMAKE_CURRENT_LIST_DIR}/tools/run_wasm_workbench_test.ts")

foreach(_hlolli_required_file IN ITEMS
    "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
    "${HLOLLI_WG_DOUBLE_BASS_MODEL}"
    "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}"
    "${HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER}"
    "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}")
  if(NOT EXISTS "${_hlolli_required_file}")
    message(FATAL_ERROR "Required file is missing: ${_hlolli_required_file}")
  endif()
endforeach()

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

function(_hlolli_configure_c_target target)
  set_target_properties("${target}" PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    C_VISIBILITY_PRESET hidden)
  if(HLOLLI_WG_DOUBLE_BASS_STRICT_WARNINGS)
    if(MSVC)
      target_compile_options("${target}" PRIVATE /W4)
      if(HLOLLI_WG_DOUBLE_BASS_WARNINGS_AS_ERRORS)
        target_compile_options("${target}" PRIVATE /WX)
      endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options("${target}" PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wformat=2
        -Wshadow
        -Wstrict-prototypes)
      if(HLOLLI_WG_DOUBLE_BASS_WARNINGS_AS_ERRORS)
        target_compile_options("${target}" PRIVATE -Werror)
      endif()
    endif()
  endif()
endfunction()

function(_hlolli_find_csound_header_target result)
  foreach(candidate IN ITEMS
      Csound::CsoundLib64 Csound::CsoundLib Csound::Csound
      CsoundLib64 CsoundLib)
    if(TARGET "${candidate}")
      get_target_property(candidate_includes "${candidate}"
                          INTERFACE_INCLUDE_DIRECTORIES)
      if(candidate_includes
          AND NOT candidate_includes MATCHES "-NOTFOUND$")
        set("${result}" "${candidate}" PARENT_SCOPE)
        return()
      endif()
    endif()
  endforeach()
  set("${result}" "" PARENT_SCOPE)
endfunction()

set(_hlolli_default_csound_build
    "${CMAKE_CURRENT_LIST_DIR}/../csound/build")
if(NOT EXISTS "${_hlolli_default_csound_build}/include/version.h")
  set(_hlolli_default_csound_build "")
endif()
set(HLOLLI_CSOUND_BUILD_DIR "${_hlolli_default_csound_build}" CACHE PATH
    "Csound build tree containing generated version headers")

set(_hlolli_default_csound_source
    "${CMAKE_CURRENT_LIST_DIR}/../csound")
if(NOT EXISTS "${_hlolli_default_csound_source}/include/csdl.h")
  set(_hlolli_default_csound_source "")
endif()
set(HLOLLI_CSOUND_SOURCE_DIR "${_hlolli_default_csound_source}" CACHE PATH
    "Csound source tree, used when public and generated headers are split")

set(HLOLLI_CSOUND_INCLUDE_DIR "" CACHE PATH
    "Installed include directory containing all public and version headers")

set(_hlolli_default_csound_executable
    "${CMAKE_CURRENT_LIST_DIR}/../csound/build/csound")
if(NOT EXISTS "${_hlolli_default_csound_executable}")
  set(_hlolli_default_csound_executable "")
endif()
set(CSOUND_EXECUTABLE "${_hlolli_default_csound_executable}" CACHE FILEPATH
    "Path to the Csound executable")

set(HLOLLI_WG_DOUBLE_BASS_CUSTOM_CMAKE
    "${CMAKE_CURRENT_LIST_DIR}/Custom.cmake" CACHE FILEPATH
    "Optional local CMake settings file")
if(EXISTS "${HLOLLI_WG_DOUBLE_BASS_CUSTOM_CMAKE}")
  message(STATUS
      "hlolli_wg_double_bass: including ${HLOLLI_WG_DOUBLE_BASS_CUSTOM_CMAKE}")
  include("${HLOLLI_WG_DOUBLE_BASS_CUSTOM_CMAKE}")
endif()

if(NOT CSOUND_EXECUTABLE)
  find_program(_hlolli_found_csound NAMES csound csound64)
  if(_hlolli_found_csound)
    set(CSOUND_EXECUTABLE "${_hlolli_found_csound}" CACHE FILEPATH
        "Path to the Csound executable" FORCE)
  endif()
endif()

set(_hlolli_using_csound_helper OFF)
set(_hlolli_csound_header_target "")
set(_hlolli_csound_split_include_dirs "")
set(_hlolli_csound_version_header "")

if(COMMAND make_plugin AND DEFINED libcsound_public_include_dirs)
  set(_hlolli_using_csound_helper ON)
  message(STATUS "hlolli_wg_double_bass: using Csound's make_plugin() helper")
else()
  if(HLOLLI_CSOUND_INCLUDE_DIR)
    foreach(required_header IN ITEMS csdl.h version.h float-version.h)
      if(NOT EXISTS "${HLOLLI_CSOUND_INCLUDE_DIR}/${required_header}")
        message(FATAL_ERROR
          "HLOLLI_CSOUND_INCLUDE_DIR=${HLOLLI_CSOUND_INCLUDE_DIR} lacks "
          "${required_header}. Use HLOLLI_CSOUND_BUILD_DIR and "
          "HLOLLI_CSOUND_SOURCE_DIR for a source checkout.")
      endif()
    endforeach()
    set(_hlolli_csound_version_header
        "${HLOLLI_CSOUND_INCLUDE_DIR}/version.h")
  elseif(HLOLLI_CSOUND_BUILD_DIR)
    find_path(_hlolli_csound_build_include
      NAMES csdl.h
      HINTS "${HLOLLI_CSOUND_BUILD_DIR}/include"
      NO_DEFAULT_PATH)
    if(_hlolli_csound_build_include
        AND EXISTS "${_hlolli_csound_build_include}/version.h"
        AND EXISTS "${_hlolli_csound_build_include}/float-version.h")
      set(HLOLLI_CSOUND_INCLUDE_DIR "${_hlolli_csound_build_include}")
      set(_hlolli_csound_version_header
          "${HLOLLI_CSOUND_INCLUDE_DIR}/version.h")
    elseif(HLOLLI_CSOUND_SOURCE_DIR
        AND EXISTS "${HLOLLI_CSOUND_SOURCE_DIR}/include/csdl.h"
        AND EXISTS "${HLOLLI_CSOUND_BUILD_DIR}/include/version.h"
        AND EXISTS "${HLOLLI_CSOUND_BUILD_DIR}/include/float-version.h")
      list(APPEND _hlolli_csound_split_include_dirs
        "${HLOLLI_CSOUND_BUILD_DIR}/include"
        "${HLOLLI_CSOUND_SOURCE_DIR}/include")
      set(_hlolli_csound_version_header
          "${HLOLLI_CSOUND_BUILD_DIR}/include/version.h")
    endif()
  endif()

  if(NOT HLOLLI_CSOUND_INCLUDE_DIR
      AND NOT _hlolli_csound_split_include_dirs)
    _hlolli_find_csound_header_target(_hlolli_csound_header_target)
  endif()

  if(NOT HLOLLI_CSOUND_INCLUDE_DIR
      AND NOT _hlolli_csound_split_include_dirs
      AND NOT _hlolli_csound_header_target)
    find_package(Csound 7 CONFIG QUIET)
    _hlolli_find_csound_header_target(_hlolli_csound_header_target)
  endif()

  if(NOT HLOLLI_CSOUND_INCLUDE_DIR
      AND NOT _hlolli_csound_split_include_dirs
      AND NOT _hlolli_csound_header_target)
    find_path(_hlolli_installed_csound_include
      NAMES csdl.h
      HINTS
        "$ENV{CSOUND_INCLUDE_DIR}"
        "$ENV{CSOUND_HOME}/include"
        "$ENV{CSOUND_HOME}/Headers"
        "$ENV{HOME}/Library/Frameworks/CsoundLib64.framework/Headers"
        "/Library/Frameworks/CsoundLib64.framework/Headers"
        "/Applications/Csound/CsoundLib64.framework/Headers"
      PATH_SUFFIXES csound Headers)
    if(_hlolli_installed_csound_include)
      foreach(required_header IN ITEMS csdl.h version.h float-version.h)
        if(NOT EXISTS
            "${_hlolli_installed_csound_include}/${required_header}")
          message(FATAL_ERROR
            "Csound include folder ${_hlolli_installed_csound_include} lacks "
            "${required_header}.")
        endif()
      endforeach()
      set(HLOLLI_CSOUND_INCLUDE_DIR
          "${_hlolli_installed_csound_include}")
      set(_hlolli_csound_version_header
          "${HLOLLI_CSOUND_INCLUDE_DIR}/version.h")
    endif()
  endif()

  if(NOT HLOLLI_CSOUND_INCLUDE_DIR
      AND NOT _hlolli_csound_split_include_dirs
      AND NOT _hlolli_csound_header_target)
    message(FATAL_ERROR
      "Could not find Csound headers. Pass HLOLLI_CSOUND_BUILD_DIR and "
      "HLOLLI_CSOUND_SOURCE_DIR, or pass HLOLLI_CSOUND_INCLUDE_DIR.")
  endif()

  if(_hlolli_csound_version_header)
    file(STRINGS "${_hlolli_csound_version_header}"
         _hlolli_required_csound_major_line
         REGEX "^#define[ \t]+CS_VERSION")
    string(REGEX MATCH "[0-9]+" _hlolli_required_csound_major
           "${_hlolli_required_csound_major_line}")
    if("${_hlolli_required_csound_major}" STREQUAL ""
        OR _hlolli_required_csound_major LESS 7)
      message(FATAL_ERROR
        "hlolli_wg_double_bass requires Csound 7 headers; found "
        "${_hlolli_csound_version_header}")
    endif()
  endif()
endif()

function(_hlolli_attach_csound_headers target)
  if(_hlolli_using_csound_helper)
    target_include_directories("${target}" SYSTEM PRIVATE
                               ${libcsound_public_include_dirs})
  elseif(HLOLLI_CSOUND_INCLUDE_DIR)
    target_include_directories("${target}" SYSTEM PRIVATE
                               "${HLOLLI_CSOUND_INCLUDE_DIR}")
  elseif(_hlolli_csound_split_include_dirs)
    target_include_directories("${target}" SYSTEM PRIVATE
                               ${_hlolli_csound_split_include_dirs})
  elseif(_hlolli_csound_header_target)
    target_include_directories("${target}" SYSTEM PRIVATE
      "$<TARGET_PROPERTY:${_hlolli_csound_header_target},INTERFACE_INCLUDE_DIRECTORIES>")
  endif()
  if(UNIX AND NOT APPLE)
    target_link_libraries("${target}" PRIVATE m)
  endif()
endfunction()

if(_hlolli_using_csound_helper)
  make_plugin(hlolli_wg_double_bass "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
else()
  if(APPLE)
    add_library(hlolli_wg_double_bass SHARED
                "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
  else()
    add_library(hlolli_wg_double_bass MODULE
                "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
  endif()
  _hlolli_attach_csound_headers(hlolli_wg_double_bass)
endif()

_hlolli_configure_c_target(hlolli_wg_double_bass)
target_compile_definitions(hlolli_wg_double_bass PRIVATE BUILD_PLUGINS)
set_target_properties(hlolli_wg_double_bass PROPERTIES
  EXPORT_NAME wg_double_bass)
if(TARGET sync_build_include_headers)
  add_dependencies(hlolli_wg_double_bass sync_build_include_headers)
endif()
add_library(hlolli::wg_double_bass ALIAS hlolli_wg_double_bass)

set(_hlolli_model_check_stamp
    "${CMAKE_CURRENT_BINARY_DIR}/model/double_bass-v1.checked")
add_custom_command(
  OUTPUT "${_hlolli_model_check_stamp}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory
          "${CMAKE_CURRENT_BINARY_DIR}/model"
  COMMAND "${Python3_EXECUTABLE}" -B
          "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}" --check
  COMMAND "${Python3_EXECUTABLE}" -B
          "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}" --self-test
  COMMAND "${CMAKE_COMMAND}" -E touch "${_hlolli_model_check_stamp}"
  DEPENDS
    "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}"
    "${HLOLLI_WG_DOUBLE_BASS_MODEL}"
    "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  COMMENT "Checking fixed double-bass model data"
  VERBATIM)
add_custom_target(hlolli_wg_double_bass_check_model
  DEPENDS "${_hlolli_model_check_stamp}")
add_custom_target(hlolli_wg_double_bass_generate_model
  COMMAND "${Python3_EXECUTABLE}" -B
          "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}"
  DEPENDS
    "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}"
    "${HLOLLI_WG_DOUBLE_BASS_MODEL}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  COMMENT "Generating fixed double-bass model data"
  VERBATIM)
add_dependencies(hlolli_wg_double_bass hlolli_wg_double_bass_check_model)

set(_hlolli_wasm_source
    "${CMAKE_CURRENT_BINARY_DIR}/wasm/hlolli_wg_double_bass.c")
add_custom_command(
  OUTPUT "${_hlolli_wasm_source}"
  COMMAND "${Python3_EXECUTABLE}" -B
          "${HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER}"
          --source "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
          --output "${_hlolli_wasm_source}"
  DEPENDS
    "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
    "${HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER}"
  COMMENT "Preparing bounded browser compiler source"
  VERBATIM)
add_custom_target(hlolli_wg_double_bass_prepare_wasm_source
  DEPENDS "${_hlolli_wasm_source}")
add_dependencies(hlolli_wg_double_bass_prepare_wasm_source
                 hlolli_wg_double_bass_check_model)

# Explicit alternative for the selected legato demo; standard module unchanged.
set(_hlolli_legato_source
    "${CMAKE_CURRENT_BINARY_DIR}/legato/legato.c")
set(_hlolli_legato_wasm_source
    "${CMAKE_CURRENT_BINARY_DIR}/legato/legato-wasm.c")
add_custom_command(
  OUTPUT "${_hlolli_legato_source}" "${_hlolli_legato_wasm_source}"
  COMMAND "${Python3_EXECUTABLE}" -B
          "${CMAKE_CURRENT_LIST_DIR}/tools/prepare_legato_source.py"
          --source "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
          --output "${_hlolli_legato_source}"
  COMMAND "${Python3_EXECUTABLE}" -B
          "${HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER}"
          --source "${_hlolli_legato_source}"
          --output "${_hlolli_legato_wasm_source}"
  DEPENDS "${HLOLLI_WG_DOUBLE_BASS_SOURCE}"
          "${CMAKE_CURRENT_LIST_DIR}/tools/prepare_legato_source.py"
          "${HLOLLI_WG_DOUBLE_BASS_WASM_PREPARER}"
  COMMENT "Preparing optional full-legato demo module"
  VERBATIM)
# Do not use Csound's make_plugin helper here: it adds an install rule.
if(APPLE)
  add_library(hlolli_wg_double_bass_legato SHARED
              "${_hlolli_legato_source}")
else()
  add_library(hlolli_wg_double_bass_legato MODULE
              "${_hlolli_legato_source}")
endif()
_hlolli_attach_csound_headers(hlolli_wg_double_bass_legato)
_hlolli_configure_c_target(hlolli_wg_double_bass_legato)
target_compile_definitions(hlolli_wg_double_bass_legato PRIVATE
  BUILD_PLUGINS)
add_dependencies(hlolli_wg_double_bass_legato
                 hlolli_wg_double_bass_check_model)
if(TARGET sync_build_include_headers)
  add_dependencies(hlolli_wg_double_bass_legato
                   sync_build_include_headers)
endif()

set(_hlolli_default_workbench
    "${CMAKE_CURRENT_LIST_DIR}/../csound-wasm-plugin-compiler")
if(NOT EXISTS "${_hlolli_default_workbench}/package.json")
  set(_hlolli_default_workbench "")
endif()
set(HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR "${_hlolli_default_workbench}"
    CACHE PATH "Csound browser opcode workbench checkout")
find_program(HLOLLI_BUN_EXECUTABLE NAMES bun)
if(HLOLLI_BUN_EXECUTABLE
    AND CSOUND_EXECUTABLE
    AND EXISTS "${CSOUND_EXECUTABLE}"
    AND EXISTS "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}/package.json"
    AND EXISTS
        "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}/node_modules/@csound/browser/dist/csound.js")
  add_custom_target(hlolli_wg_double_bass_check_wasm
    COMMAND "${HLOLLI_BUN_EXECUTABLE}"
            "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
            --compiler-root "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}"
            --source "${_hlolli_wasm_source}"
            --csd "${CMAKE_CURRENT_LIST_DIR}/examples/wasm_smoke.csd"
            --output "${CMAKE_CURRENT_BINARY_DIR}/wasm/facts.json"
    COMMAND "${HLOLLI_BUN_EXECUTABLE}"
            "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
            --compiler-root "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}"
            --source "${_hlolli_wasm_source}"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity.csd"
            --output "${CMAKE_CURRENT_BINARY_DIR}/wasm/pcm-facts.json"
            --pcm-output "${CMAKE_CURRENT_BINARY_DIR}/wasm/browser.f64"
    COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_LIST_DIR}/tests/compare_wasm_pcm.py"
            --csound "${CSOUND_EXECUTABLE}"
            --module "$<TARGET_FILE:hlolli_wg_double_bass>"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity.csd"
            --browser-pcm "${CMAKE_CURRENT_BINARY_DIR}/wasm/browser.f64"
            --browser-facts
            "${CMAKE_CURRENT_BINARY_DIR}/wasm/pcm-facts.json"
    COMMAND "${HLOLLI_BUN_EXECUTABLE}"
            "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
            --compiler-root "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}"
            --source "${_hlolli_wasm_source}"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity_phrase.csd"
            --output "${CMAKE_CURRENT_BINARY_DIR}/wasm/phrase-facts.json"
            --pcm-output "${CMAKE_CURRENT_BINARY_DIR}/wasm/phrase-browser.f64"
    COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_LIST_DIR}/tests/compare_wasm_pcm.py"
            --csound "${CSOUND_EXECUTABLE}"
            --module "$<TARGET_FILE:hlolli_wg_double_bass>"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity_phrase.csd"
            --browser-pcm "${CMAKE_CURRENT_BINARY_DIR}/wasm/phrase-browser.f64"
            --browser-facts
            "${CMAKE_CURRENT_BINARY_DIR}/wasm/phrase-facts.json"
    COMMAND "${HLOLLI_BUN_EXECUTABLE}"
            "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
            --compiler-root "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}"
            --source "${_hlolli_wasm_source}"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity_midpoint.csd"
            --output "${CMAKE_CURRENT_BINARY_DIR}/wasm/midpoint-facts.json"
            --pcm-output "${CMAKE_CURRENT_BINARY_DIR}/wasm/midpoint-browser.f64"
    COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_LIST_DIR}/tests/compare_wasm_pcm.py"
            --csound "${CSOUND_EXECUTABLE}"
            --module "$<TARGET_FILE:hlolli_wg_double_bass>"
            --csd "${CMAKE_CURRENT_LIST_DIR}/tests/parity_midpoint.csd"
            --browser-pcm "${CMAKE_CURRENT_BINARY_DIR}/wasm/midpoint-browser.f64"
            --browser-facts
            "${CMAKE_CURRENT_BINARY_DIR}/wasm/midpoint-facts.json"
    COMMAND "${HLOLLI_BUN_EXECUTABLE}"
            "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
            --compiler-root "${HLOLLI_WG_DOUBLE_BASS_WORKBENCH_DIR}"
            --source "${_hlolli_legato_wasm_source}"
            --csd "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i_legato.csd"
            --max-blocks 150000
            --output "${CMAKE_CURRENT_BINARY_DIR}/wasm/full-legato-facts.json"
            --pcm-output "${CMAKE_CURRENT_BINARY_DIR}/wasm/full-legato-browser.f64"
    COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_LIST_DIR}/tests/compare_wasm_pcm.py"
            --csound "${CSOUND_EXECUTABLE}"
            --module "$<TARGET_FILE:hlolli_wg_double_bass_legato>"
            --csd "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i_legato.csd"
            --expected-samples 9000000
            --browser-pcm "${CMAKE_CURRENT_BINARY_DIR}/wasm/full-legato-browser.f64"
            --browser-facts
            "${CMAKE_CURRENT_BINARY_DIR}/wasm/full-legato-facts.json"
    DEPENDS
      hlolli_wg_double_bass_prepare_wasm_source
      hlolli_wg_double_bass
      hlolli_wg_double_bass_legato
      "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i_legato.csd"
      "${CMAKE_CURRENT_LIST_DIR}/examples/wasm_smoke.csd"
      "${CMAKE_CURRENT_LIST_DIR}/tests/compare_wasm_pcm.py"
      "${CMAKE_CURRENT_LIST_DIR}/tests/parity.csd"
      "${CMAKE_CURRENT_LIST_DIR}/tests/parity_phrase.csd"
      "${CMAKE_CURRENT_LIST_DIR}/tests/parity_midpoint.csd"
      "${HLOLLI_WG_DOUBLE_BASS_WASM_RUNNER}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    USES_TERMINAL
    COMMENT "Compiling and playing the double bass in the browser workbench"
    VERBATIM)
endif()

if(HLOLLI_WG_DOUBLE_BASS_BUILD_TESTS)
  enable_testing()

  add_test(NAME hlolli_wg_double_bass.model_data
    COMMAND "${Python3_EXECUTABLE}" -B
            "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}" --check)
  set_tests_properties(hlolli_wg_double_bass.model_data PROPERTIES
    LABELS "model;tooling"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    TIMEOUT 30)

  add_test(NAME hlolli_wg_double_bass.model_generator_self_test
    COMMAND "${Python3_EXECUTABLE}" -B
            "${HLOLLI_WG_DOUBLE_BASS_MODEL_GENERATOR}" --self-test)
  set_tests_properties(
    hlolli_wg_double_bass.model_generator_self_test PROPERTIES
    LABELS "model;tooling"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    TIMEOUT 30)

  add_test(NAME hlolli_wg_double_bass.wasm_source_preparer
    COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_LIST_DIR}/tests/test_prepare_wasm_source.py")
  set_tests_properties(
    hlolli_wg_double_bass.wasm_source_preparer PROPERTIES
    LABELS "tooling;wasm"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    TIMEOUT 30)

  add_executable(hlolli_wg_double_bass_bow_math
    "${CMAKE_CURRENT_LIST_DIR}/tests/test_bow_math.c")
  _hlolli_configure_c_target(hlolli_wg_double_bass_bow_math)
  _hlolli_attach_csound_headers(hlolli_wg_double_bass_bow_math)
  target_compile_definitions(hlolli_wg_double_bass_bow_math PRIVATE BUILD_PLUGINS)
  add_dependencies(hlolli_wg_double_bass_bow_math
                   hlolli_wg_double_bass_check_model)
  if(TARGET sync_build_include_headers)
    add_dependencies(hlolli_wg_double_bass_bow_math sync_build_include_headers)
  endif()
  add_test(NAME hlolli_wg_double_bass.bow_math
    COMMAND hlolli_wg_double_bass_bow_math)
  set_tests_properties(hlolli_wg_double_bass.bow_math PROPERTIES
    LABELS "physics;bow;math;parity" TIMEOUT 30)

  if(CSOUND_EXECUTABLE AND EXISTS "${CSOUND_EXECUTABLE}")
    add_test(NAME hlolli_wg_double_bass.parity_phrase
      COMMAND "${CSOUND_EXECUTABLE}"
              "--opcode-lib=$<TARGET_FILE:hlolli_wg_double_bass>"
              --sample-accurate --num-threads=1 -n -d -m128
              "${CMAKE_CURRENT_LIST_DIR}/tests/parity_phrase.csd")
    set_tests_properties(hlolli_wg_double_bass.parity_phrase PROPERTIES
      LABELS "parity;bow;handoff" TIMEOUT 30)

    add_test(NAME hlolli_wg_double_bass.parity_midpoint
      COMMAND "${CSOUND_EXECUTABLE}"
              "--opcode-lib=$<TARGET_FILE:hlolli_wg_double_bass>"
              --sample-accurate --num-threads=1 -n -d -m128
              "${CMAKE_CURRENT_LIST_DIR}/tests/parity_midpoint.csd")
    set_tests_properties(hlolli_wg_double_bass.parity_midpoint PROPERTIES
      LABELS "parity;bow;handoff" TIMEOUT 30)

    add_test(NAME hlolli_wg_double_bass.demo_legato
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_legato_demo_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_legato>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i_legato.csd"
              --reference "${CMAKE_CURRENT_LIST_DIR}/tests/parity_full_legato.csd")
    set_tests_properties(hlolli_wg_double_bass.demo_legato PROPERTIES
      LABELS "example;legato;parity" TIMEOUT 120 RUN_SERIAL TRUE)

    foreach(_hlolli_demo IN ITEMS basic solo_techniques section bach_menuet_i)
      add_test(NAME hlolli_wg_double_bass.demo_${_hlolli_demo}
        COMMAND "${CSOUND_EXECUTABLE}"
                "--opcode-lib=$<TARGET_FILE:hlolli_wg_double_bass>"
                -n -d -m128
                "${CMAKE_CURRENT_LIST_DIR}/examples/${_hlolli_demo}.csd")
      set_tests_properties(hlolli_wg_double_bass.demo_${_hlolli_demo}
        PROPERTIES LABELS "example" TIMEOUT 60)
    endforeach()

    add_test(NAME hlolli_wg_double_bass.smoke
      COMMAND "${CSOUND_EXECUTABLE}"
              "--opcode-lib=$<TARGET_FILE:hlolli_wg_double_bass>"
              -n -d -m128
              "${CMAKE_CURRENT_LIST_DIR}/tests/smoke.csd")
    set_tests_properties(hlolli_wg_double_bass.smoke PROPERTIES
      LABELS "api"
      TIMEOUT 30)

    if(_hlolli_using_csound_helper)
      make_plugin(hlolli_wg_double_bass_test
                  "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
    elseif(APPLE)
      add_library(hlolli_wg_double_bass_test SHARED
                  "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
      _hlolli_attach_csound_headers(hlolli_wg_double_bass_test)
    else()
      add_library(hlolli_wg_double_bass_test MODULE
                  "${HLOLLI_WG_DOUBLE_BASS_SOURCE}")
      _hlolli_attach_csound_headers(hlolli_wg_double_bass_test)
    endif()
    _hlolli_configure_c_target(hlolli_wg_double_bass_test)
    target_compile_definitions(hlolli_wg_double_bass_test PRIVATE
      BUILD_PLUGINS
      HLOLLI_WG_DOUBLE_BASS_TEST_API=1)
    if(TARGET sync_build_include_headers)
      add_dependencies(hlolli_wg_double_bass_test sync_build_include_headers)
    endif()
    add_dependencies(hlolli_wg_double_bass_test
                     hlolli_wg_double_bass_check_model)

    set_source_files_properties("${_hlolli_wasm_source}" PROPERTIES
      GENERATED TRUE)
    if(_hlolli_using_csound_helper)
      make_plugin(hlolli_wg_double_bass_prepared_test
                  "${_hlolli_wasm_source}")
    elseif(APPLE)
      add_library(hlolli_wg_double_bass_prepared_test SHARED
                  "${_hlolli_wasm_source}")
      _hlolli_attach_csound_headers(hlolli_wg_double_bass_prepared_test)
    else()
      add_library(hlolli_wg_double_bass_prepared_test MODULE
                  "${_hlolli_wasm_source}")
      _hlolli_attach_csound_headers(hlolli_wg_double_bass_prepared_test)
    endif()
    _hlolli_configure_c_target(hlolli_wg_double_bass_prepared_test)
    target_compile_definitions(hlolli_wg_double_bass_prepared_test PRIVATE
      BUILD_PLUGINS)
    if(TARGET sync_build_include_headers)
      add_dependencies(hlolli_wg_double_bass_prepared_test
                       sync_build_include_headers)
    endif()
    add_dependencies(hlolli_wg_double_bass_prepared_test
                     hlolli_wg_double_bass_prepare_wasm_source)

    add_test(NAME hlolli_wg_double_bass.ownership
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_ownership_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --model "${HLOLLI_WG_DOUBLE_BASS_MODEL}"
              --ownership-csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/ownership.csd"
              --handoff-csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/handoff.csd")
    set_tests_properties(hlolli_wg_double_bass.ownership PROPERTIES
      LABELS "ownership"
      RUN_SERIAL TRUE
      TIMEOUT 60)

    add_test(NAME hlolli_wg_double_bass.engine_parity
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_parity_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass>"
              --test-module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --prepared-module
              "$<TARGET_FILE:hlolli_wg_double_bass_prepared_test>"
              --constructors-csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/constructors.csd"
              --constructor-bounds-csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/constructor_bounds.csd"
              --parity-csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/parity.csd")
    set_tests_properties(hlolli_wg_double_bass.engine_parity PROPERTIES
      LABELS "api;parity;worker"
      RUN_SERIAL TRUE
      TIMEOUT 120)

    add_test(NAME hlolli_wg_double_bass.geometry
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_geometry_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/geometry.csd")
    set_tests_properties(hlolli_wg_double_bass.geometry PROPERTIES
      LABELS "api;geometry"
      RUN_SERIAL TRUE
      TIMEOUT 180)

    add_test(NAME hlolli_wg_double_bass.pizzicato
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_pizzicato_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/pizzicato.csd"
              --model "${HLOLLI_WG_DOUBLE_BASS_MODEL}")
    set_tests_properties(hlolli_wg_double_bass.pizzicato PROPERTIES
      LABELS "physics;pizzicato;string;model"
      RUN_SERIAL TRUE
      TIMEOUT 90)

    add_test(NAME hlolli_wg_double_bass.bow_control
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_bow_control_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/bow_control.csd"
              --model "${HLOLLI_WG_DOUBLE_BASS_MODEL}")
    set_tests_properties(hlolli_wg_double_bass.bow_control PROPERTIES
      LABELS "physics;bow;string;model"
      RUN_SERIAL TRUE
      TIMEOUT 90)

    add_test(NAME hlolli_wg_double_bass.chord
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_chord_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/chord.csd")
    set_tests_properties(hlolli_wg_double_bass.chord PROPERTIES
      LABELS "physics;chord;sympathetic;string;body"
      RUN_SERIAL TRUE
      TIMEOUT 90)

    add_test(NAME hlolli_wg_double_bass.gesture_matrix
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_gesture_matrix_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/gesture_matrix.csd"
              --model "${HLOLLI_WG_DOUBLE_BASS_MODEL}")
    set_tests_properties(hlolli_wg_double_bass.gesture_matrix PROPERTIES
      LABELS "physics;gesture;bow;pizzicato;harmonic;slur;string"
      RUN_SERIAL TRUE
      TIMEOUT 120)

    add_test(NAME hlolli_wg_double_bass.release_stress
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_release_stress_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd
              "${CMAKE_CURRENT_LIST_DIR}/tests/release_stress.csd")
    set_tests_properties(hlolli_wg_double_bass.release_stress PROPERTIES
      LABELS "release;stress;rate;block;string;body"
      RUN_SERIAL TRUE
      TIMEOUT 300)

    add_test(NAME hlolli_wg_double_bass.release_cost
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_cost_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/cost.csd")
    set_tests_properties(hlolli_wg_double_bass.release_cost PROPERTIES
      LABELS "release;cost;handle;worker;parity"
      RUN_SERIAL TRUE
      TIMEOUT 180)

    add_test(NAME hlolli_wg_double_bass.finite_recovery
      COMMAND "${Python3_EXECUTABLE}" -B
              "${CMAKE_CURRENT_LIST_DIR}/tests/run_recovery_test.py"
              --csound "${CSOUND_EXECUTABLE}"
              --module "$<TARGET_FILE:hlolli_wg_double_bass_test>"
              --csd "${CMAKE_CURRENT_LIST_DIR}/tests/recovery.csd")
    set_tests_properties(hlolli_wg_double_bass.finite_recovery PROPERTIES
      LABELS "recovery"
      RUN_SERIAL TRUE
      TIMEOUT 120)
  else()
    message(WARNING
      "Csound was not found; native engine tests are off.")
  endif()
endif()

include(GNUInstallDirs)
set(_hlolli_plugin_folder "plugins64")
if(_hlolli_csound_version_header
    AND EXISTS "${_hlolli_csound_version_header}")
  file(STRINGS "${_hlolli_csound_version_header}"
       _hlolli_csound_major_line REGEX "^#define[ \t]+CS_VERSION")
  file(STRINGS "${_hlolli_csound_version_header}"
       _hlolli_csound_minor_line REGEX "^#define[ \t]+CS_SUBVER")
  string(REGEX MATCH "[0-9]+" _hlolli_csound_major
         "${_hlolli_csound_major_line}")
  string(REGEX MATCH "[0-9]+" _hlolli_csound_minor
         "${_hlolli_csound_minor_line}")
  if(NOT "${_hlolli_csound_major}" STREQUAL ""
      AND NOT "${_hlolli_csound_minor}" STREQUAL "")
    set(_hlolli_plugin_folder
        "plugins64-${_hlolli_csound_major}.${_hlolli_csound_minor}")
  endif()
endif()

if(DEFINED PLUGIN_INSTALL_DIR AND NOT "${PLUGIN_INSTALL_DIR}" STREQUAL "")
  set(_hlolli_default_install_dir "${PLUGIN_INSTALL_DIR}")
else()
  set(_hlolli_default_install_dir
      "${CMAKE_INSTALL_LIBDIR}/csound/${_hlolli_plugin_folder}")
endif()
set(HLOLLI_CSOUND_PLUGIN_DIR "${_hlolli_default_install_dir}" CACHE STRING
    "Csound runtime plugin install directory")

if(HLOLLI_WG_DOUBLE_BASS_INSTALL)
  install(TARGETS hlolli_wg_double_bass
    LIBRARY DESTINATION "${HLOLLI_CSOUND_PLUGIN_DIR}"
    RUNTIME DESTINATION "${HLOLLI_CSOUND_PLUGIN_DIR}"
    COMPONENT Runtime)
  # Keep the alternative out of Csound's auto-loaded plugin directory:
  # it deliberately registers the same opcodes as the standard module.
  install(TARGETS hlolli_wg_double_bass_legato
    LIBRARY DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/hlolli_wg_double_bass/legato"
    RUNTIME DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/hlolli_wg_double_bass/legato"
    COMPONENT Runtime)
  install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    "${CMAKE_CURRENT_LIST_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/hlolli_wg_double_bass"
    COMPONENT Documentation)
  install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/examples/basic.csd"
    "${CMAKE_CURRENT_LIST_DIR}/examples/solo_techniques.csd"
    "${CMAKE_CURRENT_LIST_DIR}/examples/section.csd"
    "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i.csd"
    "${CMAKE_CURRENT_LIST_DIR}/examples/bach_menuet_i_legato.csd"
    "${CMAKE_CURRENT_LIST_DIR}/examples/wasm_smoke.csd"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/hlolli_wg_double_bass/examples"
    COMPONENT Documentation)
endif()
