# Engine Architecture

This document describes how Lucid Game Engine is organized on disk, how responsibility is split between the C++ kernel/engine layer and the Lucid scripting layer above it, and what a project looks like as it moves from source repository → downloadable SDK → shipped game.

Some entries correct and extend the original tree to match decisions locked in `MasterPlan.md` and the implementation detail in `docs/kernel/kernel_implementation.md`.

---

## Two-Layer Architecture

The engine is built on a strict two-layer model. The boundary between them is the kernel's C ABI — the flat function table exported by `LGE_GetAPI()`. Nothing crosses this boundary in the wrong direction.

```
┌─────────────────────────────────────────────────────────────────┐
│                   LAYER 2 — User / Scripting                    │
│                                                                 │
│   User game logic written in Lucid (.luc files)                 │
│   Imports from bindings/ to access engine features              │
│   Never touches kernel headers or C++ directly                  │
│                                                                 │
│   bindings/          ← Lucid wrappers over lge_*.h headers      │
│   core_lib/          ← Standard library (io, math, array...)    │
│   (user project)/    ← Developer's own game logic               │
├─────────────────────────────────────────────────────────────────┤
│              C ABI boundary  —  LGE_GetAPI()                    │
│              lge_*.h headers  —  POD structs, C types only      │
├─────────────────────────────────────────────────────────────────┤
│                   LAYER 1 — Internal Engine (C++)               │
│                                                                 │
│   kernel/    ← Microkernel: rendering, physics, ECS, audio,     │
│               VFS, input, network, security, platform           │
│               Talks to third-party libs directly in C++         │
│                                                                 │
│   engine/    ← Editor shell: ImGui panels, docking, toolbar,    │
│               scene view, inspector, asset browser              │
│               Written in C++, uses ImGui and kernel APIs        │
│                                                                 │
│   externals/ ← Jolt, GLFW, ImGui, RmlUI, VMA, etc.              │
└─────────────────────────────────────────────────────────────────┘
```

### Layer 1 — Internal Engine (C++)

The kernel and the editor shell are written entirely in C++. They talk to third-party libraries (Jolt, GLFW, Vulkan, ImGui, RmlUI, miniaudio) directly via their C++ APIs. No Lucid code exists at this layer. The kernel exposes its functionality upward through `lge_*.h` headers — C types only, no C++ classes at the boundary.

The editor shell (`engine/`) is built on ImGui. Every panel you see — the scene hierarchy, the inspector, the console, the asset browser — is a C++ ImGui panel calling the kernel's C API. This is the same decision Godot made: the editor is engine code, not user code.

### Layer 2 — User / Scripting (Lucid)

Game logic, extensions, and any user-authored behavior are written in Lucid. Users never write C++ and never import `lge_*.h` directly. Instead they import from `bindings/` — a set of `.luc` files that wrap each kernel header in idiomatic Lucid APIs. The bindings are what ships in the SDK alongside `core_lib/`.

```lucid
-- A user's game script imports from bindings, not from kernel headers
import "bindings/physics"
import "bindings/ecs"
import "core_lib/math"

const on_update (dt float) = {
    let hit Result<RaycastHit, bool> = physics:raycast(origin, direction, 100.0)
    if hit.flag {
        ecs:destroy_entity(hit.value.entity)
    }
}
```

The bindings layer is analogous to Godot's GDScript built-in class bindings, Unity's `UnityEngine.dll`, or Unreal's Blueprint function library — a curated, ergonomic API that sits between raw engine internals and user code.

---

## File Structure

