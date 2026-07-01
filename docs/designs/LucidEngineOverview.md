# Luc Game Engine: Development & Architecture Plan

This document outlines the strategic plan for building the **Luc Game Engine**, an extension-first, high-performance game engine using a hybrid C++/Luc architecture.

## 1. Core Architecture: The "Microkernel" Model
The engine follows a "Thin Core" or "Microkernel" strategy, utilizing C++ for hardware-level performance and **Luc** for high-level logic and extensions.

* **C++ Kernel (The "How"):** * **Vulkan RHI:** Manages the Render Hardware Interface, memory allocation (via VMA), and synchronization.
    * **OS Bridge:** Handles windowing, input events, and file system access.
    * **FFI Layer:** Exposes a stable C-ABI interface for Luc to call into.
* **Luc Layer (The "What"):**
    * **Game Logic:** Primary language for developers.
    * **Extension System:** Uses a VS Code-style architecture where extensions add functionality.
    * **IDE & UI:** Built using a custom UI library written in Luc.


- Performance Layer (C++): A pre-compiled Kernel (luc_kernel.dll / .so) built with C++. It handles the "heavy" lifting: Vulkan RHI, GLFW/SDL2 for windowing, and the Physics bridge.

- Logic Layer (Luc): The "Soul" of the engine. Users write scripts in Luc, and the IDE itself is built using Luc.

- The Bridge (FFI): A stable C-ABI that allows Luc to call Kernel functions with zero-cost overhead.

## 2. Graphics: Vulkan Integration
The engine utilizes the Vulkan API for modern, explicit control over the GPU.

* **Vulkan Loader:** The engine interacts with `vulkan-1.dll` (standard on Windows systems).
* **Abstraction:** The C++ core handles the verbose setup (Swapchains, Pipelines, Command Pools), while Luc utilizes FFI to trigger high-level draw commands.
* **Direct Access:** Luc modules can eventually map `vulkan-1.dll` signatures directly for power users who require raw Vulkan access.

- Custom UI System: A snapping, docking IDE (Unity/VS Code style) built with a custom library.

    - Editor: Uses snapping math and docking panes for the "Studio" experience.

    - Game: Uses a lightweight batch-renderer for 2D interfaces.

- Visual Programming: A drag-and-drop node system (like draw.io) that programmatically generates Luc source code.

## 3. The Custom UI Library & Visual Programming
A key feature of Luc Engine is its integrated Visual Programming environment (similar to draw.io).

* **UI Foundation:** A custom library designed for both the Engine IDE and the games themselves.
* **Aesthetics:** Support for **SDF (Signed Distance Field) Fonts** for crisp text and vector-based layout (Flexbox style).
* **Visual Logic:** Drag-and-drop nodes generate Luc source code. Nodes represent Directed Acyclic Graphs (DAGs), where processes are connected by data flows.

## 4. Time, Frames, and Synchronization
The engine heartbeat is managed by a high-resolution clock in the C++ Kernel.

* **Delta Time:** Calculated every frame to ensure consistent physics across different hardware.
* **Execution Flow:** 1. Input Polling
    2. Luc Script Update (passing DeltaTime)
    3. Physics Step
    4. Vulkan Rendering
* **Pacing:** Support for V-Sync (FIFO) and Mailbox modes to manage latency and tearing.

## 5. Development Roadmap & Bootstrapping
To manage the simultaneous development of the engine and the Luc compiler:

1.  **Phase 1 (C++ Bedrock):** Build the functional kernel and Vulkan RHI in C++. Compile this into `luc_kernel.dll`.
2.  **Phase 2 (Luc Bridge):** Ship the engine with the C++ DLL and the current `luc_compiler.exe`. Build the Visual Programming UI in Luc.
3.  **Phase 3 (Evolution):** Gradually rewrite C++ modules (like Math or File Systems) into Luc as the compiler stabilizes.

* **The Pre-Processor Contract (Kernel Side)**
When you build your C++ Kernel, you use Pre-processor Macros. This allows you to write one piece of code that automatically exports the correct format depending on the OS you are currently compiling on.

