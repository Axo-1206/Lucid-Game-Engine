# Lucid Game Engine — Kernel Implementation Plan

> **Status:** Living document — sections are expanded as each system is designed and implemented.  
> **Companion docs:** `MasterPlan.md` (decisions + rationale), `BuildSystem.md` (CMake layout), `AssetPipeline.md` (asset formats).

---

## Table of Contents

1. [Overview](#1-overview)
   - [Keywords & Glossary](#keywords--glossary)
2. [Boot Sequence](#2-boot-sequence)
3. [ECS (Entity-Component-System)](#3-ecs-entity-component-system)
4. [Vulkan RHI (Render Hardware Interface)](#4-vulkan-rhi-render-hardware-interface)
5. [Physics Bridge (Jolt)](#5-physics-bridge-jolt)
6. [Input Bridge (GLFW)](#6-input-bridge-glfw)
7. [UI Backend (ImGui Shell + RmlUI In-Game)](#7-ui-backend-imgui-shell--rmlui-in-game)
8. [Audio Pipeline](#8-audio-pipeline)
9. [Virtual File System (VFS)](#9-virtual-file-system-vfs)
10. [Script Manager & Hot-Reload](#10-script-manager--hot-reload)
11. [Extension System & FFI](#11-extension-system--ffi)
12. [Console & Command Pipeline](#12-console--command-pipeline)
13. [Security Layer](#13-security-layer)
14. [Platform Layer](#14-platform-layer)
15. [Lucid Execution Model (Interpreter → Compiler)](#15-lucid-execution-model-interpreter--compiler)
16. [FFI Foreign Symbol Resolution](#16-ffi-foreign-symbol-resolution)
17. [Arena Memory & the Shared `ArenaDescriptor`](#17-arena-memory--the-shared-arenadescriptor)

---

## 1. Overview

### What the Kernel Is
The Kernel (`luc_kernel.dll`) is the C++ bedrock of the Lucid Engine. It owns every system that must run at native performance or touch hardware directly: rendering, physics, input, file I/O, memory, and thread management. Everything else — game logic, editor UI, extension behavior — is written in Lucid and runs on top of it.

The Kernel never directly imports or references Lucid code. Communication is strictly one-directional at the ABI level: the Kernel exports a stable C-ABI function table (`LGE_GetAPI()`), and Lucid modules call into it. This is the same boundary model used by Vulkan itself — a thin, versioned, C-compatible surface underneath a higher-level abstraction.

### What the Kernel Is Not
The Kernel is not a monolith. It does not contain editor logic, game scripts, or UI layout definitions. Those live in the `engine/` (editor) and `user_projects/` (game) directories, compiled to `.lmod` / `.dll` files that the Kernel loads at runtime. If the editor panels were deleted, the Kernel would still boot, still run a game, still render frames — it's just missing its IDE skin.

### The Microkernel Boundary

```
┌──────────────────────────────────────────────┐
│               Lucid Layer                    │
│  editor/*.Lucid   game/*.Lucid   extensions  │
│              (lmod / dll files)              │
├──────────────────────────────────────────────┤
│           C ABI Surface (lge_api.h)          │
│         LGE_GetAPI() → LGE_ExtensionAPI      │
├──────────────────────────────────────────────┤
│              luc_kernel.dll                  │
│  ECS · RHI · Physics · Input · VFS · Audio  │
│  Script Manager · Extension Loader · ImGui   │
└──────────────────────────────────────────────┘
│     Externals (Jolt, GLFW, Vulkan, GLM)      │
└──────────────────────────────────────────────┘
```

### Core Design Rules
- **No heap allocation in hot paths.** All per-frame work uses pre-allocated pools or stack memory.
- **No STL in the ABI surface.** `lge_api.h` uses only C types (raw pointers, primitive types, POD structs). STL types cross DLL boundaries unsafely.
- **POD components only.** ECS component structs are plain data — no virtual methods, no owning pointers, no constructors. They can be `memcpy`'d, serialized, and reflected without special handling.
- **Systems never talk to each other directly.** They communicate through shared ECS component data. `PhysicsSystem` writes to `TransformComponent`; `RenderSystem` reads from it. They never call each other.

---

### Keywords & Glossary

A reference for every acronym, term, and file format used throughout this document. When in doubt, come here first.

#### Acronyms & Terms

| Term | Full Name | Meaning in this engine |
|------|-----------|------------------------|
| **LGE** | Lucid Game Engine | The engine's namespace and export prefix. Every public C-ABI symbol starts with `LGE_` (e.g. `LGE_GetAPI`, `LGE_Boot`) to avoid name collisions with other libraries. |
| **ECS** | Entity-Component-System | The engine's scene/data model. Every object in the world is an `EntityID` (a `uint64_t` number). Data is attached via components (POD structs). Logic runs in systems that iterate over matching components. No class hierarchy, no scene nodes. |
| **RHI** | Render Hardware Interface | The abstraction layer sitting between the engine and the raw Vulkan SDK. Everything else in the engine calls `RHI::BeginFrame()`, `LGE_DrawMesh()`, etc. — it never touches `VkSubmitInfo` or `VkDescriptorSet` directly. |
| **VFS** | Virtual File System | A unified file-access API that works identically in development (loose files on disk) and in shipping (files packed inside an encrypted `.pck` bundle). Engine code always reads through the VFS, never with raw OS file paths. |
| **FFI** | Foreign Function Interface | The mechanism that lets Lucid code and C++ extensions call into the kernel. The kernel exports a flat C-ABI function table; Lucid modules receive it as a pre-negotiated `api` object. No DLL linking required. |
| **ABI** | Application Binary Interface | The low-level contract between compiled binaries — calling conventions, struct layouts, symbol names. The kernel's ABI surface (`lge_api.h`) uses only C types so it is stable across compilers and language runtimes. |
| **API** | Application Programming Interface | The set of functions and types a system exposes for others to call. In this engine, each subsystem has a corresponding API header (e.g. `lge_physics.h`, `lge_input.h`). |
| **SDK** | Software Development Kit | The pre-compiled package game developers download to build games. Contains `luc_kernel.dll`, `luc_compiler.exe`, `luc_langserver.exe`, the editor, and the standard library. Does not include C++ kernel source code. |
| **POD** | Plain Old Data | A C/C++ struct with no constructors, no virtual methods, and no owning pointers. POD structs can be `memcpy`'d, serialized to disk, and passed across DLL boundaries safely. All ECS components are POD. |
| **STL** | Standard Template Library | C++'s built-in containers (`std::vector`, `std::string`, `std::map`, etc.). These are **not** used in the kernel's ABI surface because their memory layout differs across compilers and C++ runtimes — passing an `std::string` across a DLL boundary can corrupt memory. |
| **DOM** | Document Object Model | The tree of UI elements that RmlUI manages internally, similar to an HTML DOM in a web browser. Each `ui:div`, `ui:button` etc. in Lucid corresponds to a node in this tree. |
| **DRM** | Digital Rights Management | The technical systems that protect the engine's license and the shipped game's assets from unauthorized use. In this engine: Ed25519-signed license files + AES-256 encrypted asset bundles. |
| **IDE** | Integrated Development Environment | The full editor experience — the window the game developer works in, including the scene view, inspector, console, and asset browser. The Lucid Engine's IDE is itself written in Lucid and rendered by the kernel. |
| **IPC** | Inter-Process Communication | Communication between two separate running programs. Used by the engine console to send commands from the editor process into a running game `.exe` via a named pipe or socket. |
| **PSO** | Pipeline State Object | A Vulkan concept: a compiled GPU rendering pipeline (shaders + blend modes + vertex format). Creating PSOs is expensive; the RHI caches them on first use so they are never recreated mid-game. |
| **VMA** | Vulkan Memory Allocator | A third-party library (MIT, by AMD) used by the RHI to manage GPU memory. Handles sub-allocation, defragmentation, and memory type selection — things that are easy to get wrong in raw Vulkan. |
| **GLFW** | Graphics Library FrameWork | A lightweight C library for creating OS windows and receiving input events (keyboard, mouse, gamepad) on Windows, Linux, and macOS. Used as the desktop platform provider behind the `IInputProvider` interface. |
| **GLM** | OpenGL Mathematics | A C++ math library (vectors, matrices, quaternions) that mirrors GLSL shader math. Used throughout the engine for transform calculations, camera matrices, and physics positions. |
| **ADPCM** | Adaptive Differential Pulse-Code Modulation | A compressed audio format used for short sound effects (`.lsfx`). Faster to decode than MP3/Ogg and suitable for real-time mixing with low CPU overhead. |
| **PBKDF2 / HKDF** | Password-Based Key Derivation Function 2 / HMAC-based Key Derivation Function | Cryptographic functions used to derive the AES-256 VFS decryption key from the publisher's master key and the machine's UUID. Ensures a license file from one machine cannot decrypt assets on another. |
| **SHA-256** | Secure Hash Algorithm 256-bit | A one-way hash function. Used at boot to compute a fingerprint of `luc_kernel.dll` and compare it against the expected value — detecting if the kernel binary has been tampered with. |
| **AES-256** | Advanced Encryption Standard (256-bit key) | The symmetric encryption algorithm used to encrypt shipped game asset bundles (`.pck` files). Industry-standard, hardware-accelerated on modern CPUs. |
| **Ed25519** | Edwards-curve Digital Signature Algorithm | The public-key signature scheme used to sign engine license files and extension packages. Fast, compact signatures (64 bytes). The public key is baked into `luc_kernel.dll`. |
| **UUID** | Universally Unique Identifier | A 128-bit identifier. Used as the machine fingerprint in license validation — derived from hardware IDs so a license tied to one machine fails on another. |
| **CSS / RCSS** | Cascading Style Sheets / RmlUI CSS | The stylesheet language used to style in-game UI (colors, layout, shadows, animations). RCSS is RmlUI's CSS dialect — nearly identical to web CSS with a few engine-specific extensions. |
| **RML** | RmlUI Markup Language | The HTML-like markup format that describes in-game UI structure (elements, hierarchy). Generated by the engine's Visual UI Designer; game developers interact with it via the Lucid `ui:` bridge instead of writing RML by hand. |
| **SFX** | Sound Effect | A short, fully-decoded audio clip (stored as `.lsfx`) played on demand with low latency. Contrast with streaming audio (`.lstream`) which is decoded progressively from disk. |
| **NDK** | Native Development Kit | Android's C/C++ SDK. Referenced in the input system as the future platform provider for Android targets, replacing GLFW behind the `IInputProvider` interface. |

---

#### File Formats

| Extension | Name | Description |
|-----------|------|-------------|
| `.Lucid` | Lucid Source File | Human-readable source code written in the Lucid language. This is what developers author. The compiler turns these into `.lmod` or native `.dll` files. Equivalent to `.cs` for C# or `.py` for Python. |
| `.lmod` | Lucid Module | A compiled, native-code binary produced by `luc_compiler.exe` from one or more `.Lucid` source files. Loaded by the kernel's Script Manager at runtime. Equivalent to a `.so` or `.dll` but scoped to the Lucid runtime. |
| `.dll` | Dynamic-Link Library | A Windows shared library. The kernel itself (`luc_kernel.dll`) is a `.dll`. C++ extensions also ship as `.dll` files. On Linux the equivalent is `.so` (shared object) — the engine's platform layer abstracts the difference. |
| `.exe` | Executable | A Windows application binary. `luc_compiler.exe`, `luc_langserver.exe`, `LucidEditor.exe`, and the shipped game's launcher are all `.exe` files. |
| `.h` | C/C++ Header | A C or C++ header file declaring types, structs, and function signatures. The kernel's public ABI is defined entirely in `.h` files under `kernel/include/` (e.g. `lge_api.h`, `lge_render.h`). |
| `.cpp` | C++ Source File | A C++ implementation file. All kernel internals are `.cpp` files under `kernel/src/`. Game developers never write `.cpp` — that's the engine team's domain. |
| `.pck` | Pack / Asset Bundle | An encrypted, packed archive containing all of a shipped game's assets (textures, audio, meshes). Read-only at runtime, decrypted in RAM by the VFS. Never written to disk in decrypted form. |
| `.lic` | License File | An Ed25519-signed file issued by the engine's licensing system. Stored on the developer's machine. Verified at boot by the `ILicenseVerifier` backend against the public key baked into `luc_kernel.dll`. |
| `.lsfx` | Lucid Sound Effect | A short audio clip in ADPCM format. Fully loaded into RAM at startup for zero-latency playback. Used for game sounds: footsteps, shots, UI clicks. |
| `.lstream` | Lucid Audio Stream | A long audio file in Ogg Vorbis format. Decoded progressively from disk/VFS during playback — never fully loaded into RAM. Used for music and long ambient loops. |
| `.lmesh` | Lucid Mesh | A cooked binary mesh asset produced by the asset baker. Contains a 32-byte header followed by raw Vulkan-compatible vertex and index buffers. Zero parsing overhead — loaded with a single `memcpy` into a GPU buffer. |
| `.rml` | RmlUI Markup | An HTML-like file describing an in-game UI document's element structure. Generated by the engine's Visual UI Designer. Loaded by `LGE_UI_LoadDocument()` at runtime. |
| `.rcss` | RmlUI Stylesheet | A CSS-like file controlling the visual appearance of an in-game UI document (colors, layout, shadows, fonts). Paired with a `.rml` file. Supports CSS variables for runtime theming. |
| `lge_api.h` | Kernel ABI Header | The master public header for the kernel's C ABI. Defines `LGE_ExtensionAPI` (the function table struct) and `LGE_GetAPI()`. Every extension includes only this file — no other kernel headers. |
| `extension.json` | Extension Manifest | A JSON file shipped alongside each extension. Declares the extension's name, version, required kernel API version, and entry-point symbol. Read by the kernel's Extension Loader before the `.dll`/`.lmod` is opened. |
| `symbols.json` | ECS Reflection Schema | A JSON file generated by the compiler for each compiled module. Describes which structs are ECS components and which fields are editor-inspectable. Read by the Property Inspector to auto-generate UI for custom components. |

---

## 2. Boot Sequence

### Introduction
The boot sequence is the ordered series of initializations that takes the kernel from a blank process to a fully running frame loop. It is deterministic — every subsystem comes up in the same order every time, and a failure at any stage shuts down cleanly rather than crashing.

### How It Works
`kernel.cpp` contains the single entry point, `LGE_Boot()`, which the platform bootstrap (`LucidEditor.exe` or a shipped game's `.exe`) calls once. The boot proceeds in strict dependency order:

```
1. Platform Layer      (window, OS handles)
2. Vulkan RHI          (device, swapchain, memory allocator)
3. ImGui               (editor shell renderer — attaches to RHI swapchain)
4. VFS                 (mount encrypted asset bundle, set up file roots)
5. ECS World           (allocate archetype storage, register core components)
6. Physics             (Jolt init, broad-phase, contact listener)
7. Audio               (device open, mixing thread start)
8. Input Bridge        (GLFW callbacks registered)
9. Security / License  (verify boot checksum, check license file)
10. Script Manager     (load core .lmod files — editor or game entry point)
11. Frame Loop         (begins — never returns until shutdown)
```

### Why This Order Matters
Each step depends on the one before it. RHI must exist before ImGui (needs a swapchain to draw into). VFS must be mounted before the Script Manager (scripts and assets are inside the bundle). ECS must be ready before Physics (Jolt body creation stores results into ECS components). Security runs late enough that all other systems are live and verifiable (the boot checksum covers the loaded `.dll`s), but early enough that no game logic has started.

### Code Structure

```cpp
// kernel/src/core/kernel.cpp

void LGE_Boot(const LGE_BootConfig& cfg) {
    Platform::Init(cfg.window_title, cfg.width, cfg.height);
    RHI::Init();
    ImGuiShell::Init(RHI::GetSwapchain());
    VFS::Mount(cfg.asset_bundle_path);
    ECSWorld::Init();
    PhysicsBridge::Init();
    AudioSystem::Init();
    InputBridge::Init(Platform::GetWindow());
    Security::VerifyBoot();
    ScriptManager::LoadEntry(cfg.entry_module);
    RunFrameLoop();   // blocks until window close / LGE_Shutdown()
}
```

---

## 3. ECS (Entity-Component-System)

### Introduction
The ECS is the data backbone of every scene in the engine. Every object in the world — a mesh, a light, a player, a trigger zone — is an `EntityID` (a `uint64_t`) with components attached to it. There are no base classes, no scene nodes, no inheritance hierarchies. A "character" is just an entity that happens to have a `TransformComponent`, a `MeshComponent`, a `PhysicsComponent`, and a `HealthComponent` at the same time.

This is the engine's most performance-sensitive system. It must handle tens of thousands of entities per frame without cache misses.

### How It Works
Components of the same type are stored in contiguous arrays (an "archetype" storage). When `RenderSystem` iterates over all entities with `MeshComponent + TransformComponent`, it reads a flat array of structs in sequential memory — no pointer chasing, no virtual dispatch.

```
World
├── Archetype [Transform, Mesh]           → array of N entities
├── Archetype [Transform, Physics, Mesh]  → array of M entities
├── Archetype [Transform, Audio]          → array of K entities
└── ...
```

Adding or removing a component moves an entity from one archetype bucket to another — an O(1) operation using a lookup table of `EntityID → ArchetypeSlot`.

### Why It's Needed
- **Cache efficiency.** Iterating 10,000 `TransformComponent`s in a flat array is fast. Iterating 10,000 game objects through virtual pointers is not.
- **Extension-friendly.** Extensions register new component types at runtime without touching the core. The archetype system absorbs new types automatically.
- **System parallelism.** Systems that touch non-overlapping component sets (e.g. `AudioSystem` on `AudioComponent` vs. `RenderSystem` on `MeshComponent`) can run on separate threads with no locking.

### Code Structure

```cpp
// kernel/include/lge_ecs.h

using EntityID = uint64_t;

// Register a new component type — called by extensions on load
uint32_t  LGE_RegisterComponent(const char* name, size_t size, size_t align);

EntityID  LGE_CreateEntity();
void      LGE_DestroyEntity(EntityID id);
void*     LGE_AddComponent(EntityID id, uint32_t component_type);
void*     LGE_GetComponent(EntityID id, uint32_t component_type);
void      LGE_RemoveComponent(EntityID id, uint32_t component_type);

// System iteration — callback receives a flat array slice per archetype bucket
void LGE_ForEach(
    uint32_t* component_types,  // array of types to query
    uint32_t  type_count,
    void (*callback)(EntityID*, void** component_ptrs, uint32_t count)
);
```

---

## 4. Vulkan RHI (Render Hardware Interface)

### Introduction
The RHI is the engine's abstraction over the raw Vulkan SDK. Direct Vulkan code is verbose, stateful, and easy to get wrong — `VkSubmitInfo`, `VkPipelineLayout`, descriptor set management, synchronization primitives. The RHI hides all of that behind a clean API that the rest of the Kernel (and never any Lucid code) calls.

### How It Works
The RHI owns the Vulkan device, swapchain, and memory allocator (VMA). It exposes a simple frame lifecycle to the Kernel's frame loop:

```cpp
RHI::BeginFrame();   // acquire next swapchain image, reset command buffers
// ... RenderSystem submits draw calls here ...
RHI::EndFrame();     // submit command buffer
RHI::Present();      // present swapchain image, handle resize if needed
```

Internally it uses a **bindless descriptor** model: all textures and buffers are registered into a global GPU-side array at load time, and shaders address them by integer index (`textureSampler[textureID]`). This removes per-draw descriptor set binding from the hot path.

Pipeline State Objects (PSOs) are cached on first use and reused every subsequent frame, eliminating "shader stutter" during gameplay.

### Why It's Needed
Without this layer, every system that submits GPU work (the render system, ImGui, RmlUI, the physics debug renderer) would need to manage its own Vulkan state — a recipe for conflicts and crashes. The RHI gives them all a single, synchronized submission path.

### Code Structure

```cpp
// kernel/include/lge_render.h

// Texture management
TextureID LGE_UploadTexture(const void* pixels, uint32_t w, uint32_t h, PixelFormat fmt);
void      LGE_FreeTexture(TextureID id);

// Mesh management
BufferID  LGE_UploadMesh(const Vertex* verts, uint32_t vcount,
                          const uint32_t* indices, uint32_t icount);

// Draw submission (called by RenderSystem each frame)
void LGE_DrawMesh(BufferID mesh, TextureID albedo, const Mat4* transform);

// Frame lifecycle (called by kernel.cpp frame loop only)
void LGE_RHI_BeginFrame();
void LGE_RHI_EndFrame();
void LGE_RHI_Present();
```

---

## 5. Physics Bridge (Jolt)

### Introduction
The Physics Bridge wraps Jolt Physics so the rest of the engine (and Lucid scripts) never import Jolt headers directly. Jolt is a C++ library — it needs to stay behind the bridge to keep compile times reasonable and to allow a future physics backend swap without touching game code.

### How It Works
At boot, `PhysicsBridge::Init()` creates a Jolt `PhysicsSystem` with a broad-phase layer setup. Each frame, `PhysicsSystem::Update(dt)` steps the simulation. Jolt calls the engine's `LucContactListener` when collisions occur, which translates them into Lucid-visible callback events.

The bridge writes results back into ECS components — `TransformComponent.position` and `.rotation` are updated directly from Jolt body positions. The `RenderSystem` reads those components next in the frame. Jolt and Vulkan never communicate directly.

```
Frame order:
  1. PhysicsBridge::Update(dt)   → writes TransformComponent
  2. RenderSystem::Update()      → reads TransformComponent → submits draw calls
```

### Why It's Needed
Jolt's AAA-quality simulation (used in *Horizon Forbidden West*, *Death Stranding 2*) would be inaccessible to Lucid scripts without a translation layer, since Jolt uses C++ classes that can't cross the C ABI. The bridge translates between Jolt's object model and the engine's `EntityID`-based ECS.

### Code Structure

```cpp
// kernel/include/lge_physics.h

BodyID LGE_Physics_CreateBody(EntityID entity, const PhysicsBodyDef& def);
void   LGE_Physics_DestroyBody(BodyID id);
void   LGE_Physics_SetVelocity(BodyID id, Vec3 velocity);
void   LGE_Physics_AddForce(BodyID id, Vec3 force);

// Raycasting (used by the editor inspector for click-to-select)
bool   LGE_Physics_Raycast(Vec3 origin, Vec3 dir, float max_dist,
                            RaycastHit* out_hit);

// Collision callbacks — Lucid binds to these via api.physics.onCollide()
void   LGE_Physics_SetContactCallback(
           void (*on_contact)(EntityID a, EntityID b, Vec3 contact_point));
```

---

## 6. Input Bridge (GLFW)

### Introduction
The Input Bridge normalizes raw OS input events (keyboard, mouse, gamepad, touch) into a unified internal format and makes them available to Lucid scripts via the `io` standard library. The underlying platform provider (GLFW on desktop) is hidden behind an `IInputProvider` interface so it can be swapped for Android NDK, iOS UIKit, or console SDKs without changing any Lucid code.

### How It Works
GLFW registers callbacks on the OS window for key events, mouse movement, scroll, and gamepad axes. Each callback writes into a frame-local input state buffer. At the start of each frame, the Kernel snapshots this buffer and makes it available to systems for that frame. Lucid's `io.key.W.onHeld()` and `io.mouse.left.onPressed()` are syntactic sugar over polling that snapshot.

The same abstraction allows PC developers to test mobile/console input by remapping mouse clicks to virtual touch events or WASD to a virtual gamepad — no separate code path needed.

### Why It's Needed
Raw GLFW callbacks fire on the OS message thread. Directly calling Lucid callbacks from there would cause threading hazards with the ECS. The bridge buffers events, decouples them from the OS thread, and presents a clean, frame-synchronized view of input state to the rest of the engine.

### Code Structure

```cpp
// kernel/include/lge_input.h

// Polled state (sampled once per frame)
bool  LGE_Input_IsKeyHeld(KeyCode key);
bool  LGE_Input_IsKeyPressed(KeyCode key);   // true only on first frame down
bool  LGE_Input_IsKeyReleased(KeyCode key);
Vec2  LGE_Input_MousePosition();
Vec2  LGE_Input_MouseDelta();
float LGE_Input_MouseScrollDelta();

// Callback registration (Lucid io library binds these)
void LGE_Input_OnKeyPressed(KeyCode key, void (*callback)());
void LGE_Input_OnMouseButton(MouseButton btn, void (*on_press)(), void (*on_release)());
```

---

## 7. UI Backend (ImGui Shell + RmlUI In-Game)

### Introduction
The engine runs two separate UI backends, each doing the job it was designed for (see MasterPlan Decision 14). **ImGui** renders the Engine Shell — all editor tool panels (inspector, hierarchy, console, asset browser). **RmlUI** renders in-game UI — HUDs, menus, and UI elements attached to game entities via `UIComponent`. Neither system is used for the other's job.

### How It Works

**ImGui Shell:** ImGui is initialized against the RHI swapchain at boot (`ImGuiShell::Init()`). Each frame, after `RHI::BeginFrame()`, the Kernel calls `ImGuiShell::BeginFrame()`, which opens a new ImGui frame. The editor's C++ panel code runs (inspector, hierarchy, console, etc.), accumulating ImGui draw calls. `ImGuiShell::EndFrame()` flushes those as a single vertex/index batch into the RHI's command buffer, drawn on top of the 3D viewport before `RHI::Present()`.

**RmlUI In-Game:** RmlUI is initialized separately with its own Vulkan render backend. In-game UI documents (`.rml` files generated by the Visual UI Designer or loaded from disk) are loaded into RmlUI contexts. Entities with a `UIComponent` own an RmlUI context; the `UISystem` updates those contexts each frame and submits their vertex batches through the RHI. The Lucid UI Bridge (Decision 15) lets scripts update RmlUI element properties reactively without rebuilding the DOM.

Both backends emit draw calls into the same Vulkan command buffer but occupy distinct screen regions — they never composite over each other's pixels in the same frame region, so input focus arbitration between them is not needed.

### Why Two Backends
The Engine Shell needs: many simultaneous panels, flat colors, transparency, rounded corners, gradients, PNG textures — all native to ImGui's draw-list API, with no CSS engine overhead. In-game UI needs: CSS flexbox layout, animated transitions, designer-authored RCSS stylesheets, and rich visual effects (`box-shadow`, `filter: blur()`) — RmlUI's native strengths. Neither system covers the other's use case well.

### Code Structure

```cpp
// kernel/src/ui/imgui_shell.cpp — Editor shell panels
void ImGuiShell::Init(VkSwapchain* swapchain);
void ImGuiShell::BeginFrame();      // called each frame before panel code
void ImGuiShell::EndFrame();        // flushes to RHI command buffer

// kernel/src/ui/rmlui_backend.cpp — In-game RmlUI integration
RmlContextID LGE_UI_CreateContext(uint32_t width, uint32_t height);
void         LGE_UI_LoadDocument(RmlContextID ctx, const char* rml_path);
void         LGE_UI_SetProperty(RmlContextID ctx, const char* element_id,
                                 const char* property, const char* value);
void         LGE_UI_UpdateAndRender(RmlContextID ctx); // called by UISystem per frame
```

---

## 8. Audio Pipeline

### Introduction
The audio system handles playback of both short sound effects (`.lsfx`) and long streaming music/ambience tracks (`.lstream`), with support for 2D and 3D spatialized audio. It runs on a dedicated mixing thread so audio never stutters when the main thread is busy.

### How It Works
At boot, `AudioSystem::Init()` opens the OS audio device and starts a background mixing thread that wakes at fixed intervals (typically every 10ms) to fill the device's output buffer. Game code triggers sounds by calling `LGE_Audio_Play()`, which posts a command to a lock-free queue. The mixing thread reads from that queue, decodes the next chunk of audio data (ADPCM for SFX, Ogg Vorbis for streams), applies 3D spatialization math if the source has a position, and mixes into the output buffer.

`AudioComponent` on an entity stores the current asset handle, playback state, and 3D position. The `AudioSystem` reads each entity's `AudioComponent` and `TransformComponent` each frame to update spatialization parameters.

### Why It's Needed
Audio mixing is time-sensitive — an OS audio callback fires on a strict timer and must be filled immediately or the user hears a glitch. Running this on the main game thread risks starvation during heavy frames. A dedicated mixing thread with a lock-free command queue decouples audio timing from game logic timing entirely.

### Code Structure

```cpp
// kernel/include/lge_audio.h

AudioSourceID LGE_Audio_LoadSFX(const char* lsfx_path);
AudioSourceID LGE_Audio_LoadStream(const char* lstream_path);
void          LGE_Audio_Play(AudioSourceID id, bool loop);
void          LGE_Audio_Stop(AudioSourceID id);
void          LGE_Audio_SetVolume(AudioSourceID id, float volume);   // 0.0 - 1.0
void          LGE_Audio_SetPosition(AudioSourceID id, Vec3 world_pos); // 3D spatialization
void          LGE_Audio_SetListenerTransform(Vec3 pos, Vec3 forward);
```

---

## 9. Virtual File System (VFS)

### Introduction
The VFS provides a single, unified file-access API that works identically in development (reading loose files from disk) and in production (reading from an AES-256 encrypted `.pck` bundle). Game code and engine internals never use raw OS file paths — they always go through the VFS so that the same code works in both contexts with no changes.

### How It Works
At boot, `VFS::Mount()` is called with the path to the asset bundle (or a development directory). In production, it decrypts the bundle's header table (AES-256, key derived from the publisher key + machine fingerprint via PBKDF2/HKDF), loads the file index into memory, and serves subsequent `VFS::Open()` calls by seeking into the packed file. Files are decrypted in RAM and never written to disk — they exist only in the process's address space for the duration of the read.

In development, `VFS::Mount()` points at a loose directory. `VFS::Open("textures/hero.png")` is a normal file open — no decryption. The same API, different backend.

### Why It's Needed
Without a VFS, shipping a game means either distributing raw unprotected assets or writing asset-loading code twice (once for dev, once for prod). The VFS collapses that into a single always-on abstraction. The encryption layer on top provides the DRM asset protection described in Decisions 10 and 24.

### Code Structure

```cpp
// kernel/include/lge_vfs.h

void    LGE_VFS_Mount(const char* bundle_path_or_dir);
VFSFile LGE_VFS_Open(const char* virtual_path);   // e.g. "audio/music/theme.lstream"
size_t  LGE_VFS_Read(VFSFile file, void* out_buf, size_t bytes);
size_t  LGE_VFS_Size(VFSFile file);
void    LGE_VFS_Close(VFSFile file);
bool    LGE_VFS_Exists(const char* virtual_path);
```

---

## 10. Script Manager & Hot-Reload

### Introduction
The Script Manager is responsible for loading compiled Lucid modules (`.lmod` / `.dll` files), calling their lifecycle hooks (`on_load`, `on_unload`), and swapping them at runtime when source files change — what the engine calls "hot-reload." This is what makes saving a `.Lucid` file in the editor instantly reflect changes in the running game without restarting.

### How It Works
Three components work in sequence:

**FileWatcher** — a background thread watches the active project's `src/` directory using native OS APIs (`ReadDirectoryChangesW` on Windows, `inotify` on Linux). When a `.Lucid` file is saved, it fires an event with the changed file's path.

**Compiler Subprocess** — the Kernel spawns `luc_compiler.exe` as a child process, targeting the changed file. Output goes to a `_next.dll` temp file, never overwriting the live module. If compilation fails, the live module is untouched and errors are piped to the editor console.

**Module Swapper** — on a successful compile, the Swapper waits for the current frame to finish, calls the old module's `on_unload` hook, unloads it, atomically renames `_next.dll` → `.dll`, loads the new module, and calls its `on_load` hook. Because all game state lives in ECS components (not inside scripts), the new module picks up exactly where the old one left off.

### Why It's Needed
Hot-reload is one of the engine's flagship developer experience features. Without it, every code change in a game requires a full restart — at scale, this costs developers hours per day. Because Lucid's ECS design stores all state in components rather than script instances, hot-reload comes essentially for free: there is no "script state" to migrate.

### Code Structure

```cpp
// kernel/src/core/script_manager.cpp

void ScriptManager::LoadEntry(const char* module_path);
void ScriptManager::RecompileModule(const string& luc_file);  // triggered by FileWatcher
void ScriptManager::SwapModule(const string& module_name);    // called after successful compile

// Per-module handle tracked in the live module table
struct LiveModule {
    HMODULE       handle;
    LGE_ModuleAPI api;          // function pointers resolved after load
    string        source_path;
};
```

---

## 11. Extension System & FFI

### Introduction
Extensions are `.dll` files (C++ or compiled Lucid) that plug new functionality into the engine at runtime — new ECS components, new editor panels, new Lucid API namespaces. The FFI (Foreign Function Interface) is the contract that makes this safe: extensions never link against the Kernel directly; they call through a versioned function table so that old extensions keep working after kernel updates.

### How It Works
The Kernel exports a single entry point: `LGE_GetAPI(uint32_t requested_version)`. An extension calls this at load time and receives a `LGE_ExtensionAPI` struct — a flat table of C function pointers covering every subsystem. If the Kernel's major version is higher than the extension's requested version, it returns `nullptr` and the extension fails gracefully rather than crashing.

Lucid extensions receive a pre-negotiated `api` object from the Kernel before their `on_load` hook is called — version checking already done, no raw pointers to manage. C++ extensions call `LGE_GetAPI` directly.

### Why It's Needed
Without versioned FFI, every Kernel update would be a breaking change for every extension ever built. The function table model (the same one Vulkan uses) means a major version bump is the only event that can break compatibility — and extensions declare which major version they were built against, so the mismatch is detected at load time, not at the first crash.

### Code Structure

```cpp
// kernel/include/lge_api.h

struct LGE_ExtensionAPI {
    uint32_t version;

    // ECS
    EntityID (*create_entity)();
    void*    (*add_component)(EntityID, uint32_t type);
    void*    (*get_component)(EntityID, uint32_t type);

    // Render
    TextureID (*upload_texture)(const void* pixels, uint32_t w, uint32_t h);
    void      (*draw_mesh)(BufferID mesh, TextureID tex, const Mat4* transform);

    // Physics, Input, Audio, VFS ... (one function pointer per subsystem call)
};

// The one stable symbol the kernel exports
LGE_ExtensionAPI* LGE_GetAPI(uint32_t requested_version);
```

---

## 12. Console & Command Pipeline

### Introduction
The engine console is not a text parser bolted on the side — it is a live bridge into kernel memory. Every registered command is a named C++ function pointer. Typing a command in the console calls that function directly. This means console commands have zero overhead compared to calling the same code from a button click in the editor.

### How It Works
Systems register commands at boot by calling `LGE_Console_Register()` with a name, a description, and a callback. The console interpreter stores these in a flat hash map. When the user types a command (or a Lucid script calls `api.console.exec()`), the interpreter looks up the name in the map and calls the function pointer with the parsed argument string. There is no eval, no scripting layer, no reflection magic — just a function table.

The console supports two execution modes: **in-process** (same window, zero latency) and **out-of-process** (a separate running game `.exe` connected via named pipe / IPC), so the editor console can inject commands into a live game build.

### Why It's Needed
A text-parser-based console would need to re-implement type coercion, error handling, and argument validation for every command — and it would be slower. A function-table console lets each command own its own argument parsing and gives it direct access to kernel state at full C++ speed.

### Code Structure

```cpp
// kernel/include/lge_console.h

void LGE_Console_Register(
    const char* name,
    const char* description,
    void (*callback)(const char* args)
);

void LGE_Console_Exec(const char* command_string);

// IPC bridge for out-of-process execution (editor → running game)
void LGE_Console_StartIPCServer(uint16_t port);
void LGE_Console_ConnectIPC(const char* host, uint16_t port);
```

---

## 13. Security Layer

### Introduction
The security layer handles three distinct concerns: boot integrity (has the kernel been tampered with?), license validation (is this a valid copy of the engine?), and asset protection (are the game's assets encrypted against extraction?). These are implemented as separate, swappable backends behind interfaces — so the online verification path can be added later without touching the kernel's core.

### How It Works
- **Boot checksum:** At the start of `LGE_Boot()`, `Security::VerifyBoot()` computes a SHA-256 hash of `luc_kernel.dll` itself and compares it against a baked-in expected value. A mismatch aborts boot.
- **License verification:** The Kernel holds an `ILicenseVerifier*` pointer. Currently it points to `OfflineLicenseVerifier`, which checks an Ed25519-signed `.lic` file against a public key baked into the kernel. A future `OnlineLicenseVerifier` can be swapped in by changing one assignment in `kernel.cpp` — no other code changes.
- **Asset encryption:** AES-256 keys are never stored raw. They are derived at runtime from the publisher's key + the machine's UUID via PBKDF2/HKDF (`key_derivation.cpp`). A license file from one machine will fail decryption on another.

### Why It's Needed
Engine licensing and asset DRM are requirements for commercial distribution (Decision 4, 10, 24). Implementing them as swappable interfaces rather than hard-coded logic means the offline model ships first (simpler, no server infrastructure) and the online model can be added as a drop-in later.

### Code Structure

```cpp
// kernel/include/lge_security.h

// Swappable license backend (see Decision 4)
struct ILicenseVerifier {
    virtual bool Verify(const char* license_path) = 0;
};

void LGE_Security_SetVerifier(ILicenseVerifier* verifier);
bool LGE_Security_VerifyBoot();   // SHA-256 self-check

// Key derivation for VFS decryption (see Decision 10)
void LGE_Security_DeriveVFSKey(
    const uint8_t* publisher_key, size_t key_len,
    const char*    machine_uuid,
    uint8_t*       out_aes_key    // 32 bytes
);
```

---

## 14. Platform Layer

### Introduction
The Platform Layer isolates all OS-specific code — window creation, dynamic library loading, file system access, thread primitives — so the rest of the Kernel is written against a single abstract interface. Adding a new target platform (e.g. a console) means implementing this one layer, not touching ECS, physics, or rendering.

### How It Works
On Windows, `Platform::Init()` calls `glfwCreateWindow()` (GLFW handles Win32 window setup), `LoadLibrary` / `GetProcAddress` for extension loading, and `ReadDirectoryChangesW` for file watching. On Linux, the same `Platform::Init()` call uses the same GLFW interface for the window, `dlopen` / `dlsym` for loading, and `inotify` for file watching. The rest of the Kernel never calls `LoadLibrary` or `dlopen` directly — it calls `Platform::LoadModule()`.

### Why It's Needed
Cross-platform code that's littered with `#ifdef _WIN32` throughout the physics bridge, script manager, and VFS is maintenance debt that compounds with every new file. Isolating platform-specific calls into one layer means platform-conditional code exists in exactly two files (`win32_loader.cpp`, `linux_loader.cpp`) and nowhere else.

### Code Structure

```cpp
// kernel/include/lge_platform.h

void        Platform::Init(const char* title, uint32_t w, uint32_t h);
void*       Platform::GetNativeWindowHandle();   // HWND on Windows, Window on X11

// Dynamic library loading (wraps LoadLibrary / dlopen)
ModuleHandle Platform::LoadModule(const char* path);
void*        Platform::GetSymbol(ModuleHandle mod, const char* symbol);
void         Platform::UnloadModule(ModuleHandle mod);

// Threading primitives (thin wrappers over std::thread or platform threads)
ThreadHandle Platform::SpawnThread(void (*fn)(void*), void* arg);
void         Platform::JoinThread(ThreadHandle t);
```

---

## 15. Lucid Execution Model (Interpreter → Compiler)

### Introduction
Lucid has two execution modes that share a single frontend and a single intermediate representation. The **interpreter** (`lucid run`) uses LLVM's ORC JIT to compile Lucid source to native code in memory and execute it immediately. The **compiler** (`lucid build`) uses LLVM AOT to emit object files and invoke the system linker. Both modes are built on LLVM — the frontend is written exactly once and produces LLVM IR; what happens to that IR is the only difference between the two modes.

This is a deliberate architectural decision documented in the Lucid grammar:

> *"Both the interpreter and the future compiler are built on LLVM. The Lucid frontend — parser, type checker, and code lowering — produces LLVM IR. What happens to that IR is the only difference between the two modes."*

### How It Works

The pipeline has one shared path and two separate exits, both at the LLVM level:

```
Lucid Source (.Lucid / .lfi)
         │
       Parser
         │
         AST
         │
  Semantic Analysis
  + Type Checking
         │
  LLVM IR Lowering       ← single shared representation
         │
┌────────┴────────┐
│                 │
▼                 ▼
LLVM ORC JIT    LLVM AOT
(lucid run)     (lucid build)
compile to      emit object file
memory,         system linker
execute         resolves symbols,
immediately     produces binary
│                 │
└────────┬────────┘
         │
   luc_kernel.dll
   (C functions via @[foreign("C")])
```

**Stage 1 — Parser:** Reads `.Lucid` source and produces an AST. Identical for both modes.

**Stage 2 — Semantic Analysis:** Resolves names, checks types, validates `@[foreign("C")]` declarations against the known symbol table from `lge_ffi.lfi`. Errors are reported before any code generation begins. Identical for both modes.

**Stage 3 — LLVM IR Lowering:** Translates the validated AST to LLVM IR. Every Lucid intrinsic (`#sqrt`, `#memcpy`, `#atomic_add`, etc.) maps directly to an LLVM intrinsic or IR instruction — written once, shared by both modes. Foreign calls (`@[foreign("C")]`) lower to LLVM `declare` + `call` instructions referencing the external C symbol by name. Identical for both modes.

**Stage 4a — ORC JIT (interpreter mode):** LLVM's ORC JIT receives the IR, compiles it to native machine code in memory, and executes it immediately. Foreign symbols named in `@[link(...)]` are resolved by the JIT's built-in dynamic linker — it calls `dlopen` (Linux/macOS) or `LoadLibrary` (Windows) on the named library and registers its symbols with the JIT's symbol table. When the JIT emits a `call` to a foreign function, the address is resolved from the registered table. No separate marshaling layer (libffi) is needed — LLVM's own codegen handles calling conventions.

**Stage 4b — LLVM AOT (compiler mode):** LLVM emits an object file from the same IR. `@[link(...)]` names are passed directly to the system linker (`ld`, `lld`, `link.exe`) as `-lname` flags. Foreign symbols are unresolved references in the object file; the linker patches their addresses. The output is a native binary with zero runtime overhead on foreign calls.

### Foreign Symbol Resolution — The Key Difference

The `@[foreign("C")]` declaration in `lge_ffi.lfi` produces the same LLVM IR in both modes — a `declare` statement naming the external symbol. What differs is only *when* and *how* that symbol's address is filled in:

| | ORC JIT (interpreter) | LLVM AOT (compiler) |
|---|---|---|
| When resolved | At JIT startup, via `dlopen` | At link time, by the system linker |
| How resolved | JIT dynamic linker registers the `.dll`/`.so` symbols | Linker patches the object file's relocation entries |
| Calling convention | LLVM codegen handles it | LLVM codegen handles it |
| libffi needed | No | No |
| Runtime overhead | Zero (native call instruction) | Zero (native call instruction) |

Both modes produce native `call` instructions. The JIT just does the linker's job in memory at startup instead of on disk at build time.

### What This Means for the Source File Requirement

The grammar documents one important asymmetry (lines 156–161): in compiler mode, a C source file path in `@[link("wrapper.c")]` can be compiled inline as part of the build. In interpreter mode, source files cannot be compiled on the fly — they must be pre-compiled to a `.so`/`.dylib`/`.dll` first. For `luc_kernel.dll` this is always pre-built, so it applies equally in both modes.

### Intrinsics

Every Lucid intrinsic maps directly to an LLVM IR node, lowered once in the frontend, shared by both modes:

```
Lucid intrinsic    LLVM IR node
─────────────────────────────────────────────
#sqrt(x)       →   call @llvm.sqrt.f32(float %x)
#memcpy(d,s,n) →   call @llvm.memcpy(ptr %d, ptr %s, i64 %n)
#atomic_add    →   atomicrmw add <ptr>, <val> <ordering>
#simd_add      →   add <N x T> %a, %b
#sizeof(T)     →   DataLayout::getTypeAllocSize (compile-time constant)
#toRef(ptr)    →   non-null assertion + bitcast
#toPtr(ref)    →   bitcast to pointer type
```

### Code Structure

```
luc_frontend/
├── src/
│   ├── parser.cpp          -- Lucid source → AST
│   ├── sema.cpp            -- semantic analysis and type checking
│   ├── ir_lowering.cpp     -- AST → LLVM IR (shared by both modes)
│   ├── intrinsics.cpp      -- Lucid intrinsic → LLVM IR node mappings
│   └── foreign.cpp         -- @[foreign("C")] → LLVM declare + call
│
luc_interpreter/
├── src/
│   ├── jit.cpp             -- LLVM ORC JIT session setup and execution
│   └── dynlink.cpp         -- dlopen/LoadLibrary wrapper for @[link(...)]
│
luc_compiler/
├── src/
│   ├── aot.cpp             -- LLVM AOT object file emission
│   └── linker.cpp          -- invokes system linker with @[link(...)] flags
```

---

## 16. FFI Foreign Symbol Resolution

### Introduction
Foreign symbol resolution is the mechanism by which `@[foreign("C")]` declarations in Lucid code find the actual C function addresses in `luc_kernel.dll`. The process is completely different in each mode — but because both are built on LLVM, neither requires a separate marshaling library or a runtime symbol table. LLVM handles calling conventions in both cases.

### Interpreter Mode (ORC JIT)

The ORC JIT has a built-in dynamic linker. At startup, for every `@[link("luc_kernel")]` annotation the JIT encounters, it:

1. Calls `dlopen("luc_kernel.dll")` / `LoadLibrary("luc_kernel.dll")` to load the library
2. Registers all of its exported symbols with the JIT's symbol table
3. When JIT-compiled code calls a foreign function, the JIT resolves the address from the registered table — the same resolution a system linker would perform at compile time, done in memory at runtime

The foreign call in the JIT-compiled native code is a direct `call` instruction to the resolved address. There is no wrapper, no marshaling layer, no overhead beyond the call itself. LLVM's codegen handles the platform calling convention (System V AMD64 on Linux, Microsoft x64 on Windows) from the `declare` statement's parameter types.

```
lge_ffi.lfi declares:
  @[foreign("C"), link("luc_kernel")]
  const LGE_Physics_CreateBody (entity uint64, def *uint8) -> uint32 = {}

ORC JIT at startup:
  dlopen("luc_kernel.dll")               → module handle
  register all exported symbols           → JIT symbol table entry:
                                            "LGE_Physics_CreateBody" → 0x7ff8a3c2...

JIT-compiled Lucid call:
  LGE_Physics_CreateBody(entity, def)    → native CALL 0x7ff8a3c2...
                                            (direct address, zero overhead)
```

### Compiler Mode (AOT)

The AOT compiler emits an object file. The `declare` for a foreign function becomes an undefined external symbol reference in the object file. `@[link("luc_kernel")]` is passed to the system linker as `-lluc_kernel`. The linker finds the symbol in `luc_kernel.dll`'s export table and patches the address into the object file's relocation entries.

```
lge_ffi.lfi declares:
  @[foreign("C"), link("luc_kernel")]
  const LGE_Physics_CreateBody (entity uint64, def *uint8) -> uint32 = {}

LLVM AOT emits in object file:
  declare i32 @LGE_Physics_CreateBody(i64 %entity, ptr %def)
  ...
  call i32 @LGE_Physics_CreateBody(...)    ← unresolved reference

System linker:
  -lluc_kernel                             → finds LGE_Physics_CreateBody
                                             in luc_kernel.dll export table
                                           → patches CALL address in binary

Shipped binary:
  CALL 0x7ff8a3c2...                       ← direct address, zero overhead
```

### Why No libffi

libffi exists to call C functions dynamically when you don't know their signature at compile time — interpreters like CPython use it because Python has no static type information about C function signatures. Lucid does not have this problem. Every `@[foreign("C")]` declaration carries full, explicit type information — parameter types and return type are written in the `.lfi` file. LLVM receives that type information as part of the `declare` statement and generates a correct calling-convention-aware `call` instruction from it. There is nothing left for libffi to do.

### Code Structure

```
luc_interpreter/
├── src/
│   ├── jit.cpp          -- ORC JIT session: ThreadSafeContext, LLJIT setup,
│   │                       IR compilation, module addition
│   └── dynlink.cpp      -- loads @[link(...)] libraries via dlopen/LoadLibrary,
│                           registers symbols with JIT's DynamicLibrarySearchGenerator

luc_compiler/
├── src/
│   ├── aot.cpp          -- TargetMachine setup, object file emission via
│   │                       PassManager + raw_fd_ostream
│   └── linker.cpp       -- assembles linker invocation from @[link(...)] annotations,
│                           calls lld or system ld/link.exe

-- Note: no ffi_dispatch.cpp, no type_marshal.cpp, no libffi dependency.
-- LLVM IR lowering (luc_frontend/src/ir_lowering.cpp) handles everything.
```

### Notes
- `luc_kernel.dll` is always pre-built before either mode runs. Neither the JIT nor the AOT compiler builds it on the fly.
- The JIT's dynamic linker holds module handles open for the entire session — `dlclose` is only called on interpreter shutdown.
- In interpreter mode, a C source file path in `@[link("wrapper.c")]` cannot be compiled on the fly — it must be pre-compiled to a `.so`/`.dll` first. `luc_kernel.dll` is always pre-compiled, so this restriction doesn't affect the engine's FFI layer.
- Both modes resolve symbols by exact name — the C symbol name in `lge_*.h` must match the name in the `@[foreign("C")]` declaration in `lge_ffi.lfi` exactly.

---

## 17. Arena Memory & the Shared `ArenaDescriptor`

### Introduction
The Lucid language has three memory regions — scope arena (stack), `#alloc` heap, and named arenas. Only named arenas can be safely shared with C because they are the only region whose lifetime and boundaries are unambiguous to both sides. The mechanism that makes this sharing safe without per-pointer tagging or runtime overhead is the `ArenaDescriptor` struct — a plain POD value that both Lucid and C read with identical layout.

This section covers how `ArenaDescriptor` is defined, how it flows through the kernel's C API layer, and the ownership check pattern both the kernel's C++ internals and external C code use to verify arena membership.

### Why `ArenaDescriptor` Instead of Per-Pointer Tags

The alternative — tagging each `*T` pointer with which allocator produced it — would require the type system to track allocator provenance through every assignment, function call, and return value. That is significant compiler complexity and either a hidden tag byte per pointer (runtime overhead) or a fully parameterized pointer type (language complexity). Neither is justified when the problem can be solved at the arena level instead of the pointer level.

An `ArenaDescriptor` puts the identity information on the region, not on individual pointers. A single range check — `is ptr inside [base, base+size)?` — answers the ownership question for every pointer into that arena simultaneously, with no per-pointer cost and no type system extension.

### The Struct — Lucid Side

`ArenaDescriptor` is a built-in POD struct defined by the language. It is not user-defined and not importable — it exists as a primitive the same way `uint32` or `bool` does:

```lucid
-- built-in — not written by the user
const ArenaDescriptor = struct {
    base *uint8   -- start address of the arena region (immutable after create)
    size uint64   -- total byte capacity      (immutable after create)
}
```

Two fields only. `base` and `size` are set once by `#arena_create` and never change for the lifetime of the arena. The allocation cursor (`used`) is tracked internally by the language processor and is intentionally absent from the descriptor — C has no legitimate reason to read or write it.

```lucid
-- create and use
let physics_arena ArenaDescriptor = #arena_create(2 * 1024 * 1024)   -- 2 MB
const bodies  *uint8 = #arena_alloc(physics_arena, uint8, 65536)
const shapes  *uint8 = #arena_alloc(physics_arena, uint8, 32768)

-- pass to C — descriptor travels with the data
LGE_Physics_LoadBodies(bodies, shapes, #addrof(physics_arena))

-- Lucid owns the lifetime
#arena_free(physics_arena)
```

### The Struct — C Side

The kernel's C header defines `LGE_ArenaDescriptor` with identical field order, types, and alignment. No padding is inserted between fields on any supported 64-bit platform — `*uint8` (8 bytes) followed by `uint64_t` (8-byte aligned) is naturally packed:

```c
// kernel/include/lge_arena.h
#pragma once
#include <stdint.h>

// Plain POD struct — identical layout to Lucid's ArenaDescriptor.
// base and size are immutable after LGE_Arena_Create().
// The allocation cursor is managed by the Lucid runtime — never touch it from C.
typedef struct {
    uint8_t* base;   // start address of the arena region
    uint64_t size;   // total byte capacity in bytes
} LGE_ArenaDescriptor;

// Ownership check — two comparisons, no hash lookup, no lock.
// Returns 1 if ptr falls within the arena region, 0 otherwise.
static inline int LGE_Arena_Contains(
    const LGE_ArenaDescriptor* arena,
    const void* ptr)
{
    return (uint8_t*)ptr >= arena->base &&
           (uint8_t*)ptr <  arena->base + arena->size;
}
```

### How Kernel Subsystems Use It

Each kernel subsystem that accepts arena-allocated memory from Lucid follows the same pattern: receive the data pointer(s) and the descriptor, assert ownership in debug builds, proceed safely:

```cpp
// physics_bridge.cpp — loading physics bodies from Lucid arena memory

void LGE_Physics_LoadBodies(
    const uint8_t*             bodies_data,
    const uint8_t*             shapes_data,
    const LGE_ArenaDescriptor* arena)
{
    // Debug: verify both pointers belong to the declared arena.
    // Compiles away under NDEBUG — zero cost in release builds.
    assert(LGE_Arena_Contains(arena, bodies_data) &&
           "bodies_data does not belong to the provided arena");
    assert(LGE_Arena_Contains(arena, shapes_data) &&
           "shapes_data does not belong to the provided arena");

    // Safe to proceed — memory is arena-owned, must not call free() on it.
    // The arena outlives this call; Lucid holds the descriptor.
}
```

### Lifetime Contract

```
ArenaDescriptor must outlive every C call that receives #addrof(arena).
The arena must not be #arena_free'd while C still holds pointers into it.
```

In practice:
- Declare `ArenaDescriptor` at the scope that owns the C interaction, never inside a loop
- Call `#arena_free` only after all C calls (and anything they spawn) have returned
- Arenas that span multiple frames (e.g. a physics world that lives for a whole scene) should be allocated at scene load and freed at scene unload

### Code Structure

```
kernel/
├── include/
│   └── lge_arena.h      -- LGE_ArenaDescriptor typedef + LGE_Arena_Contains inline

luc_runtime/
├── src/
│   └── arena.cpp        -- #arena_create / #arena_alloc / #arena_reset / #arena_free
│                           manages internal bump-pointer cursor (not in descriptor)
```

`lge_arena.h` is included by every kernel subsystem that accepts Lucid arena memory. `LGE_ArenaDescriptor` in C maps to `ArenaDescriptor` in Lucid with identical field layout — no marshaling needed when passing the descriptor across the FFI boundary.

---

*— End of initial structure. Sections will be expanded with full implementation detail, edge cases, and code listings as each system is built.*