```
Lucid-Game-Engine/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── .gitignore
├── .gitmodules
│
├── bindings/                         -- Lucid wrappers over kernel C headers (ships in SDK)
│   │                                 -- Users import these instead of touching lge_*.h directly
│   ├── ecs.luc                     -- Entity/component API: create_entity, add_component, for_each
│   ├── physics.luc                 -- Physics API: create_body, raycast, set_velocity, on_contact
│   ├── render.luc                  -- Render API: upload_texture, draw_mesh, get_render_size
│   ├── audio.luc                   -- Audio API: load_sfx, play, set_volume, set_position
│   ├── input.luc                   -- Input API: is_key_held, mouse_pos, gamepad_axis
│   ├── vfs.luc                     -- VFS API: open, read, write, list_dir
│   ├── network.luc                 -- Network API: connect, listen, send, poll_event
│   ├── console.luc                 -- Console API: log, warn, error, register command
│   └── ui.luc                      -- In-game UI API: Lucid bridge over RmlUI contexts
│
├── core_lib/                         -- Lucid standard library (ships in SDK, not engine-specific)
│   ├── io.luc                      -- I/O: print, read_line, file helpers
│   ├── math.luc                    -- Vec2/3/4, Mat4, Quat, lerp, clamp, trig
│   ├── array.luc                   -- map, filter, reduce, sort, zip
│   └── string.luc                  -- split, trim, find, format
│
├── docs/                             -- Engine documentation and specifications
│   ├── MasterPlan.md                 -- Engine roadmap and design decisions
│   ├── EngineArchitecture.md         -- This document
│   ├── LUCID_GRAMMAR.md              -- The Lucid language specification
│   │
│   ├── designs/                      -- Detailed design reference papers
│   │   ├── AssetPipeline.md
│   │   ├── BuildSystem.md
│   │   ├── ControlPanelDesign.md
│   │   ├── EcsSerialization.md
│   │   ├── ExecutionPlan.md
│   │   ├── LucidEngineOverview.md
│   │   ├── LucidEngineSecurity.md
│   │   ├── LucidFileFormats.md
│   │   ├── LucidVersionControll.md
│   │   └── SecurityKeyGuide.md
│   │
│   └── kernel/
│       ├── kernel_implementation.md  -- Microkernel implementation details
│       └── kernel_api_reference.md  -- Full lge_*.h C API reference
│
engine/                           -- Editor shell (Lucid-based)
│   │                             -- Written entirely in Lucid
│   │                             -- Calls kernel C API via bindings/
│   └── src/
│       ├── main.luc              -- Editor entry point
│       │
│       ├── shell/                -- Top-level editor chrome
│       │   ├── activity_bar.luc  -- Left sidebar icon rail
│       │   ├── status_bar.luc    -- Bottom status line (play state, frame time)
│       │   └── tab_manager.luc   -- Dockable tab container management
│       │
│       ├── editor/               -- Editor tool panels (all Lucid UI)
│       │   ├── scene_view.luc    -- 3D viewport: camera control, gizmos, entity picking
│       │   ├── inspector_panel.luc -- Property inspector: reflects ECS components
│       │   ├── hierarchy_panel.luc -- Scene entity tree
│       │   ├── asset_browser.luc -- VFS-backed file/asset browser
│       │   ├── console_panel.luc -- Live console output + command input
│       │   └── profiler_panel.luc -- Frame time, draw calls, memory stats
│       │
│       ├── ui/                   -- Editor UI infrastructure
│       │   ├── workspace_manager.luc -- Docking layout save/load
│       │   ├── theme.luc         -- UI style tokens (colors, rounding, fonts)
│       │   └── widgets.luc       -- Shared custom UI widgets (drag-vec3, color picker, etc.)
│       │
│       └── visual_programming/   -- Visual scripting canvas
│           └── node_graph.luc    -- Node-based visual scripting logic
│
├── externals/                        -- 3rd-party dependencies and submodules
│   ├── cgltf/                        -- Single-header GLTF loader
│   ├── glfw/                         -- Windowing and input
│   ├── glm/                          -- Math library for graphics matrices
│   ├── imgui/                        -- Immediate-mode GUI (editor shell only)
│   ├── jolt/                         -- 3D physics simulation
│   ├── lucidlang/                    -- Lucid language processor (lexer, parser, JIT/AOT)
│   ├── miniaudio/                    -- Single-header audio mixer
│   ├── nlohmann/                     -- JSON parser
│   ├── rmlui/                        -- Retained HTML/CSS engine for in-game UI
│   └── vma/                          -- Vulkan memory allocator
│
├── kernel/                           -- C++ microkernel (the engine bedrock)
│   ├── include/                      -- Public C-ABI headers (no C++ types)
│   │   ├── lge_api.h                 -- Stable function table: LGE_GetAPI()
│   │   ├── lge_arena.h               -- LGE_ArenaDescriptor + LGE_Arena_Contains
│   │   ├── lge_audio.h               -- SFX/stream audio API
│   │   ├── lge_console.h             -- Console command dispatch
│   │   ├── lge_ecs.h                 -- Entity/component/archetype API
│   │   ├── lge_input.h               -- Input polling and callbacks
│   │   ├── lge_network.h             -- Network provider interface
│   │   ├── lge_physics.h             -- Jolt physics bridge API
│   │   ├── lge_platform.h            -- OS abstraction (internal only)
│   │   ├── lge_render.h              -- Vulkan RHI submission API
│   │   ├── lge_security.h            -- License + signature verification
│   │   └── lge_vfs.h                 -- Encrypted virtual file system API
│   │
│   ├── ffi/                          -- Generated FFI artifacts (do not edit manually)
│   │   └── lge_ffi.lfi               -- Auto-generated: @[foreign("C")] declarations
│   │                                 -- Consumed by Lucid compiler + JIT for symbol resolution
│   │                                 -- Generated from lge_*.h by lge_header_parser tool
│   │
│   └── src/                          -- Microkernel subsystem implementations
│       ├── core/
│       │   ├── checksum.cpp          -- SHA-256 self-integrity checks
│       │   ├── clock.cpp             -- Frame delta-time clocks
│       │   ├── console_interpreter.cpp -- Console command reflection runner
│       │   ├── file_watcher.cpp      -- ReadDirectoryChangesW / inotify watcher
│       │   ├── kernel.cpp            -- LGE_Boot: ordered subsystem initialization
│       │   └── script_manager.cpp    -- Lucid module hot-reload via DLL swap
│       ├── ecs/
│       │   ├── components/           -- POD component structs
│       │   │   ├── audio_source.cpp
│       │   │   ├── physics_body.cpp
│       │   │   ├── sprite_renderer.cpp
│       │   │   └── transform.cpp
│       │   ├── component_store.cpp   -- Archetype sequential array storage
│       │   ├── system_scheduler.cpp  -- Multithreaded system execution
│       │   └── world.cpp             -- Entity lifecycle management
│       ├── input/
│       │   └── input_bridge.cpp      -- GLFW polling → lge_input.h surface
│       ├── network/
│       │   └── network_manager.cpp   -- TCP/UDP providers, INetworkProvider host
│       ├── physics/
│       │   ├── debug_renderer.cpp    -- Jolt debug wireframe visualization
│       │   └── physics_bridge.cpp    -- Jolt integration and contact callbacks
│       ├── platform/
│       │   ├── linux_loader.cpp      -- dlopen/dlsym
│       │   └── win32_loader.cpp      -- LoadLibrary/GetProcAddress
│       ├── render/
│       │   ├── sdf_font.cpp          -- Multi-channel SDF font rendering
│       │   └── vulkan_rhi.cpp        -- Vulkan device, swapchain, pipelines
│       ├── security/
│       │   ├── extension_verifier.cpp -- Ed25519 extension package verification
│       │   ├── key_derivation.cpp    -- PBKDF2/HKDF key derivation
│       │   └── license_verifier.cpp  -- Offline Ed25519 license validation
│       ├── ui/
│       │   ├── imgui_shell.cpp       -- ImGui frame lifecycle (BeginFrame/EndFrame)
│       │   └── rmlui_backend.cpp     -- RmlUI Vulkan backend for in-game UI
│       ├── audio/
│       │   └── audio_system.cpp      -- Miniaudio SFX/stream mixer
│       └── vfs/
│           ├── vfs_packer.cpp        -- Encrypted .pck asset bundler
│           └── vfs_reader.cpp        -- Memory-resident AES-256 decrypter
│
├── luc_runtime/                      -- Lucid language runtime (sibling of kernel/)
│   └── src/
│       └── arena.cpp                 -- #arena_create/alloc/reset/free, bump-pointer cursor
│
├── tools/                            -- Development utility scripts
│   ├── lge_header_parser/            -- Generates lge_ffi.lfi from lge_*.h via libclang
│   │   ├── main.cpp
│   │   └── CMakeLists.txt
│   ├── bootstrap_main.cpp            -- MSBuild boot for LucidEditor
│   ├── download_externals.ps1        -- Submodule initialization
│   └── package_sdk.py                -- Packages binaries + core_lib + bindings → LucidSetup.exe
│
└── tests/
    ├── kernel/                       -- C++ unit/system tests (GoogleTest)
    └── logic/                        -- Lucid integration test scripts
```

