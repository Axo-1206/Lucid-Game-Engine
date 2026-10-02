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

# ---- GLAD ----
set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "" FORCE)
set(GLAD_REPRODUCIBLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
    GIT_SHALLOW    TRUE
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(glad)
unset(CMAKE_POLICY_VERSION_MINIMUM)

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
