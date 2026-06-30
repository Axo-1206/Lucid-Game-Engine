cmake_minimum_required(VERSION 3.20)
project(LucidEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories (Decision 29)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dev_build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dev_build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Lucid Kernel (DLL) ---
add_library(luc_kernel SHARED)

target_include_directories(luc_kernel PUBLIC 
    kernel/include
    kernel/src
)

file(GLOB_RECURSE KERNEL_SOURCES 
    "kernel/src/*.cpp"
    "kernel/src/*.hpp"
)

target_sources(luc_kernel PRIVATE ${KERNEL_SOURCES})

# Export macro for Windows
if(WIN32)
    target_compile_definitions(luc_kernel PRIVATE LGE_EXPORT_DLL)
endif()

# --- Dependencies ---

# 1. Vulkan SDK
find_package(Vulkan REQUIRED)

# 2. Header-only externals
target_include_directories(luc_kernel SYSTEM PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    externals/glm
    externals/vma
    externals/miniaudio
    externals/cgltf
    externals/nlohmann
)

# 3. Large Compiled Libraries (Git Submodules)
# We add these only if the folders are not empty
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/glfw/CMakeLists.txt")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(externals/glfw)
    target_link_libraries(luc_kernel PRIVATE glfw)
endif()

# 4. Lucid Compiler (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/lucid_compiler/CMakeLists.txt")
    add_subdirectory(externals/lucid_compiler)
endif()

# 5. RmlUI (Editor shell — Win32 + Vulkan backend)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/rmlui/CMakeLists.txt")

    if(NOT TARGET Freetype::Freetype)
            find_package(Freetype REQUIRED)
    endif()

    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    # Force static linking for RmlDebugger (plugin architecture) to avoid DLL hell
    set(RMLUI_DYNAMIC_LINKING OFF CACHE BOOL "" FORCE)

    add_subdirectory(externals/rmlui)

    # Explicitly build the Vulkan backend
    set(RMLUI_BACKEND "Win32_VK")
    add_subdirectory(externals/rmlui/Backends)

    # Link the kernel against RmlUI and its Vulkan backend
    target_link_libraries(luc_kernel PRIVATE
        RmlUi::Core
        RmlUi::Debugger
        rmlui_backend_Win32_VK
        Vulkan::Vulkan
    )

    # Include RmlUI headers in the kernel's public include path
    target_include_directories(luc_kernel SYSTEM PRIVATE
        externals/rmlui/Include
        externals/rmlui/Backends
    )

    if(WIN32)
        target_link_libraries(luc_kernel PRIVATE imm32)
    endif()
endif()

# --- Lucid Editor (Bootstrap) ---
add_executable(LucidEditor)
target_sources(LucidEditor PRIVATE tools/bootstrap_main.cpp)
target_link_libraries(LucidEditor PRIVATE luc_kernel ${Vulkan_LIBRARIES})

# --- Testing ---
enable_testing()
# add_subdirectory(tests/kernel)
