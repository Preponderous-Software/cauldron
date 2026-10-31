cmake_minimum_required(VERSION 3.20)
project(cauldron VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CAULDRON_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CAULDRON_BUILD_TESTS "Build the test suite" ON)
option(CAULDRON_WEB "Build the browser game (Emscripten + Strata's WebGL 2 backend)" OFF)

# Strata compiles C++ exceptions as wasm exceptions, and everything in one
# .wasm must match.
if(EMSCRIPTEN)
  cmake_minimum_required(VERSION 3.24)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fwasm-exceptions>")
  add_link_options(-fwasm-exceptions -sALLOW_MEMORY_GROWTH=1)
endif()
if(CAULDRON_WEB AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "CAULDRON_WEB needs Emscripten (emcmake cmake ...)")
endif()

function(cauldron_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(CAULDRON_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(CAULDRON_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# The rules: ingredients, recipes, judging a stir. No engine, no GPU.
add_library(cauldron_sim STATIC src/sim/brew.cpp)
target_include_directories(cauldron_sim PUBLIC src)
cauldron_warnings(cauldron_sim)

if(CAULDRON_BUILD_TESTS)
  enable_testing()
  foreach(t brew)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE cauldron_sim)
    target_include_directories(test_${t} PRIVATE tests)
    cauldron_warnings(test_${t})
    add_test(NAME ${t} COMMAND test_${t})
  endforeach()
endif()

# ---------------------------------------------------------------- the game on Strata
# Strata is fetched at CAULDRON_STRATA_REF (the v0.7.0 tag), or taken from a
# local checkout with -DFETCHCONTENT_SOURCE_DIR_STRATA=<path>.
# emcmake cmake -S . -B build-web -DCAULDRON_WEB=ON: the page is
# cauldron.html + .js + .wasm.
if(CAULDRON_WEB)
  include(FetchContent)
  set(CAULDRON_STRATA_REF "20827c3d67e71c1f942c29e7cf8adfa4801188a8" CACHE STRING "Strata git ref (a full commit SHA)")
  set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(STRATA_WITH_SIM OFF CACHE BOOL "" FORCE)
  set(STRATA_WITH_USD OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(strata
    GIT_REPOSITORY https://github.com/Preponderous-Software/strata.git
    GIT_TAG ${CAULDRON_STRATA_REF})
  FetchContent_MakeAvailable(strata)

  add_library(cauldron_play STATIC src/play/cottage.cpp)
  target_include_directories(cauldron_play PUBLIC src)
  target_link_libraries(cauldron_play PUBLIC cauldron_sim strata::static)
  cauldron_warnings(cauldron_play)

  add_executable(cauldron-web app/web_main.cpp)
  target_link_libraries(cauldron-web PRIVATE cauldron_play)
  cauldron_warnings(cauldron-web)
  set_target_properties(cauldron-web PROPERTIES OUTPUT_NAME cauldron SUFFIX ".html")
  target_link_options(cauldron-web PRIVATE
    -sSTACK_SIZE=1MB
    "-sEXPORTED_FUNCTIONS=_main,_cauldron_add,_cauldron_stir,_cauldron_pour,_cauldron_next,_cauldron_restart,_cauldron_peek,_cauldron_tap,_cauldron_resize,_cauldron_cover,_cauldron_ingredients,_cauldron_status"
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  set_property(TARGET cauldron-web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-kit/strata-loader.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-boot.js")
  # The loading shell (web/strata-kit, vendored from Strata) and its bridge go
  # inline into the page, with the download's size (web/strata_page.cmake).
  add_custom_command(TARGET cauldron-web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DHTML=$<TARGET_FILE:cauldron-web>"
            "-DWASM=$<TARGET_FILE_DIR:cauldron-web>/$<TARGET_FILE_BASE_NAME:cauldron-web>.wasm"
            "-DWEB=${CMAKE_CURRENT_SOURCE_DIR}/web" -P "${CMAKE_CURRENT_SOURCE_DIR}/web/strata_page.cmake"
    VERBATIM)
endif()
