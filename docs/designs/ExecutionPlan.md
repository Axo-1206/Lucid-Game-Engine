# Lucid Game Engine — Execution Plan & Roadmap

This document outlines the step-by-step implementation strategy for building the Lucid Game Engine. It translates the architectural decisions from `MasterPlan.md` into an actionable, chronological roadmap.

---

## 📊 Overall Project Status

| Phase | Description | Status |
|:---:|:---|:---|
| **0** | **Environment & Foundation** | 🟢 **In Progress** (Skeleton Ready) |
| **1** | **The C++ Bedrock** | ⚪ Pending |
| **2** | **The Luc Bridge** | ⚪ Pending |
| **3** | **The Engine Editor** | ⚪ Pending |
| **4** | **Advanced Tooling** | ⚪ Pending |

---

## Phase 0: Environment & Foundation Setup

**Goal:** Establish the physical repository structure, the build system, and integrate all external dependencies.

- [x] **1. Repository Initialization**
    - [x] Initialize Git (done).
    - [x] Create core folder structure: `kernel/`, `engine/`, `core_lib/`, `externals/`, `tests/`, `tools/`.
    - [ ] Set up `dev_build/` folder and establish symlink scripts for live development (Decision 29).
- [x] **2. CMake Build System Setup**
    - [x] Create root `CMakeLists.txt`.
    - [x] Configure targets: `luc_kernel` (DLL/SO) and `LucidEditor` (Bootstrap Executable).
    - [ ] Configure test targets (`tests/kernel/`).
- [ ] **3. Dependency Acquisition (`externals/`)**
    - [ ] Add **Vulkan SDK** & **VMA** (Vulkan Memory Allocator).
    - [ ] Add **GLFW** (Windowing) and **GLM** (Math).
    - [ ] Add **Jolt Physics**.
    - [ ] Add **RmlUI** and **NanoSVG**.
    - [ ] Add **miniaudio** and **cgltf**.
    - [ ] Add **LLVM Core** (for the LangServer and JIT components).
    - [ ] Add **nlohmann/json** and **MessagePack**.
    - [ ] Add **MbedTLS** (Security/Crypto).

---

## Phase 1: The C++ Bedrock (The Microkernel)

**Goal:** Build a high-performance, stand-alone C++ core that can open a window, render a triangle, and manage memory, independent of the Luc language.

- [ ] **1. Platform & Windowing**
    - [x] Implement `kernel.cpp` (Boot sequence boilerplate).
    - [ ] Implement `clock.cpp` (High-res delta time, V-Sync).
    - [ ] Implement `input_bridge.cpp` (GLFW abstraction).
- [ ] **2. The Vulkan RHI (Decision 27)**
    - [ ] Initialize Vulkan Instance, Physical Device, Logical Device.
    - [ ] Initialize Swapchain and VMA.
    - [ ] Implement Bindless Global Descriptor system.
    - [ ] Implement `vulkan_cmd_buffer.cpp` (Drawing abstraction).
- [ ] **3. The ECS Core (Decisions 1 & 28)**
    - [ ] Implement `world.cpp` (Entity registry, contiguous component stores).
    - [ ] Implement Doubly-Linked List `RelationshipComponent` system.
    - [ ] Implement System Scheduler.
- [ ] **4. The FFI & Module System (Decisions 2 & 5)**
    - [x] Define `lge_api.h` (The C-ABI Function Table).
    - [ ] Implement `module_manager.cpp` (LoadLibrary, Hot-Reload swapping, File Watcher).

---

## Phase 2: The Luc Bridge & Core Libraries

**Goal:** Connect the C++ Bedrock to the Luc programming language, allowing scripts to control the engine natively.

- [ ] **1. C++ to Luc Projections**
    - [ ] Implement `math.luc` (GLM projection).
    - [ ] Implement `io.luc` (GLFW input mapping).
- [ ] **2. The Component Suite (Decision 19 & 28)**
    - [ ] Define POD C++ components: `Transform`, `Mesh`, `Sprite`, `Audio`, `Physics`, `Collider`.
    - [ ] Create equivalent `@packed` struct definitions in Luc.
- [ ] **3. Subsystem Bridges**
    - [ ] Implement Physics Bridge (Jolt ↔ Luc `api.physics`).
    - [ ] Implement Audio Bridge (miniaudio ↔ Luc `api.audio`).
    - [ ] Implement Render Bridge (Vulkan RHI ↔ Luc `api.render`).
- [ ] **4. The Console Interpreter (Decision 20)**
    - [ ] Implement Tri-Level Dispatcher (`luc_console.cpp`).
    - [ ] Wire up Memory Map reflection reader (`symbols.json`).

---

## Phase 3: The Engine Editor (The UI Shell)

**Goal:** Build the actual Lucid IDE using Luc code and RmlUI.

- [ ] **1. Unified UI Kernel (Decision 14 & 15)**
    - [ ] Integrate RmlUI with Vulkan RHI.
    - [ ] Build `ui.luc` Reactive Bridge.
- [ ] **2. Workspace Manager**
    - [ ] Implement 4-Tab Bottom Panel (Terminal, Output, Problems, Console).
    - [ ] Implement IDE Docking and Tab management.
- [ ] **3. Scene & Inspector View (Decision 18)**
    - [ ] Implement `scene_view.luc` (Vulkan canvas hook + Jolt Raycasting).
    - [ ] Implement `inspector_panel.luc` (Auto-generating UI from ECS Reflection).
- [ ] **4. Extension Manager (Decision 7)**
    - [ ] Implement `extension.json` parser.
    - [ ] Implement Permission Gatekeeper (Kernel-level enforcement).
    - [ ] Build "Extensions" UI panel.

---

## Phase 4: Advanced Tooling & Polish

**Goal:** Implement complex developer tools, distribution systems, and final security passes.

- [ ] **1. Visual Programming (Decision 26)**
    - [ ] Update `luc_langserver.exe` to scan for `---@node` comments.
    - [ ] Implement Node Graph Canvas in Editor (bezier curves, drag-and-drop).
    - [ ] Implement `.lgraph` to `.luc` auto-compiler.
- [ ] **2. The Asset Pipeline (Decision 11)**
    - [ ] Build CLI `luc_asset_baker.exe`.
    - [ ] Implement GLTF to `.lmesh` conversion.
    - [ ] Implement Image to `.ltex` conversion.
    - [ ] Implement Audio to `.lsfx` / `.lstream` compression.
- [ ] **3. Security & Distribution (Decisions 10, 23, 24)**
    - [ ] Implement `license.lge` verifier and AES-256 Two-Step Derivation.
    - [ ] Implement `.pck` VFS packer/reader.
    - [ ] Implement Secure JIT export (`.ljit`) and AOT export (`.lmod`) pipelines.
- [ ] **4. SDK Packaging**
    - [ ] Write `tools/package_sdk.py` to bundle engine into `LucidSetup.exe`.
