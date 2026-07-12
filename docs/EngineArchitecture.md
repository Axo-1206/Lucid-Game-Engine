# Engine Architecture

This document describes how Lucid Game Engine is organized on disk, how responsibility is split between the C++ kernel and the Lucid-authored layers above it, and what a project looks like as it moves from source repository → downloadable SDK → shipped game.

Some entries below correct and extend the original tree to match decisions locked in `MasterPlan.md` and the implementation detail in `docs/kernel/kernel_implementation.md` (notably the network subsystem, the arena allocator, and several kernel headers that existed in code samples but were missing from this listing).

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

| Folder (`kernel/src/`) | Public header | Responsibility |
|:---|:---|:---|
| `core/` | `lge_api.h`, `lge_console.h` | Boot sequence (`LGE_Boot`), frame clock, hot-reload file watching, console command dispatch |
| `ecs/` | `lge_ecs.h` | Entity registry, archetype/component storage, multithreaded system scheduling |
| `render/` | `lge_render.h` | Vulkan RHI — device, swapchain, pipelines, SDF font rendering |
| `physics/` | `lge_physics.h` | Jolt integration, contact callbacks, debug wireframe rendering |
| `input/` | `lge_input.h` | GLFW polling, mapped to the swappable `IInputProvider` interface |
| `network/` | `lge_network.h` | Core TCP/UDP `INetworkProvider` implementations; extensions can register their own (e.g. Steam, WebSockets) |
| `vfs/` | `lge_vfs.h` | Encrypted virtual file system — identical API for loose dev files and packed `.pck` bundles |
| `security/` | `lge_security.h` | Offline Ed25519 license verification, PBKDF2/HKDF key derivation, extension signature checks |
| `platform/` | `lge_platform.h` | The only place `#ifdef _WIN32` is allowed to live — `LoadLibrary`/`dlopen` abstraction |
| `ui/` | — (internal) | ImGui editor-shell panels and the RmlUI in-game HUD backend |
| `audio/` *(planned)* | `lge_audio.h` | Miniaudio-backed SFX and streaming mixer |

`luc_runtime/` sits beside `kernel/` rather than inside it: it owns the Lucid language's arena allocator (`ArenaDescriptor`), which the kernel *consumes* (via `lge_arena.h`) but does not own the lifetime of. Keeping it separate reflects that boundary — Lucid creates and frees arenas; C++ subsystems only borrow pointers into them for the duration of a call.

---

## The User Code Layer (Lucid)

Everything above the kernel's ABI line is written in Lucid, not C++, and is loaded at runtime as compiled `.lmod` modules or platform `.dll`/`.so` files rather than linked at build time.

| Folder | What lives here | Loaded as |
|:---|:---|:---|
| `core_lib/` | Standard library — `io.luc` (logging/I/O), `math.luc` (vectors, matrices) | Compiled alongside every project |
| `engine/src/` | The IDE itself: `main.luc` entry point, `editor/` panels (scene view, inspector, console manager), `ui/workspace_manager.luc` (docking/layout), `visual_programming/node_graph.luc` (the node-graph scripting canvas) | `.lmod`, hot-reloaded by `script_manager.cpp` on save |
| *(a developer's own project)* | Game logic and content — not part of this repository at all. A game developer builds against the compiled SDK (below), not against this source tree | AOT `.lmod` for shipping, JIT for in-editor play |
| *extensions* | Third-party or first-party plugins declaring an `extension.json` manifest with a permissions list (`network.client`, `filesystem.global`, etc.) | `.lmod`/`.dll`, sandboxed — the kernel checks permissions before honoring FFI calls like `LGE_Network_Listen` |

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