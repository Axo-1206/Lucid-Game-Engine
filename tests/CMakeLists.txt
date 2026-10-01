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
)

target_link_libraries(lucid_tests
	PRIVATE
		lucid::runtime
		Catch2::Catch2WithMain
)

add_test(NAME lucid_tests COMMAND lucid_tests)
