# Lucid Engine

A game engine with an embedded data-oriented scripting language.

## Status

Phase 0 — project skeleton. Compiles, opens a window, closes on Escape.

## Building

### Prerequisites

- CMake 3.20+
- A C++17 compiler (MSVC 2019+, Clang 12+, GCC 10+)
- Git (for submodules)

### First-time setup

    git submodule update --init --recursive

### Build

    cmake -S . -B build
    cmake --build build

### Run

    ./build/game/lucid_game
    ./build/editor/lucid_editor

Press Escape to close the window.

### Build options

- `-DLUCID_BUILD_EDITOR=OFF` — don't build the editor (for shipping)
- `-DLUCID_BUILD_GAME=OFF` — don't build the game
- `-DLUCID_BUILD_TESTS=ON` — build tests

## Structure

- `runtime/` — the Lucid runtime (tables, refs, values)
- `engine/` — the shared engine (renderer, physics, input, systems)
- `editor/` — the editor (ImGui panels, project browser). NOT shipped with games.
- `game/` — the game executable
- `editor_main/` — the editor executable
- `core_lib/` — Lucid core library (.luc files)
- `assets/` — test assets

## License

TBD