```c++
#ifdef _WIN32
    #define LGE_API __declspec(dllexport) // For Windows .dll
#else
    #define LGE_API __attribute__((visibility("default"))) // For Linux .so
#endif

extern "C" LGE_API void LGE_RenderFrame() { 
    // Logic here
}
```

* **The Extension Contract (Engine/Luc Side)**

Your `engine.exe` (on Windows) or `engine` (on Linux) needs to decide which library to load. You can handle this in your Configuration File or via a small bit of Startup Logic.

Instead of hardcoding `luc_kernel.dll`, the engine can look for a file named `luc_kernel` and append the platform-specific extension automatically.

```c++
string libName = "luc_kernel";

#ifdef _WIN32
    libName += ".dll";
#elif __linux__
    libName += ".so";
#endif

void* handle = LoadLibrary(libName); // Or dlopen on Linux
```

* **Summary of the Cross-Platform Handshake**

| System | Windows Contract | Linux Contract |
| --- | --- | --- | 
| File Format | Portable Executable (PE) | ELF |
| Library Ext | .dll | .so |
| Loading Call | LoadLibrary() | dlopen() |
| Symbol Lookup | GetProcAddress() | dlsym() |

## 6. Distribution & Deployment
* **Standalone Compiler:** The Luc compiler is bundled with the engine, ensuring all users have the same environment.
* **No External Dependencies:** Users do not need to install C++ compilers (like MSVC). The engine ships pre-compiled binaries for the core.
* **Extension Discovery:** A `/extensions` directory with a manifest system allows for dynamic module loading.

---
**Project Name:** Luc Game Engine  
**Core Language:** Luc  
**Implementation Language:** C++ (Kernel), Luc (High-level)  
**Graphics API:** Vulkan  
luc_game_engine_plan.md
Displaying luc_game_engine_plan.md.

- LucEngine_Root: Your private workspace containing the C++ Kernel source, the Luc compiler source, and automation scripts.

- LucEngine_Release: The distribution folder given to users.

    - engine.exe: The main IDE.

    - luc_kernel.dll: The compiled engine core.

    - compiler.exe: The bundled Luc compiler (no external C++ tools required).

    - /user_extensions & /user_projects: Folders for the extension ecosystem.

## The Development Workspace

LucidEngine_Root/
├── kernel_src/                 # C++ Source code for the .dll
│   ├── build/                  # Compiler output (Visual Studio/CMake)
│   └── include/                # Header files for FFI
├── externals/                  # External libs (Vulkan headers, VMA, etc.)
├── scripts/                    # Automation scripts (e.g., "copy_to_release.py")
└── tools/                      
    └── compiler.exe            # A COPY of your Luc compiler binary


To tell the engine where your compiler is, you need to establish a "Path Contract." Since the engine and compiler are separate projects, you shouldn't hardcode a path like C:\Users\TaiAx\Documents\... because it will break if you move the folder or give it to a friend.

Instead, you use a Relative Path based on where the engine.exe is sitting

1. The Configuration File
The most professional way to handle this is a config.luc or engine_settings.json file located in the same directory as your engine executable.

```json
{
    "compiler_path": "./compiler.exe",
    "project_root": "./user_projects/",
    "extension_root": "./user_extensions/",
    "api_version": "1.0.0"
}
```

2. How the Engine "Finds" the Compiler
In your C++ Kernel code, you will use the Working Directory to find the compiler. Here is the logic you would implement in C++:

2.1 Get Executable Path: Get the path of the running engine.exe.

2.2 Append the Relative Path: Combine that path with the "compiler_path" from your config file.

2.3 Check Existence: Use std::filesystem::exists() to make sure the compiler is actually there.

```c++
#include <filesystem>
namespace fs = std::filesystem;

// Get the folder where engine.exe lives
fs::path engine_dir = fs::current_path(); 
fs::path compiler_executable = engine_dir / "compiler.exe";

if (fs::exists(compiler_executable)) {
    // Engine now knows it can call the compiler!
} else {
    // Show error: "Luc Compiler not found in engine directory."
}
```

