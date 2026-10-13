add_library(lucid_ecs STATIC
    src/types.cpp
    src/cell_value.cpp
    src/column.cpp
    src/table.cpp
    src/world.cpp
    src/core/entity.cpp
)

target_include_directories(lucid_ecs
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(lucid_ecs PUBLIC cxx_std_17)

add_library(lucid::ecs ALIAS lucid_ecs)