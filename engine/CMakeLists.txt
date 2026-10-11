add_library(luc_engine STATIC
    src/core/log.cpp
    src/core/clock.cpp
    src/core/linear_allocator.cpp
    src/core/diagnostic_log.cpp

    src/platform/input_state.cpp
    src/platform/input_glfw.cpp
    src/platform/window_glfw.cpp

    src/render/backend_gl/shader_gl.cpp
    src/render/backend_gl/sprite_batch_gl.cpp
    src/render/backend_gl/renderer_gl.cpp
    src/render/render_list.cpp

    src/scene/scene.cpp
    src/scene/scene_loader.cpp
    src/scene/scene_saver.cpp
    src/scene/entity.cpp

    src/systems/hierarchy_system.cpp
    src/systems/movement_system.cpp
    src/systems/render_system.cpp

    src/nodes/node_registry.cpp

    src/graph/node_dispatch.cpp
    src/graph/graph_runtime.cpp
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
        lucid::ecs
        lucid_sema
        lucid_serialization
    PRIVATE
        lucid::glfw
        lucid::glm
        lucid::glad
        lucid::stb
        lucid::nlohmann
        lucid_serialization
)

target_compile_features(luc_engine PUBLIC cxx_std_17)

add_library(lucid::engine ALIAS luc_engine)