3. Running the Compiler from the Engine
When the engine needs to compile a script (e.g., when a user hits "Save" in your Visual Programming UI), the engine doesn't "open" the compiler; it spawns a child process.

You will use a system call (like CreateProcess on Windows or system() for a simpler test) to run a command line internally:

`compiler.exe --input ./user_projects/test.luc --output ./build/test.dll`

## The "Folder Release" (Distribution)

LucidEngine_v1.0/
├── luc_engine.exe              # The main launcher/IDE
├── luc_kernel.dll              # Your compiled C++ logic
├── luc_compiler.exe            # The "Roslyn-style" bundled compiler
│
├── core_lib/                   # Standard Luc library files
│   ├── vulkan.luc              # Luc definitions for FFI calls
│   └── math.luc                # Vector/Matrix math in Luc
│
├── core_extensions/             
│   ├── visual_editor/          
│   └── theme_manager/
│
├── user_extensions/ 
│   ├── Render.luc         
│   └── Color.luc          
│
├── user_projects/              # Where the user's games live
│   └── MyFirstGame/
│       ├── src/                # .luc scripts and visual graphs
│       └── assets/             # Textures, sounds for this project
│
├── images/                     # Engine-level UI icons and textures
│
└── logs/                       # Engine crash logs and compiler output

## Studio interface

### 1. The Editor Layout Architecture
Professional editors use a Workspace Manager. Instead of hardcoding where the "Scene View" or "File Explorer" goes, you create a system that manages "Panes."

- The Main Viewport (Vulkan): This is the center window where the game renders.

- The Docking Layer: A UI overlay that allows windows to be pinned to the left, right, or bottom.

- The Menu Bar: A top-level persistent strip for File, Edit, View, and Tools.

### 2. Implementing the "Settings" & "Theme" System
Since you want to change script colors and editor themes, you should store these in a Theme Configuration file (likely in the images/ or a new config/ folder).

- The Palette: Define a Theme struct in your C++/Luc code that holds colors for:

    - `Background_Primary` (The dark grey of the panels)

    - `Accent_Color` (The blue/orange highlights)

    - `Syntax_Keyword`, `Syntax_String`, `Syntax_Comment` (For your script editor)

- Live Update: Because your UI is custom, you can make these changes "live." When the user picks a new color in the settings menu, you update the global Theme object, and every UI component redraws itself using the new color.


### 3. The "Scene Window" vs. "Game Window"
In Unity/Unreal, the Scene Window is different because it has Grid Lines, Gizmos (the XYZ arrows to move objects), and a Free-fly Camera.

- Two Pipelines: You will likely have two Vulkan render passes.

    - Game Pass: Renders exactly what the player sees.

    - Editor Pass: Renders the game PLUS the "Editor UI" (Selection outlines, Light icons, Camera paths).

- The Gizmo System: This is a specialized piece of math. You’ll need to handle Raycasting—when the user clicks the 3D scene, the engine must calculate if the mouse hit a node or an object.

### 4. Menu & Commands (The VS Code Approach)
To make the engine feel "massive" and extensible, use a Command Registry.

- Don't hardcode the File > Save button logic.

- Do create a system where an extension can say: "Register a new menu item 'Visual Debugger' under the 'View' menu."

When that menu item is clicked, it sends a "Signal" (an event) that your Luc scripts can listen for. This allows your "Visual Programming" extension to add its own settings directly into the engine's main menu.

### 5. UI Implementation SuggestionsSince you are writing your own UI library to handle this:

| Feature | implementation Strategy |
| --- | --- |
| Docking | Use a "Split-Pane" container logic. Each pane has a % width/height. | 
| Top Bars | A simple horizontal list of buttons with dropdown state. |
| Scene Window | A Vulkan Texture rendered inside a UI panel (Render-to-Texture). | 
| Settings Menu | A generated list of properties (sliders, color pickers, text inputs). | 


