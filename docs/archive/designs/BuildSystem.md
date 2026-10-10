# Lucid Engine — Build System & Directory Mapping

This document explains how the Lucid Engine project is organized and how the build process transforms source code into executable binaries.

---

## 1. Project Folder Logic

| Folder | Content Type | Purpose |
|:---|:---|:---|
| `kernel/` | **Source (C++)** | The core engine logic (Rendering, ECS, Physics). |
| `externals/` | **Source / Libs** | 3rd party code we depend on. |
| `build/` | **Temp Files** | **The "Construction Site."** Contains compiler cache, `.obj` files, and project solutions. Safe to delete. |
| `dev_build/` | **Binaries** | **The "Live Engine."** Contains the final `.exe` and `.dll`. Run the engine from here. |

### Why the Separation?
We keep `build/` and `dev_build/` separate for three critical reasons:
1.  **Workspace Hygiene:** The `build/` folder is full of compiler "junk" (thousands of `.obj` files). By sending final binaries to `dev_build/`, we create a clean, one-click environment for testing.
2.  **Safety (Delete with Confidence):** You can delete the `build/` folder at any time to fix compiler glitches without losing your engine assets or setup.
3.  **Deployment Simulation:** `dev_build/` is a mirror of what the final player will download. This ensures we don't accidentally "cheat" by using paths or files that only exist on your development machine.

---

## 2. The Dependency Flow

Our engine is built from three types of "Ingredients" in the `externals/` folder:

### A. Header-Only (Merged into Kernel)
*   **Examples:** GLM, VMA, miniaudio, nlohmann/json.
*   **How it works:** These are purely text files (`.h`, `.hpp`). When we compile `luc_kernel.dll`, the compiler copies the code from these headers directly into our binary. They do not appear as separate files in the `dev_build/` folder.

### B. Compiled Libraries (Linked into Kernel)
*   **Examples:** GLFW, Jolt Physics, RmlUI.
*   **How it works:** CMake compiles these as separate sub-projects. Their code is then "Linked" into `luc_kernel.dll`. 
    *   If linked **Statically**, they are merged into our DLL (like header-only).
    *   If linked **Dynamically**, they will appear as separate `.dll` files in `dev_build/bin/`.

### C. System SDKs (External to Repo)
*   **Example:** Vulkan SDK.
*   **How it works:** These are installed on your Windows OS. CMake finds them via environment variables and links our code against the system's Vulkan drivers.

---

## 3. Output Mapping (`dev_build/`)

When you run `cmake --build .`, the following mapping occurs:

| Source Target | Resulting File | Location |
|:---|:---|:---|
| `luc_kernel` | `luc_kernel.dll` | `dev_build/bin/[Config]/` |
| `LucidEditor` | `LucidEditor.exe` | `dev_build/bin/[Config]/` |
| `core_lib/` | (Symlinked Folders) | `dev_build/core_lib/` |

> **Note:** The `[Config]` folder (Debug or Release) is created by Visual Studio/MSBuild. In a final "Release" build, all these files are moved to the root of the game folder.

---

## 4. Maintenance
*   **To Clean the Project:** Delete the `build/` folder.
*   **To Update Externals:** Run `tools/download_externals.ps1` or `git submodule update`.
*   **To Test the Engine:** Always execute `./dev_build/bin/Debug/LucidEditor.exe`.
