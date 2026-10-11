# Lucid Game Engine — File Structure

This describes the repository layout after the engine rework. For the
decisions behind this layout, see `docs/Decision.md`. For the scene
format, see `docs/notes/SceneFormat.md`. For how graphs express control
flow, see `docs/notes/TriggerModel.md`.

## Top level

```
Lucid-Game-Engine/
├── assets/              test and demo assets
├── docs/                engine design documents
├── ecs/                 the engine's table store (engine::ecs)
├── engine/              the engine: core, platform, render, scene, systems, nodes
├── apps/                executables: game, editor, editor_main
├── externals/           vendored dependencies (GLAD, stb, nlohmann/json)
├── tests/               unit and integration tests
├── other/               scratch notes (not current documentation)
├── .gitignore
├── CMakeLists.txt       root build
├── LICENSE
└── README.md
```

`externals/` holds vendored libraries. GLFW and GLM are fetched by CMake
at configure time; the Lucid format library is also fetched by CMake and
builds as a subproject. Their source lands under `build/_deps/` after the
first configure; they are not modified by the engine.

---

## ecs/

The engine's table store. `engine::ecs::World` is the live state:
every table of rows the engine reads and writes at runtime. This is
not the format library's `Registry`; see `README.md`'s Vocabulary
section for the distinction.

```
ecs/
├── include/engine/ecs/
│   ├── types.h          RowIndex, Generation, RowId, ColumnType
│   ├── row_ref.h        RowRef
│   ├── callback.h       CallbackHandle
│   ├── cell_value.h     CellValue
│   ├── column.h         Column
│   ├── table.h          Table
│   └── world.h          World
├── src/
│   ├── types.cpp
│   ├── cell_value.cpp
│   ├── column.cpp
│   ├── table.cpp
│   └── world.cpp
└── CMakeLists.txt       target: lucid_ecs, alias lucid::ecs
```

---

## engine/

The engine itself, split by responsibility.

```
engine/
├── include/engine/
│   ├── core/
│   │   ├── clock.h
│   │   ├── linear_allocator.h
│   │   ├── log.h
│   │   └── system_context.h
│   ├── platform/
│   │   ├── input.h
│   │   └── window.h
│   ├── render/
│   │   ├── camera.h
│   │   ├── camera2d.h
│   │   ├── render_list.h
│   │   ├── render_types.h
│   │   ├── renderer.h
│   │   └── texture.h
│   ├── scene/
│   │   ├── component_tags.h
│   │   ├── entity.h
│   │   ├── scene.h
│   │   └── scene_schema.h
│   ├── systems/
│   │   └── systems.h
│   └── nodes/
│       ├── node_registry.h
│       └── node_runtime.h
├── src/
│   ├── core/
│   │   ├── clock.cpp
│   │   ├── linear_allocator.cpp
│   │   └── log.cpp
│   ├── platform/
│   │   ├── input_glfw.h      (private header)
│   │   ├── input_glfw.cpp
│   │   ├── input_state.cpp
│   │   └── window_glfw.cpp
│   ├── render/
│   │   ├── backend_gl/
│   │   │   ├── renderer_gl.h
│   │   │   ├── renderer_gl.cpp
│   │   │   ├── shader_gl.h
│   │   │   ├── shader_gl.cpp
│   │   │   ├── sprite_batch_gl.h
│   │   │   └── sprite_batch_gl.cpp
│   │   └── render_list.cpp
│   ├── scene/
│   │   ├── entity.cpp
│   │   ├── scene.cpp
│   │   ├── scene_loader.cpp
│   │   └── scene_saver.cpp
│   ├── systems/
│   │   ├── hierarchy_system.cpp
│   │   ├── movement_system.cpp
│   │   └── render_system.cpp
│   └── nodes/
│       └── node_registry.cpp
└── CMakeLists.txt       target: luc_engine, alias lucid::engine
```