---

## The Kernel (C++ Bedrock)

`kernel/` is the only part of the engine written in C++, and it is the only part that touches hardware, memory, or the OS directly. It owns rendering, physics, input, audio, file I/O, and thread scheduling. Nothing above it — not the editor shell, not user scripts, not extensions — is allowed to reach past the kernel's public headers.

### Boundary Rules

- **One-directional ABI.** The kernel exports a single flat C function table via `LGE_GetAPI()`. Lucid modules call into it; the kernel never calls back into Lucid or links against it directly.
- **C types only at the boundary.** Every header under `kernel/include/` uses raw pointers and POD structs — no STL, no C++ classes cross the ABI. This keeps the surface stable across compilers and language runtimes.
- **POD components.** All ECS component structs are plain data with no constructors or virtual methods — `memcpy`-safe and reflection-friendly.
- **Systems never call each other.** `PhysicsSystem` writes to `TransformComponent`; `RenderSystem` reads from it. Communication happens only through shared component data.

### Subsystem Map

| Folder (`kernel/src/`) | Public header                | Responsibility                                                                              |
| :--------------------- | :--------------------------- | :------------------------------------------------------------------------------------------ |
| `core/`                | `lge_api.h`, `lge_console.h` | Boot sequence, frame clock, hot-reload file watching, console command dispatch              |
| `ecs/`                 | `lge_ecs.h`                  | Entity registry, archetype/component storage, multithreaded system scheduling               |
| `render/`              | `lge_render.h`               | Vulkan RHI — device, swapchain, pipelines, SDF font rendering                               |
| `physics/`             | `lge_physics.h`              | Jolt integration, contact callbacks, debug wireframe rendering                              |
| `input/`               | `lge_input.h`                | GLFW polling mapped to swappable `IInputProvider` interface                                 |
| `network/`             | `lge_network.h`              | Core TCP/UDP `INetworkProvider`; extensions register alternatives (Steam, WebSockets)       |
| `vfs/`                 | `lge_vfs.h`                  | Encrypted virtual file system — identical API for loose dev files and packed `.pck` bundles |
| `security/`            | `lge_security.h`             | Offline Ed25519 license verification, PBKDF2/HKDF key derivation                            |
| `platform/`            | `lge_platform.h`             | The only place `#ifdef _WIN32` is allowed — `LoadLibrary`/`dlopen` abstraction              |
| `ui/`                  | — (internal)                 | ImGui frame lifecycle hook; RmlUI in-game HUD Vulkan backend                                |
| `audio/`               | `lge_audio.h`                | Miniaudio-backed SFX and streaming mixer                                                    |

