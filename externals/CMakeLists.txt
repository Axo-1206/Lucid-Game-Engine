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

# ---- Aliases ----
add_library(lucid::glfw ALIAS glfw)
add_library(lucid::glm  ALIAS glm)
add_library(lucid::glad ALIAS glad)
add_library(lucid::stb  ALIAS stb)

message(STATUS "Externals: GLFW 3.4, GLM 1.0.1, GLAD 1.0.36, stb (header-only)")
