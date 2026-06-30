# NOTE.md: Lucid Engine Architecture & UI Roadmap

This document outlines the architectural decisions and component divisions for the **Lucid Engine** project (authored by TaiAx), detailing the integration of UI frameworks, modular system design, and the development strategy for the `Lucid` programming language.

---

## 1. UI Framework Division of Labor

To achieve a modern visual aesthetic without sacrificing development velocity or rendering performance, the engine utilizes a split-UI architecture:

### Engine Tools & Visual Editor $\rightarrow$ Dear ImGui

* **Use Case:** Engine Workspace, Inspectors, Asset Browsers, and the Visual Programming Node Graph.
* **Implementation:** Paired with the `imgui-node-editor` extension.
* **Justification:** ImGui natively handles the heavy mathematical lifting of a visual scripting canvas (infinite panning, zooming, grid snapping, and node-link logic). Flat styling, dynamic node coloring, anti-aliased rounded corners, and embedding custom GPU textures via the Vulkan core fulfill the requirement for a clean, modern workspace layout without the massive infrastructure overhead of a retained DOM.

### In-Game User UI $\rightarrow$ RmlUI

* **Use Case:** Production-ready HUDs, player menus, and in-game UI layer.
* **Implementation:** HTML / RCSS templates.
* **Justification:** Provides absolute artistic freedom, responsive layouts for variable screen resolutions, and smooth web-style transitions completely independent of the core engine editor loop.

---

## 2. Modular Engine Architecture (Approach 2)

The project is decoupled into separate, highly focused modules rather than a single monolithic binary. This enables independent scaling and clean code separation.

```
                  +--------------------------------+
                  |       CORE ENGINE MODULE       |
                  |  (Vulkan Core, Compiler, Luc)  |
                  +---------------+----------------+
                                  |
         +------------------------+------------------------+
         |                                                 |
         v                                                 v
+------------------------+                        +------------------------+
|   EDITOR APPLICATION   |                        |   GAME VIEWPORT DLL    |
|   (LucidEditor.exe)    |                        |    (LucidRender.dll)   |
|                        |                        |                        |
|  * Dear ImGui UI       |   Spawns/Communicates  |  * RmlUI Runtime UI    |
|  * Visual Scripting    | <--------------------> |  * High-Perf Vulkan    |
|  * Node Translation    |   (DLL Load / IPC)     |  * Multi-Window Scene  |
+------------------------+                        +------------------------+

```

* **LucidEditor.exe:** Written purely using Dear ImGui. It manages project structures, serializes visual nodes, and invokes the compiler pipeline. It handles zero heavy game loop ticks.
* **LucidRender.dll (Viewport Module):** Houses the core Vulkan renderer. It can be dynamically loaded by the editor to spawn multiple, dedicated native OS windows for different scene perspectives (e.g., Perspective, Top-Down) and executes game logic via the scripting engine.

---

## 3. Lucid Language Design & Execution Model

`Lucid` serves as the structural replacement for `luc`, optimized heavily for procedural visual layout structures (80% procedural, 20% functional hybrid, featuring a TypeScript/Java-style module system).

### Core Strategy: The Two-Path Compilation Model

To balance **lightning-fast development iteration** with **maximum native performance**, a dual-execution strategy will be implemented:

```
[ User Node / Source Code Updates ]
                │
                ├───► PATH 1: VM Interpreter (Default Phase) ───► Instant Hot-Reload (In-Editor)
                │
                └───► PATH 2: C++ Transpiler (Optional Phase) ──► Pure Machine Code .exe (Final Ship)

```

#### Path 1: Bytecode Virtual Machine (The Foundational Default)

* **Mechanism:** The language compiles instantly into lightweight bytecode executed by a custom embedded stack/register-based Virtual Machine.
* **Hot-Reloading:** Allows users to modify code or visually link nodes in real-time while the game is running inside the viewport. The VM hot-swaps the active bytecode array in memory seamlessly.
* **Low-Level Access (FFI):** The interpreter functions as a dynamic linker. Core Vulkan rendering operations are registered as native function pointers inside the VM at startup. Custom user C++ code written on the fly is compiled to a temporary dynamic library (`.dll`/`.so`) and attached to the VM scope at runtime via OS loading APIs (`LoadLibrary` / `dlopen`).

#### Path 2: Source-to-Source C++ Transpiler (The Production Optimization)

* **Mechanism:** An optional standalone tool that acts as a transpiler. Because the language structure is inherently simple, C/Assembly-like, and heavily procedural, the tool cleanly maps `.lucid` source nodes line-for-line into native `.cpp` files.
* **Compilation:** Invokes a system compiler (Clang/MSVC) to bake the generated code directly into the engine's Vulkan core, outputting a highly optimized, raw machine-code binary entirely stripped of VM overhead for final shipping.

---