`luc_runtime/` sits beside `kernel/` rather than inside it: it owns the Lucid language's arena allocator (`ArenaDescriptor`), which the kernel consumes (via `lge_arena.h`) but does not own the lifetime of.

---

## The Editor Shell (C++, engine/)

`engine/` is the Lucid IDE — the window you see when you open the engine. It is written entirely in C++ using ImGui. It is **not** a Lucid program; it does not use the scripting layer at all.

Every panel in the editor is an ImGui C++ panel that calls into the kernel's C API:
- **SceneView** — 3D viewport with camera orbit, entity picking via `LGE_Physics_RaycastFromCamera`, and gizmo overlays
- **InspectorPanel** — reads ECS component data via `LGE_GetComponent`, renders editable fields with ImGui widgets, writes changes back via the same API
- **HierarchyPanel** — queries all live entities via `LGE_ForEach`, displays them in a tree
- **AssetBrowser** — lists files via `LGE_VFS_ListDir`, loads previews with `LGE_UploadTexture`
- **ConsolePanel** — displays diagnostics, accepts command input via `LGE_Console_Exec`

The editor shell does not contain game logic. It is a tool that inspects and controls the kernel's state at edit time.

---

## The Bindings Layer (bindings/)

`bindings/` is the bridge between the kernel's raw C API and Lucid user code. Each `.luc` file in this folder wraps one or more `lge_*.h` headers, providing idiomatic Lucid function and type names instead of raw C symbols.

Users import from `bindings/` rather than writing `@[foreign("C")]` declarations themselves. The bindings are what Godot's built-in GDScript class library is, or what Unity's `UnityEngine` namespace is — the engine's public API as seen by the person writing game logic.

### Example — `bindings/physics.luc`

```lucid
-- bindings/physics.luc
-- Lucid-idiomatic wrapper over lge_physics.h
-- Users import this file; they never see the @[foreign("C")] declarations

import "kernel/ffi/lge_ffi.lfi"    -- raw FFI declarations (generated)

-- Re-export with Lucid-idiomatic names and types
const create_body (entity EntityID, def *PhysicsBodyDef) -> BodyID = {
    return LGE_Physics_CreateBody(entity, def)
}

const raycast (origin Vec3, dir Vec3, max_dist float) -> Result<RaycastHit, bool> = {
    let hit RaycastHit = RaycastHit{}
    let found bool = LGE_Physics_Raycast(
        #addrof(origin.x), #addrof(dir.x), max_dist, #addrof(hit)
    )
    return Result<RaycastHit, bool>{ value = hit, flag = found }
}

const add_force (id BodyID, force Vec3) = {
    LGE_Physics_AddForce(id, force.x, force.y, force.z)
}
```

### Example — user game script

```lucid
-- A user's game script — clean, no raw C visible
import "bindings/physics"
import "bindings/ecs"
import "bindings/input"
import "core_lib/math"

const on_update (dt float) = {
    if input:is_key_pressed(KeyCode.Space) {
        let hit Result<RaycastHit, bool> = physics:raycast(
            camera_pos, camera_forward, 100.0
        )
        if hit.flag {
            ecs:destroy_entity(hit.value.entity)
        }
    }
}
```

### Relationship to `lge_ffi.lfi`

`kernel/ffi/lge_ffi.lfi` is the **machine-level** FFI file — a generated file containing raw `@[foreign("C")]` declarations for every symbol in the kernel's C headers. It is consumed by the Lucid JIT and AOT compiler to resolve foreign symbols. It is not user-facing.

`bindings/*.luc` are the **human-facing** layer — hand-authored Lucid files that import from `lge_ffi.lfi` internally and expose clean, documented, type-safe Lucid APIs to game developers.

| File                     | Written by                           | Used by                                     | Purpose                   |
| ------------------------ | ------------------------------------ | ------------------------------------------- | ------------------------- |
| `kernel/include/lge_*.h` | Engine team (C)                      | C++ kernel internally + `lge_header_parser` | Raw C ABI definition      |
| `kernel/ffi/lge_ffi.lfi` | `lge_header_parser` tool (generated) | Lucid compiler + JIT                        | Raw FFI symbol resolution |
| `bindings/*.luc`         | Engine team (Lucid)                  | Game developers                             | User-facing Lucid API     |

---

## Distribution Structures

### B. The Engine SDK (What Game Developers Download)

