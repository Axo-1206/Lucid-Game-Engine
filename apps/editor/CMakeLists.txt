# The editor library: ImGui panels, project browser, editor app.
# This library is ONLY linked by the editor executable. It is never
# linked by the game executable.

add_library(luc_editor STATIC
    src/editor_app.cpp
)

target_include_directories(luc_editor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(luc_editor
    PUBLIC
        lucid::engine
)

target_compile_features(luc_editor PUBLIC cxx_std_17)

add_library(lucid::editor ALIAS luc_editor)
