include(FetchContent)

set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(catch2)

add_executable(lucid_tests
    engine/test_log.cpp
    engine/test_clock.cpp
    engine/test_linear_allocator.cpp
    engine/test_input.cpp
    engine/test_entity.cpp
    engine/test_scene.cpp
    engine/test_systems.cpp
    engine/test_format_diagnostics.cpp

    ecs/test_world.cpp
    ecs/test_table.cpp
    
    graph/test_graph_runtime.cpp
    
    registry/test_default_registry.cpp
    registry/test_package_manager.cpp
    physics/test_stub_backend.cpp
    physics/test_backend_box2d.cpp
)

target_link_libraries(lucid_tests
    PRIVATE
        lucid::ecs
        lucid::engine
        Catch2::Catch2WithMain
)

add_test(NAME lucid_tests COMMAND lucid_tests)
