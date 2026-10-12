include(FetchContent)

# ---- GLFW ----
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# ---- GLM ----
set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ---- GLAD (pre-generated, vendored) ----
# The files in externals/glad/ were generated once from glad.dav1d.de with:
#   Language: C/C++
#   Specification: OpenGL
#   API: gl=3.3, core
#   Generate a loader: yes
#
# They are committed to the repo so clean builds need no network access.
#
# To regenerate: visit https://glad.dav1d.de/, use the settings above,
# download the zip, and replace the contents of externals/glad/.
add_library(glad STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---- stb (header-only) ----
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

# ---- nlohmann/json (vendored single-header) ----
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---- Lucid Format Library ----
#
# Fetched as a subproject. The options tell the library to build only
# its libraries — no CLI, no tests. The engine owns the build settings.
#
# The library's targets after fetching:
#   lucid_core, lucid_parser, lucid_formatter, lucid_sema,
#   lucid_serialization
#
# The engine links against lucid_sema (for the Registry and Graph types)
# and lucid_serialization (for the .lucgraph loader).

set(LUCID_FORMAT_IS_SUBPROJECT ON  CACHE BOOL "" FORCE)
set(LUCID_FORMAT_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
set(LUCID_FORMAT_BUILD_CLI    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    lucid_format
    GIT_REPOSITORY https://github.com/Axo-1206/Lucid-Graph-Format.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lucid_format)

# ---- Box2D 3.x ----
#
# The 2D physics engine. Version 3.x is a rewrite of the 2.x
# line with a cleaner API. MIT-licensed.
#
# Options below disable Box2D's tests, samples, and benchmarks.
# The library target is `box2d`.

set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED    OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS       OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_SAMPLES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(box2d)

# Override Box2D's runtime library to match ours
if(MSVC)
    set_target_properties(box2d PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
endif()

# ---- Aliases ----
add_library(lucid::glfw ALIAS glfw)
add_library(lucid::glm  ALIAS glm)
add_library(lucid::glad ALIAS glad)
add_library(lucid::stb  ALIAS stb)
add_library(lucid::nlohmann ALIAS nlohmann_json)
add_library(lucid::box2d ALIAS box2d)

message(STATUS "Externals: GLFW 3.4 (fetch), GLM 1.0.1 (fetch), GLAD (vendored), 
stb (vendored), nlohmann/json (vendored), Box2D 3.1.0 (fetch), Lucid Format (fetched)")