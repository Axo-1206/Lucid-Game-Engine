add_library(luc_engine STATIC
    src/core/log.cpp
    src/core/clock.cpp
    src/core/linear_allocator.cpp
    src/ecs/entity.cpp
    src/input/input_state.cpp
    src/input/input_glfw.cpp
    src/render/backend_gl/shader_gl.cpp
    src/render/backend_gl/sprite_batch_gl.cpp
    src/render/backend_gl/renderer_gl.cpp
    src/scene/scene.cpp
    src/scene/scene_loader.cpp
    src/scene/scene_saver.cpp
    src/platform/window_glfw.cpp
)

target_include_directories(luc_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(luc_engine
    PUBLIC
        lucid::runtime
    PRIVATE
        lucid::glfw
        lucid::glm
        lucid::glad
        lucid::stb
        lucid::nlohmann
)

target_compile_features(luc_engine PUBLIC cxx_std_17)

add_library(lucid::engine ALIAS luc_engine)
