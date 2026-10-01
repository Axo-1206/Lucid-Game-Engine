# Tests are optional. For Phase 0, this is a placeholder.
# Step 1 will add the first real test (for the runtime tables).

# Uncomment when ready to use Catch2:
#
# include(FetchContent)
# FetchContent_Declare(
#     catch2
#     GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#     GIT_TAG        v3.5.2
#     GIT_SHALLOW    TRUE
# )
# FetchContent_MakeAvailable(catch2)
#
# add_executable(lucid_tests
#     runtime/test_table.cpp
# )
# target_link_libraries(lucid_tests PRIVATE lucid::runtime Catch2::Catch2WithMain)
# add_test(NAME lucid_tests COMMAND lucid_tests)

message(STATUS "Tests: not yet implemented (Phase 1+)")
