cmake_minimum_required(VERSION 3.20)
project(LucidEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dev_build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dev_build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Lucid Kernel (DLL) ---
add_library(luc_kernel SHARED)

# Source files
file(GLOB_RECURSE KERNEL_SOURCES 
    "kernel/src/*.cpp"
    "kernel/src/*.hpp"
)
target_sources(luc_kernel PRIVATE ${KERNEL_SOURCES})

# Include directories
target_include_directories(luc_kernel PUBLIC 
    kernel/include
    kernel/src
)

# Export macro for Windows
if(WIN32)
    target_compile_definitions(luc_kernel PRIVATE LGE_EXPORT_DLL)
endif()

# --- Dependencies ---

# 1. Vulkan SDK (Required)
find_package(Vulkan REQUIRED)
target_include_directories(luc_kernel SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})

# 2. Header-only libraries (Downloaded by download_externals.ps1)
target_include_directories(luc_kernel SYSTEM PRIVATE 
    externals/glm
    externals/vma
    externals/miniaudio
    externals/cgltf
    externals/nlohmann
)

# 3. GLFW (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/glfw/CMakeLists.txt")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(externals/glfw)
    target_link_libraries(luc_kernel PRIVATE glfw)
else()
    message(WARNING "GLFW submodule not found. Run: git submodule update --init")
endif()

# 4. Jolt Physics (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/jolt/CMakeLists.txt")
    add_subdirectory(externals/jolt)
    target_link_libraries(luc_kernel PRIVATE Jolt)
else()
    message(WARNING "Jolt submodule not found. Run: git submodule update --init")
endif()

# 5. ImGui (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/imgui/CMakeLists.txt")
    add_subdirectory(externals/imgui)
    target_link_libraries(luc_kernel PRIVATE imgui)
else()
    message(WARNING "ImGui submodule not found. Run: git submodule update --init")
endif()

# 6. RmlUI (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/rmlui/CMakeLists.txt")
    # Try to find Freetype, but don't fail
    find_package(Freetype QUIET)
    
    if(NOT Freetype_FOUND)
        message(WARNING "Freetype not found. RmlUI will use internal font engine.")
        set(RMLUI_FONT_ENGINE "internal" CACHE STRING "" FORCE)
    else()
        set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
        target_link_libraries(luc_kernel PRIVATE ${FREETYPE_LIBRARIES})
    endif()
    
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    set(RMLUI_DYNAMIC_LINKING OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(externals/rmlui)
    
    set(RMLUI_BACKEND "Win32_VK")
    add_subdirectory(externals/rmlui/Backends)
    
    target_link_libraries(luc_kernel PRIVATE
        RmlUi::Core
        RmlUi::Debugger
        rmlui_backend_Win32_VK
        Vulkan::Vulkan
    )
    
    target_include_directories(luc_kernel SYSTEM PRIVATE
        externals/rmlui/Include
        externals/rmlui/Backends
    )
    
    if(WIN32)
        target_link_libraries(luc_kernel PRIVATE imm32)
    endif()
else()
    message(WARNING "RmlUI submodule not found. Run: git submodule update --init")
endif()

# 7. Lucid Compiler (Submodule)
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/lucid_compiler/CMakeLists.txt")
    add_subdirectory(externals/lucid_compiler)
else()
    message(WARNING "Lucid Compiler submodule not found. Run: git submodule update --init")
endif()

# 8. Platform-specific libraries
if(WIN32)
    target_link_libraries(luc_kernel PRIVATE
        ws2_32
        winmm
        user32
        shell32
        advapi32
        opengl32
    )
elseif(UNIX)
    target_link_libraries(luc_kernel PRIVATE
        dl
        pthread
    )
endif()

# --- Kernel Validator (Test tool) ---
add_executable(KernelValidator)
target_sources(KernelValidator PRIVATE tools/bootstrap_main.cpp)
target_link_libraries(KernelValidator PRIVATE luc_kernel)

# --- Testing ---
enable_testing()
# add_subdirectory(tests/kernel)