```text
Lucid-SDK/
├── bin/
│   ├── luc_kernel.dll          ← Pre-compiled C++ bedrock
│   ├── luc_compiler.exe        ← LLVM-based AOT compiler
│   ├── luc_langserver.exe      ← LSP server for IDE autocomplete
│   └── LucidEditor.exe         ← Editor shell bootstrap
├── bindings/                   ← Lucid API wrappers over kernel headers
│   ├── ecs.luc
│   ├── physics.luc
│   ├── render.luc
│   ├── audio.luc
│   ├── input.luc
│   ├── vfs.luc
│   ├── network.luc
│   ├── console.luc
│   └── ui.luc
├── core_lib/                   ← Standard library
│   ├── io.luc
│   ├── math.luc
│   ├── array.luc
│   └── string.luc
└── core_extensions/            ← Official plugins
```

`bindings/` now ships alongside `core_lib/` in the SDK. Game developers have both immediately available after installing. The `lge_ffi.lfi` file does not ship separately — it is bundled inside `luc_kernel.dll`'s companion data or embedded in the compiler. Game developers never need to see it.

### C. The Shipped Game (What Players Download)

```text
MyAwesomeGame/
├── MyAwesomeGame.exe           ← Tiny C++ bootstrap (generated)
├── luc_kernel.dll              ← Copied from Engine SDK
├── content/
│   ├── game.lmod               ← AOT-compiled game logic
│   ├── assets.pck              ← Encrypted VFS bundle
│   └── settings.json
```

No source `.luc` files, no `bindings/`, no `core_lib/`, no compiler — everything is AOT-compiled into `game.lmod` and the runtime `luc_kernel.dll`. The player's download is bounded by the kernel size plus the developer's own `assets.pck`.

> **SDK vs. shipped-game size:** The SDK is dominated by developer tooling — LLVM-bundled compiler, language server, editor executable, and editor assets. None of that ships to a player. The player download carries only `luc_kernel.dll` + `game.lmod` + `assets.pck`.

---

## Development Workflow

Between the source repo and the SDK sits `dev_build/`: CMake writes `luc_kernel.dll` and `luc_compiler.exe` here on every successful C++ build. `core_lib/`, `bindings/`, and `engine/` assets are symlinked in (not copied) so Lucid-only changes appear instantly without a C++ recompile. Tests in `tests/kernel/` (C++ unit tests) and `tests/logic/` (Lucid integration scripts) both run against this workspace before anything reaches `package_sdk.py`.

### The `lge_header_parser` Tool

`tools/lge_header_parser/` is a build-time tool (not shipped) that reads `kernel/include/lge_*.h` via libclang and regenerates `kernel/ffi/lge_ffi.lfi`. It runs automatically as a CMake pre-build step whenever a kernel header is modified. The updated `lge_ffi.lfi` is committed to the repository alongside the header change so both the JIT and AOT compiler always have a current symbol table.


## File Structure

