add_executable(lucid_editor
    main.cpp
)

target_link_libraries(lucid_editor
    PRIVATE
        lucid::engine
        lucid::editor
)
