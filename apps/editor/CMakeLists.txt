# The editor library: ImGui panels, project browser, editor app.
# This library is ONLY linked by the editor executable. It is never
# linked by the game executable.

add_library(luc_editor STATIC
    # ImGui core
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp

    # ImGui stdlib helpers
    third_party/imgui/misc/cpp/imgui_stdlib.cpp

    # ImGui backends
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp

    # Editor sources
    src/editor_app.cpp
    src/imgui_layer.cpp
    src/project.cpp
    src/platform_paths.cpp
    src/settings.cpp
    src/project_roots.cpp
    src/document.cpp
    src/panels/script_editor_panel.cpp
    src/diagnostics.cpp
    src/build.cpp
    src/run.cpp
    src/runtime_diagnostics.cpp
    src/symbol_index.cpp

    # Editor Panels
    src/panels/launch_panel.cpp
    src/panels/main_shell.cpp
    src/panels/console_panel.cpp
    src/panels/explorer_panel.cpp
    src/panels/problems_panel.cpp
    src/panels/package_panel.cpp
    src/panels/diagnostics_panel.cpp
    src/panels/file_finder.cpp
    src/panels/symbol_finder.cpp
    src/find_in_files.cpp
    src/panels/find_in_files_panel.cpp
)

target_include_directories(luc_editor
    PUBLIC
        include
        third_party/imgui
        third_party/imgui/backends
        third_party/imgui/misc/cpp
)

target_link_libraries(luc_editor
    PUBLIC
        luc_engine
        lucid::glfw
        lucid::glad
)

# Use GLAD instead of ImGui's embedded OpenGL loader.
target_compile_definitions(luc_editor
    PRIVATE
        IMGUI_IMPL_OPENGL_LOADER_GLAD
)