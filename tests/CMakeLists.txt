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
    runtime/test_row_ref.cpp
    runtime/test_cellvalue.cpp
    runtime/test_column.cpp
    runtime/test_table.cpp
    runtime/test_runtime.cpp
    engine/test_log.cpp
    engine/test_clock.cpp
    engine/test_linear_allocator.cpp
    engine/test_input.cpp
    engine/test_entity.cpp
    engine/test_scene.cpp
)

target_link_libraries(lucid_tests
    PRIVATE
        lucid::runtime
        lucid::engine
        Catch2::Catch2WithMain
)

add_test(NAME lucid_tests COMMAND lucid_tests)