```
Lucid-Game-Engine/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── .gitignore
├── .gitmodules
│
├── core_lib/                         -- Lucid standard core libraries
│   ├── io.luc                        -- Basic I/O wrapper and logging functions
│   └── math.luc                      -- Vector, matrix, and mathematical utilities
│
├── docs/                             -- Engine documentation and specifications
│   ├── MasterPlan.md                 -- Engine roadmap and design decisions
│   ├── NOTE.md                       -- Architecture and UI division guide
│   ├── Architecture.md               -- This document
│   ├── LUCID_GRAMMAR.md              -- The Lucid language specification
│   │
│   ├── designs/                      -- Detailed design reference papers
│   │   ├── AssetPipeline.md          -- Baker and offline cooking pipelines
│   │   ├── BuildSystem.md            -- CMake structure and output layouts
│   │   ├── ControlPanelDesign.md     -- Layout design for IDE bottom dock panels
│   │   ├── EcsSerialization.md       -- POD components reflection specifications
│   │   ├── ExecutionPlan.md          -- Phased engine development roadmap
│   │   ├── LucidEngineOverview.md    -- Overview of architecture concept
│   │   ├── LucidEngineSecurity.md    -- Security, JIT, and AOT compiler pipelines
│   │   ├── LucidFileFormats.md       -- Source, cooked, and distribution format specs
│   │   ├── LucidVersionControll.md   -- Project versioning and update system design
│   │   └── SecurityKeyGuide.md       -- DRM mechanisms and key rotations lifecycle
│   │
│   ├── implementation/               -- Component implementation blueprints
│   │   └── kernelImplementationPlan.md
│   │
│   └── kernel/                       -- Microkernel specifications
│       └── kernel_implementation.md  -- Microkernel implementation details
│
├── engine/                           -- Lucid editor shell implementation
│   └── src/
│       ├── main.luc                  -- Main entry point for the Lucid editor
│       ├── editor/                   -- Editor panels and console management
│       │   ├── console_manager.luc   -- Live memory bridge console integration
│       │   ├── inspector_panel.luc   -- Dynamic properties inspector panel
│       │   └── scene_view.luc        -- 3D viewport context wrapper
│       ├── ui/                       -- Editor workspace UI modules
│       │   └── workspace_manager.luc -- Window layout and docking manager
│       └── visual_programming/        -- Editor visual node graph modules
│           └── node_graph.luc        -- Visual scripting canvas logic
│
├── externals/                        -- 3rd-party dependencies and submodules
│   ├── cgltf/                        -- Single-header GLTF loader library
│   ├── glfw/                         -- Windowing and input context management
│   ├── glm/                          -- Math library for graphic matrices
│   ├── imgui/                        -- Immediate-mode GUI engine for editor shell
│   ├── jolt/                         -- 3D physics simulation backend
│   ├── lucidlang/                    -- Compiler & VM for the Lucid language
│   ├── miniaudio/                    -- Single-header audio mixer library
│   ├── nlohmann/                     -- JSON parser and serializer library
│   ├── rmlui/                        -- Retained HTML/CSS engine for in-game UI
│   └── vma/                          -- Vulkan memory allocator AMD library
│
├── kernel/                           -- C++ microkernel core implementation
│   ├── include/                      -- Public C-ABI header definitions
│   │   ├── lge_api.h                 -- Stable function table exporter definition
│   │   ├── lge_arena.h               -- LGE_ArenaDescriptor + ownership checks
│   │   ├── lge_audio.h               -- Miniaudio SFX/stream mixer APIs
│   │   ├── lge_console.h             -- Console command dispatch interfaces
│   │   ├── lge_ecs.h                 -- Entity/component/archetype interfaces
│   │   ├── lge_input.h               -- Input bridge callbacks mapping
│   │   ├── lge_network.h             -- INetworkProvider / TCP-UDP core interfaces
│   │   ├── lge_physics.h             -- Jolt physics integration APIs
│   │   ├── lge_platform.h            -- OS abstraction (loader, paths) interfaces
│   │   ├── lge_render.h              -- Vulkan RHI renderer submission interfaces
│   │   ├── lge_security.h            -- License + signature verification interfaces
│   │   └── lge_vfs.h                 -- Encrypted Virtual File System interfaces
│   └── src/                          -- Microkernel backend subsystems source
│       ├── core/                     -- Boot sequencing, clocks, and hot-reload
│       │   ├── checksum.cpp          -- SHA-256 self integrity checks
│       │   ├── clock.cpp             -- Subsystem delta-time frame clocks
│       │   ├── console_interpreter.cpp -- Console command reflection runner
│       │   ├── file_watcher.cpp      -- Windows ReadDirectoryChangesW watcher
│       │   ├── kernel.cpp            -- Microkernel LGE_Boot initialization
│       │   └── script_manager.cpp    -- DLL module hot-reload managers
│       ├── ecs/                      -- Archetype Entity-Component-System
│       │   ├── components/           -- Standard engine POD component structs
│       │   │   ├── audio_source.cpp
│       │   │   ├── physics_body.cpp
│       │   │   ├── sprite_renderer.cpp
│       │   │   └── transform.cpp
│       │   ├── component_store.cpp   -- Archetype sequential array allocations
│       │   ├── system_scheduler.cpp  -- Multithreaded systems execution planner
│       │   └── world.cpp             -- ECS entity lifecycle managers
│       ├── input/                    -- Input bridge handlers
│       │   └── input_bridge.cpp      -- GLFW message loop polling mapping
│       ├── network/                  -- Built-in networking core (Decision 25)
│       │   └── network_manager.cpp   -- TCP/UDP providers, INetworkProvider host
│       ├── physics/                  -- Physics simulation bridging
│       │   ├── debug_renderer.cpp    -- Jolt simulation lines visualization
│       │   └── physics_bridge.cpp    -- Jolt simulation integration handlers
│       ├── platform/                 -- Platform OS specifics abstraction
│       │   ├── linux_loader.cpp      -- Linux dlopen/dlsym library loading
│       │   └── win32_loader.cpp      -- Windows LoadLibrary library loading
│       ├── render/                   -- Graphics Vulkan renderer backend
│       │   ├── sdf_font.cpp          -- Multi-channel signed distance fields font
│       │   └── vulkan_rhi.cpp        -- Vulkan device, swapchain and pipelines
│       ├── security/                 -- Security & licensing backend
│       │   ├── extension_verifier.cpp -- Ed25519 signatures checks for packages
│       │   ├── key_derivation.cpp    -- PBKDF2/HKDF licensing key derivation
│       │   └── license_verifier.cpp  -- Offline Ed25519 license validation
│       ├── ui/                       -- UI renderer hooks
│       │   ├── imgui_shell.cpp       -- Editor shell panels (Dear ImGui)
│       │   └── rmlui_backend.cpp     -- In-game HUD/menu rendering (RmlUI)
│       └── vfs/                      -- Virtual File System implementation
│           ├── vfs_packer.cpp        -- Encrypted asset pack bundler CLI
│           └── vfs_reader.cpp        -- Memory-resident decrypter loader
│
├── luc_runtime/                      -- Lucid language runtime (sibling of kernel/)
│   └── src/
│       └── arena.cpp                 -- #arena_create/alloc/reset/free, bump-pointer cursor
│
├── tests/                            -- Automated test suites
│   ├── kernel/                       -- Microkernel unit/system tests
│   └── logic/                        -- Lucid logic test scripts
│
└── tools/                            -- Development utility scripts
    ├── bootstrap_main.cpp            -- MSBuild boots LucidEditor bootstrap
    ├── download_externals.ps1        -- PS submodule initialization scripts
    └── package_sdk.py                -- Gathers binaries + core_lib into LucidSetup.exe
```

