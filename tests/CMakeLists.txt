include(FetchContent)

set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(catch2)

# Helper: create a test executable from a folder
function(lucid_add_test_group name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name}
        PRIVATE
            lucid::ecs
            lucid::engine
            luc_editor
            Catch2::Catch2WithMain
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_executable(test_helper_echo editor/helper_echo.cpp)


lucid_add_test_group(lucid_tests_ecs
    ecs/test_world.cpp
    ecs/test_table.cpp
)

lucid_add_test_group(lucid_tests_graph
    graph/test_graph_runtime.cpp
)

lucid_add_test_group(lucid_tests_registry
    registry/test_default_registry.cpp
    registry/test_package_manager.cpp
    registry/test_math_package.cpp
)

lucid_add_test_group(lucid_tests_physics
    physics/test_stub_backend.cpp
    physics/test_backend_box2d.cpp
)

lucid_add_test_group(lucid_tests_engine
    engine/test_log.cpp
    engine/test_clock.cpp
    engine/test_input.cpp
    engine/test_entity.cpp
    engine/test_scene.cpp
    engine/test_systems.cpp
    engine/test_format_diagnostics.cpp
    engine/test_render_types.cpp
    engine/test_view.cpp
    engine/test_ui_system.cpp
    engine/test_operation.cpp
    engine/test_diagnostics.cpp
)

lucid_add_test_group(lucid_tests_editor
    editor/test_project.cpp
    editor/test_platform_paths.cpp
    editor/test_settings.cpp
    editor/test_editor_app.cpp
    editor/test_console_panel.cpp
    editor/test_project_roots.cpp
    editor/test_explorer_panel.cpp
    editor/test_document.cpp
    editor/test_diagnostics.cpp
    editor/test_build.cpp
    editor/test_run.cpp
    editor/test_reload_packages.cpp
    editor/test_diagnostics_panel.cpp
    editor/test_file_finder.cpp
    editor/test_symbol_index.cpp
    editor/test_find_in_files.cpp
)