The `core/` subdirectory holds non-render, non-platform utilities.
The `platform/` subdirectory holds OS-facing code (window, input); its
private GLFW headers are `src/`-local and not exposed. The `render/`
subdirectory contains the renderer interface and the OpenGL backend
under `backend_gl/`. The `scene/` subdirectory owns the `Scene` type,
its loader and saver, and the entity helpers. The `nodes/` subdirectory
owns the node registry — the boundary between the engine and the Lucid
format library.

---

## apps/

Every executable target and the libraries that exist only to serve one.

```
apps/
├── game/
│   ├── CMakeLists.txt
│   └── main.cpp
├── editor/
│   ├── include/editor/
│   │   └── editor_app.h
│   ├── src/
│   │   └── editor_app.cpp
│   └── CMakeLists.txt       target: luc_editor
├── editor_main/
│   ├── CMakeLists.txt
│   └── main.cpp
└── CMakeLists.txt           gates subdirectories on LUCID_BUILD_*
```

The game executable is built from `apps/game/`. The editor is split into
a library (`luc_editor`) in `apps/editor/` and its entry point in
`apps/editor_main/`. This split lets the editor library be linked into
test targets without dragging in `WinMain`.

Executables land in `build/bin/`: `lucid_game.exe`, `lucid_editor.exe`.

---

## tests/

```
tests/
├── ecs/
│   └── test_world.cpp
├── engine/
│   ├── test_clock.cpp
│   ├── test_entity.cpp
│   ├── test_input.cpp
│   ├── test_linear_allocator.cpp
│   ├── test_log.cpp
│   ├── test_scene.cpp
│   └── test_systems.cpp
├── nodes/
│   └── test_node_registry.cpp
└── CMakeLists.txt
```

All tests build into a single `lucid_tests` executable (Catch2). Run with:

```
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

---

## externals/

Vendored dependencies that are checked in directly.

```
externals/
├── include/
│   ├── glad/
│   │   └── glad.h
│   └── KHR/
│       └── khrplatform.h
├── nlohmann/
│   └── json.hpp
├── src/
│   └── glad.c
├── stb/
│   └── stb_image.h
└── CMakeLists.txt
```

GLFW, GLM, and the Lucid format library are fetched by CMake and are
not in this directory.

---

## assets/

```
assets/
├── graphs/              (empty — .lucgraph files go here at compile time)
├── scenes/
│   └── demo.lscene
└── .gitkeep
```

`.lscene` files are JSON scene descriptions (see `docs/notes/SceneFormat.md`).
`.lucgraph` files are compiled behavior graphs produced by the Lucid
format library's `lucid` CLI; they are not checked in.

---

## docs/

```
docs/
├── README.md            index of this folder
├── Decision.md          design decisions, one per phase
├── FileStructure.md     this file
├── notes/
│   ├── TriggerModel.md  how a Lucid graph expresses control flow
│   └── SceneFormat.md   the .lscene format spec and evolution rules
├── implementationPlans/
│   └── kernelImplementationPlan.md
└── archive/             older design documents, kept for history
    ├── designs/
    │   └── ...
    ├── kernel/
    │   └── ...
    ├── EngineArchitecture.md
    ├── MasterPlan.md
    └── NOTE.md
```

Documents in `archive/` describe earlier designs and are not current.
`LucidGraphFormat.md` at the docs root describes the Lucid graph text
format; see also the format library's own `Architecture.md` and
`Grammar.md` under `build/_deps/lucid_format-src/docs/` after configure.

---

## The format library

The Lucid format library is a separate project, fetched by CMake and
built as a subproject. Its source lives under `build/_deps/lucid_format-src/`
after the first configure. The engine does not modify it.

The engine consumes the format library through four headers:

- `sema/Registry.hpp` — the node vocabulary struct the engine fills in
- `sema/Graph.hpp` — the compiled graph structure the runtime walks
- `sema/Sema.hpp` — the `compile()` function
- `serialization/Serializer.hpp` / `Deserializer.hpp` — binary I/O

The format library's own documentation (`Architecture.md`, `Grammar.md`,
`notes/RegistryContractNote.md`) lives under its source root.
