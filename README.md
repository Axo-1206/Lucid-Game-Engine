# Lucid Engine
### The VS Code for Game Development

**Lucid Engine** is a modern, high-performance game engine designed from the ground up to be the "VS Code of Game Development." It combines the raw power of a C++ systems kernel with the flexibility and elegance of the **Luc language**, providing a workflow that is fast, extensible, and developer-first.

---

## 🏗️ Architecture: The Microkernel Model

Lucid is built on a **Microkernel Architecture**. Unlike monolithic engines that are bloated and slow to iterate, Lucid separates the engine into two distinct layers:

1.  **The C++ Bedrock (Kernel):** A lightweight, high-performance core (built with Vulkan and Jolt Physics) that handles the "heavy lifting"—memory management, rendering, physics, and the Virtual File System (VFS).
2.  **The Luc Layer (Logic):** The entire IDE interface and all game-specific logic are written in **Luc**. This allows for near-instant iteration, hot-reloading, and a clean separation between engine stability and creative freedom.

---

## 🖋️ The Luc Language

**Luc** is the primary systems language for developing in Lucid. Designed to be C-compatible but with the safety and modern ergonomics of languages like Zig, it is the heartbeat of the engine.

*   **Performance:** LLVM-backed AOT and JIT compilation for native speed.
*   **Extensibility:** Write Extensions, tools, and game logic in the same high-level systems language.
*   **Compiler:** The Luc compiler is hosted in a separate repository.
    *   🔗 **Source:** [Luc-Compiler Repository](https://github.com/Axo-1206/Luc-Compiler.git)

---

## ✨ Key Features

### 🚀 VS Code-Inspired Workflow
The Lucid IDE is designed for productivity. With an integrated OS Terminal, structured Output Logs, real-time Problem Diagnostics, and a live **Engine Console (REPL)**, the feedback loop is instantaneous.

### 🧩 Data-Oriented ECS
Lucid uses a high-performance **Entity-Component-System (ECS)** model. Components are plain POD structs stored in contiguous memory, ensuring maximum cache efficiency and seamless parallel execution.

### 🔒 Security-First Design
Every asset in Lucid is protected. The engine uses an **AES-256 Encrypted VFS** and a **Two-Step DRM model** (Title Key + Hardware Ticket). Extensions are cryptographically signed using Ed25519 to ensure identity and integrity across the ecosystem.

### 💎 Professional Asset Pipeline
No parsing at runtime. Lucid "cooks" source assets (GLTF, PNG) into **GPU-ready binary formats** (`.lmesh`, `.ltex`) that are loaded via zero-dependency `memcpy` operations.

### 🔌 VS Code-Style Extension Ecosystem
Lucid treats extensions as first-class citizens. Developers can build editor tools, visual debuggers, and custom network providers using a local JSON-driven extension system. Every extension is isolated, declares explicit permissions (like `ecs.write` or `network.client`), and is distributed securely via signed `.lucext` packages.

### ⚙️ Dual Compilation: AOT & JIT
The Luc compiler provides unmatched deployment flexibility:
*   **AOT (Ahead-of-Time):** Compiles directly to a highly optimized native binary for a specific target platform (e.g., Windows `.exe`), maximizing runtime performance for demanding games.
*   **Secure JIT (Bytecode):** Compiles to an LLVM-backed bytecode format. This allows indie developers to package their game once and run it securely across multiple platforms without needing native cross-compilers for Mac or Linux.

---

## 🛠️ Getting Started

Lucid is currently in active development. 

*   **Engine Core:** Written in C++ (Vulkan, Jolt, LLVM).
*   **Editor & Tooling:** Written in Luc.
*   **Standard Library:** Shipped with the `core_lib/` to handle math, input, and networking.

For more detailed technical specifications, please refer to the documentation in the `docs/` folder.