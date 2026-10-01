# The shared engine: renderer, physics, input, systems.
# Both the game and the editor link this.

add_library(luc_engine STATIC
    src/core/log.cpp
    src/core/clock.cpp
    src/platform/window_glfw.cpp
)

target_include_directories(luc_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(luc_engine
    PUBLIC
        lucid::runtime
    PRIVATE
        lucid::glfw
        lucid::glm
)

target_compile_features(luc_engine PUBLIC cxx_std_17)

add_library(lucid::engine ALIAS luc_engine)