> **Note on `dev_build/`:** CMake outputs compiled binaries directly into a `dev_build/` folder at the repo root, with `core_lib/` and `engine/` symlinked in for instant Lucid iteration without recompiling. It's a generated workspace (gitignored), not source, so it isn't listed in the tree above — see [Development Workflow](#development-workflow) below.

---

## The Kernel (C++ Bedrock)

`kernel/` is the only part of the engine written in C++, and it's the only part that touches hardware, memory, or the OS directly. It owns rendering, physics, input, audio, file I/O, and thread scheduling. Nothing above it — not the editor, not extensions, not game logic — is allowed to reach past the kernel's public headers.

### Boundary rules

- **One-directional ABI.** The kernel exports a single flat C function table via `LGE_GetAPI()` (`kernel/include/lge_api.h`). Lucid modules call into it; the kernel never calls back into Lucid or links against it directly.
- **C types only at the boundary.** Every header under `kernel/include/` uses raw pointers and POD structs — no STL, no C++ classes cross the ABI. This keeps the surface stable across compilers and language runtimes.
- **POD components.** All ECS component structs (`transform.cpp`, `physics_body.cpp`, etc.) are plain data — no constructors, no virtual methods — so they can be `memcpy`'d, serialized, and reflected without special-casing.
- **Systems never call each other.** A `PhysicsSystem` writes to `TransformComponent`; `RenderSystem` reads from it. They communicate only through shared component data, never through direct calls.

### Subsystem map

| Folder (`kernel/src/`) | Public header                | Responsibility                                                                                              |
| :--------------------- | :--------------------------- | :---------------------------------------------------------------------------------------------------------- |
| `core/`                | `lge_api.h`, `lge_console.h` | Boot sequence (`LGE_Boot`), frame clock, hot-reload file watching, console command dispatch                 |
| `ecs/`                 | `lge_ecs.h`                  | Entity registry, archetype/component storage, multithreaded system scheduling                               |
| `render/`              | `lge_render.h`               | Vulkan RHI — device, swapchain, pipelines, SDF font rendering                                               |
| `physics/`             | `lge_physics.h`              | Jolt integration, contact callbacks, debug wireframe rendering                                              |
| `input/`               | `lge_input.h`                | GLFW polling, mapped to the swappable `IInputProvider` interface                                            |
| `network/`             | `lge_network.h`              | Core TCP/UDP `INetworkProvider` implementations; extensions can register their own (e.g. Steam, WebSockets) |
| `vfs/`                 | `lge_vfs.h`                  | Encrypted virtual file system — identical API for loose dev files and packed `.pck` bundles                 |
| `security/`            | `lge_security.h`             | Offline Ed25519 license verification, PBKDF2/HKDF key derivation, extension signature checks                |
| `platform/`            | `lge_platform.h`             | The only place `#ifdef _WIN32` is allowed to live — `LoadLibrary`/`dlopen` abstraction                      |
| `ui/`                  | — (internal)                 | ImGui editor-shell panels and the RmlUI in-game HUD backend                                                 |
| `audio/` *(planned)*   | `lge_audio.h`                | Miniaudio-backed SFX and streaming mixer                                                                    |

`luc_runtime/` sits beside `kernel/` rather than inside it: it owns the Lucid language's arena allocator (`ArenaDescriptor`), which the kernel *consumes* (via `lge_arena.h`) but does not own the lifetime of. Keeping it separate reflects that boundary — Lucid creates and frees arenas; C++ subsystems only borrow pointers into them for the duration of a call.

---

## The User Code Layer (Lucid)

Everything above the kernel's ABI line is written in Lucid, not C++, and is loaded at runtime as compiled `.lmod` modules or platform `.dll`/`.so` files rather than linked at build time.

| Folder                        | What lives here                                                                                                                                                                                                       | Loaded as                                                                                                     |
| :---------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------ |
| `core_lib/`                   | Standard library — `io.luc` (logging/I/O), `math.luc` (vectors, matrices)                                                                                                                                             | Compiled alongside every project                                                                              |
| `engine/src/`                 | The IDE itself: `main.luc` entry point, `editor/` panels (scene view, inspector, console manager), `ui/workspace_manager.luc` (docking/layout), `visual_programming/node_graph.luc` (the node-graph scripting canvas) | `.lmod`, hot-reloaded by `script_manager.cpp` on save                                                         |
| *(a developer's own project)* | Game logic and content — not part of this repository at all. A game developer builds against the compiled SDK (below), not against this source tree                                                                   | AOT `.lmod` for shipping, JIT for in-editor play                                                              |
| *extensions*                  | Third-party or first-party plugins declaring an `extension.json` manifest with a permissions list (`network.client`, `filesystem.global`, etc.)                                                                       | `.lmod`/`.dll`, sandboxed — the kernel checks permissions before honoring FFI calls like `LGE_Network_Listen` |

The distinction that matters: **the kernel never imports Lucid, and Lucid never bypasses the kernel's ABI.** The editor you see when you open Lucid-Game-Engine is not special-cased C++ — it's a Lucid program like any other, calling `LGE_GetAPI()` the same way a shipped game's compiled logic does. Delete `engine/` entirely and the kernel still boots, still renders, still runs a game — it just has no IDE skin on top of it.

---

## Distribution Structures

The tree at the top of this document is the **engine source repository** — what you clone and build from. It is not what a game developer or a player ever sees. Two further structures exist downstream, generated by the build/packaging tooling rather than checked into this repo.

### B. The Engine SDK (What Game Developers Download)

When a game developer installs the Lucid Engine to make a game, they receive the pre-compiled binaries and the standard libraries. They do not get the C++ source code.

```text
Lucid-SDK/
├── bin/
│   ├── luc_kernel.dll                  ← The pre-compiled C++ bedrock
│   ├── luc_compiler.exe                ← The LLVM-based compiler
│   ├── luc_langserver.exe              ← For IntelliSense in VS Code/Editor
│   └── LucidEditor.exe                 ← The Bootstrap launcher for the IDE
├── core_lib/                           ← math.luc, io.luc, etc.
└── core_extensions/                    ← Official plugins (UI tools, etc.)
```

Produced by `tools/package_sdk.py`, which gathers the compiled kernel/compiler binaries, `core_lib/`, and official extensions into `LucidSetup.exe`. Installing it also registers the `.luc` file association and adds the compiler to the system `%PATH%`.

> **Note — this is where nearly all of the SDK's download weight lives.** `luc_compiler.exe` (LLVM statically bundled) is the single largest binary in this folder — larger than `luc_kernel.dll` — because it carries LLVM's target codegen, ORC JIT, and optimization passes so a game developer never has to install LLVM separately. `luc_langserver.exe` stays small only if it's deliberately kept LLVM-free (frontend-only, per `docs/kernel/kernel_implementation.md`'s compiler notes). None of this — the compiler, the language server, `LucidEditor.exe`, or the editor's own UI assets and images — travels any further than this folder. See the callout under Structure C below for what that means for the player-facing download.

### C. The Shipped Game (What Players Download)

When a game developer clicks "Export," the compiler packages their `.luc` code into a native `.lmod` and bundles it with the Kernel.

```text
MyAwesomeGame/
├── MyAwesomeGame.exe                   ← Tiny C++ Bootstrap (Generated automatically)
├── luc_kernel.dll                      ← Copied from the Engine SDK
├── content/
│   ├── game.lmod                       ← The Game Logic (Compiled AOT Native Code)
│   ├── assets.pck                      ← Encrypted VFS (Models, Textures, Audio)
│   └── settings.json                   ← Screen resolution, input bindings
```

Nothing here reads source `.luc` files or `core_lib/` directly — everything the player runs is AOT-compiled and, per the security layer, encrypted (`assets.pck`) or Ed25519-signed where applicable.

> **Callout — SDK size and shipped-game size are two very different numbers, and it's worth not conflating them.** The Engine SDK (Structure B) is dominated by developer-only tooling: the LLVM-bundled compiler, the language server, the editor's own executable, and the editor's UI/image assets. **None of that ships to a player.** A shipped game (Structure C) carries only `luc_kernel.dll`, the developer's compiled `game.lmod`, and their packed `assets.pck` — the compiler, the LSP, `LucidEditor.exe`, and the engine's own editor-facing art and interface code are entirely absent. Concretely: if the full SDK download lands somewhere around 200+ MB (compiler + langserver + editor assets included), the player-facing footprint is realistically an order of magnitude smaller before the developer's own game content is even added — bounded mainly by `luc_kernel.dll`'s size plus whatever the developer's own `assets.pck` contains. When sizing either number, be clear about which of the two you're quoting.

### Development Workflow

Between the source repo and the SDK sits `dev_build/`: CMake writes `luc_kernel.dll`/`luc_compiler.exe` here on every successful C++ build, while `core_lib/` and `engine/` are symlinked in (not copied) so Lucid-only changes show up instantly without a recompile. `tests/kernel/` (GoogleTest/Catch2 C++ unit tests for RHI/ECS/VFS) and `tests/logic/` (functional Lucid scripts exercising physics, rendering, and input) both run against this workspace before anything reaches `package_sdk.py`.