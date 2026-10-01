add_library(luc_runtime STATIC
    src/types.cpp
    src/cellvalue.cpp
    src/row_ref.cpp
    src/function.cpp
    src/column.cpp
    src/table.cpp
    src/runtime.cpp
    src/version.cpp
)

target_include_directories(luc_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(luc_runtime PUBLIC cxx_std_17)

add_library(lucid::runtime ALIAS luc_runtime)
