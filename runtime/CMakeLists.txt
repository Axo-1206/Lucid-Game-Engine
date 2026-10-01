# The Lucid runtime: table storage, references, values.
# For Phase 0, it's an empty placeholder. Step 1 fills it in.

add_library(luc_runtime STATIC
    src/version.cpp
)

target_include_directories(luc_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(luc_runtime PUBLIC cxx_std_17)

add_library(lucid::runtime ALIAS luc_runtime)
