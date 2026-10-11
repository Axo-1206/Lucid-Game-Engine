add_executable(lucid_game
    main.cpp
)

# IMPORTANT: The game links luc_engine ONLY.
# It does NOT link luc_editor. It has no access to editor headers.
target_link_libraries(lucid_game
    PRIVATE
        lucid::engine
)
