cmake_minimum_required(VERSION 3.20)

project(LucidEngine
    VERSION 0.1.0
    DESCRIPTION "A game engine with an embedded data-oriented scripting language"
    LANGUAGES CXX
)

# ---- Global settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Put binaries in predictable places
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Multi-config generators (MSVC) append a config name; unify it
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# ---- Build options ----
option(LUCID_BUILD_EDITOR "Build the editor executable" ON)
option(LUCID_BUILD_GAME   "Build the game executable"   ON)
option(LUCID_BUILD_TESTS  "Build tests"                 OFF)

# ---- Dependencies ----
add_subdirectory(externals)

# ---- Libraries ----
add_subdirectory(runtime)
add_subdirectory(engine)

# ---- Editor library (only if the editor is being built) ----
if(LUCID_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

# ---- Executables ----
if(LUCID_BUILD_GAME)
    add_subdirectory(game)
endif()

if(LUCID_BUILD_EDITOR)
    add_subdirectory(editor_main)
endif()

# ---- Tests ----
if(LUCID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Summary ----
message(STATUS "")
message(STATUS "Lucid Engine ${PROJECT_VERSION}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build editor:  ${LUCID_BUILD_EDITOR}")
message(STATUS "  Build game:    ${LUCID_BUILD_GAME}")
message(STATUS "  Build tests:   ${LUCID_BUILD_TESTS}")
message(STATUS "  Output dir:    ${CMAKE_BINARY_DIR}/bin")
message(STATUS "")