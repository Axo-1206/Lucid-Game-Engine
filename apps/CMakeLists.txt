# Every executable target lives under this folder.
#
# Libraries live at the repo root, one folder per library, each with
# its own CMakeLists. Only libraries that exist solely to serve an
# executable live here (currently: the editor library).
#
# The gating flags (LUCID_BUILD_EDITOR, LUCID_BUILD_GAME) are declared
# in the root CMakeLists and are visible in every subdirectory. A flag
# gates an *application*, not a folder: LUCID_BUILD_EDITOR gates both
# the editor library and the editor executable, because neither has
# meaning without the other.

if(LUCID_BUILD_GAME)
    add_subdirectory(game)
endif()

if(LUCID_BUILD_EDITOR)
    add_subdirectory(editor)        # defines luc_editor (library)
    add_subdirectory(editor_main)   # defines the editor executable
endif()