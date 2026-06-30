# Implementation Plan 1 — Lucid Editor Shell (Native)

> **Scope:** Milestone 0 — build, run, and open a native desktop window with a Unity/Unreal-style dark workbench, file/folder open workflow, and a basic text view.  
> **Out of scope for now:** Visual mode, scene viewport (render-to-texture), LSP, extensions, full Monaco editor. Those come in later plans.

---

## 1. Goal

Build the **Lucid Editor** as a **single native process** — the same architectural model Unity and Unreal use:

| Layer | Unity / Unreal pattern | Lucid Editor (this plan) |
|:---|:---|:---|
| Process model | One native app | `LucidEditor.exe` + `luc_kernel.dll` |
| Editor chrome | Retained UI (UI Toolkit / Slate) | **RmlUI** (RML + RCSS) |
| Styling | Style sheets + theme tokens | **RCSS** + Lucid Design System |
| 3D view (later) | GPU texture inside a panel | Vulkan render-to-texture in RmlUI |
| Code editor (later) | Specialized widget | WebView2 + Monaco *or* Scintilla |
| Editor logic (later) | C# / C++ | **Luc** (hot-reloadable) |

### Why not Electron?

Electron is excellent for a standalone IDE (VS Code), but a poor fit for a **game engine editor**:

- Two renderers (Chromium + Vulkan) — heavy memory, awkward viewport embedding.
- Hard to share input, selection, and GPU resources with the scene view.
- Ships a full browser when you already have a C++ runtime.

**RmlUI** gives you CSS-like modern UI inside the same Vulkan app — the approach locked in by [MasterPlan Decision 14](MasterPlan.md#decision-14--unified-ui-architecture-rmlui--luc-bridge-).

**Visual mode** (node graph / design view) is deferred to **Implementation Plan 2**.

---

## 2. Target Outcome (Milestone 0)

When Milestone 0 is done, you should be able to:

1. **Build** the project with CMake + Visual Studio.
2. **Run** `LucidEditor.exe` and see a dark, professional workbench window.
3. Use **File → Open File…** to open a single file in a tab.
4. Use **File → Open Folder…** to open a workspace and see a file tree in the sidebar.
5. Click a file in the tree to open it in the editor area.

```
┌─────────────────────────────────────────────────────────────┐
│  File   Edit   View   Help                         ─  □  ×   │
├────┬──────────┬───────────────────────────────────────────────┤
│ 📁 │ EXPLORER │  main.luc                              ×    │
│    │          ├───────────────────────────────────────────────┤
│    │ ▼ src    │  fn main() {                                │
│    │   main   │      // file content shown here               │
│    │   utils  │  }                                          │
│    │          │                                               │
├────┴──────────┴───────────────────────────────────────────────┤
│  No folder opened                          Lucid Engine 0.1  │
└─────────────────────────────────────────────────────────────┘
```

For Milestone 0, the editor area uses a **simple scrollable text panel** (RmlUI `<textarea>` or equivalent). A full code editor (syntax highlight, minimap, LSP) lands in Milestone 1.

---

## 3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ LucidEditor.exe                                              │
│   • Win32 message loop                                       │
│   • Boot / shutdown luc_kernel.dll                           │
├──────────────────────────────────────────────────────────────┤
│ luc_kernel.dll                                               │
│   • GLFW or Win32 window (via RmlUI Vulkan backend)          │
│   • Vulkan RHI                                               │
│   • RmlUI context — workbench shell                          │
│   • File I/O + native open dialogs                           │
│   • (Later) Luc VM + editor scripts                          │
└──────────────────────────────────────────────────────────────┘
```

**Data flow for Open Folder:**

```
User clicks File → Open Folder
  → Win32 IFileOpenDialog (folder picker)
  → kernel stores workspace root path
  → scan directory tree (C++ std::filesystem)
  → push tree data into RmlUI (DataModel or manual element build)
  → sidebar re-renders
User clicks file in tree
  → read file from disk
  → set active tab + populate text panel
```

---

## 4. Repository Layout

New and existing folders touched by this plan:

```text
Lucid-Game-Engine/
├── kernel/
│   ├── include/
│   │   └── lge_api.h
│   └── src/
│       ├── core/
│       │   └── kernel.cpp              ← frame loop, subsystems
│       ├── ui/                         ← NEW
│       │   ├── rmlui_system.cpp        ← RmlUI init / shutdown
│       │   ├── workbench.cpp           ← load RML, tab + sidebar logic
│       │   ├── file_dialog.cpp         ← Win32 open file / folder
│       │   ├── file_tree.cpp           ← directory scan + tree model
│       │   └── theme.cpp               ← CSS variable injection
│       └── platform/
│           └── win32_dialogs.cpp       ← optional split for dialogs
├── resources/                          ← NEW: editor UI assets
│   └── ui/
│       ├── workbench.rml               ← shell layout (menu, sidebar, tabs)
│       ├── workbench.rcss              ← layout rules
│       └── theme_dark.rcss             ← Lucid design tokens
├── tools/
│   └── bootstrap_main.cpp              ← entry: init kernel, run loop
├── externals/
│   ├── rmlui/                          ← already in repo
│   └── glfw/                           ← already in repo
├── dev_build/bin/Debug/
│   └── LucidEditor.exe                 ← run target
└── docs/
    └── ImplementationPlan1.md          ← this file
```

Later, `engine/src/` (Luc scripts) will drive panels via the UI bridge. Milestone 0 keeps editor behavior in C++ until the RmlUI shell is stable.

---

## 5. Prerequisites

| Tool | Purpose |
|:---|:---|
| **Visual Studio 2022** | C++ compiler, MSBuild |
| **CMake 3.20+** | Build system (already used) |
| **Vulkan SDK** | GPU backend for RmlUI + future viewport |
| **Windows 10/11** | Win32 dialogs, WebView2 (Milestone 1) |

Verify:

```powershell
cmake --version
# Vulkan SDK: ensure VULKAN_SDK env var is set
echo $env:VULKAN_SDK
```

No Node.js or Electron required for this plan.

---

## 6. Build & Run

### First-time configure

From the repo root:

```powershell
cd C:\Users\TaiAx\Desktop\Lucid-Game-Engine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

### Daily workflow

```powershell
cmake --build build --config Debug
.\dev_build\bin\Debug\LucidEditor.exe
```

| Output | Location |
|:---|:---|
| `luc_kernel.dll` | `dev_build/bin/Debug/` |
| `LucidEditor.exe` | `dev_build/bin/Debug/` |
| UI assets (RML/RCSS) | Copied next to exe or loaded from `resources/ui/` |

> See [BuildSystem.md](BuildSystem.md) for why `build/` (temp) and `dev_build/` (binaries) are separate.

---

## 7. Step-by-Step Implementation

### Step 1 — Wire RmlUI into CMake

RmlUI is in `externals/rmlui/` but must be linked explicitly. FreeType is required for RmlUI fonts and is fetched automatically if not installed on the system.

Add to root `CMakeLists.txt`:

```cmake
# RmlUI + FreeType (auto-fetched) + Win32/Vulkan backend
if(EXISTS "${CMAKE_SOURCE_DIR}/externals/rmlui/CMakeLists.txt")
    # ... FreeType FetchContent + Freetype::Freetype alias (see CMakeLists.txt)
    add_subdirectory(externals/rmlui)
    set(RMLUI_BACKEND "Win32_VK")
    add_subdirectory(externals/rmlui/Backends)
    target_link_libraries(luc_kernel PRIVATE
        RmlUi::Core RmlUi::Debugger rmlui_backend_Win32_VK Vulkan::Vulkan)
endif()
```

The Win32/Vulkan backend sources are pulled in via the `rmlui_backend_Win32_VK` INTERFACE target (no manual copy into `kernel/src/ui/`):

- `RmlUi_Backend_Win32_VK.cpp`
- `RmlUi_Renderer_VK.cpp`
- `RmlUi_Platform_Win32.cpp`

Stub UI module (Step 2 expands this):

- `kernel/src/ui/rmlui_system.hpp`
- `kernel/src/ui/rmlui_system.cpp`

**Done when:** `cmake --build build --config Debug` succeeds.

---

### Step 2 — Boot sequence in `LucidEditor.exe`

Expand `tools/bootstrap_main.cpp`:

```cpp
// 1. LGE_GetAPI() — load kernel
// 2. kernel->Boot()   — init Vulkan, RmlUI, load workbench.rml
// 3. kernel->Run()    — poll events, update UI, render frame
// 4. kernel->Shutdown()
```

Expand `kernel/src/core/kernel.cpp` `Run()` loop:

```cpp
while (running) {
    if (!Backend::ProcessEvents()) { running = false; break; }
    context->Update();
    Backend::BeginFrame();
    context->Render();
    Backend::PresentFrame();
}
```

Use RmlUI's `Backend::Initialize("Lucid Editor", width, height)` from the Win32/Vulkan backend.

**Done when:** empty dark window opens and closes cleanly.

---

### Step 3 — Lucid Design System (theme tokens)

Create `resources/ui/theme_dark.rcss` with Unity-inspired tokens:

```css
:root {
    --bg-primary:   #1e1e1e;
    --bg-secondary: #252526;
    --bg-input:     #3c3c3c;
    --bg-hover:     #2a2d2e;
    --accent:       #007acc;
    --text-primary: #cccccc;
    --text-muted:   #858585;
    --border:       #3c3c3c;
    --font-size:    13dp;
    --radius:       4dp;
}
```

Load this stylesheet globally before `workbench.rcss`. All panels reference variables — never hardcode hex values in layout files.

**Done when:** window background and sidebar use consistent dark greys.

---

### Step 4 — Workbench layout (RML)

Create `resources/ui/workbench.rml`:

```xml
<rml>
<head>
    <link type="text/rcss" href="theme_dark.rcss"/>
    <link type="text/rcss" href="workbench.rcss"/>
</head>
<body id="workbench">
    <div id="menu-bar">...</div>
    <div id="body-row">
        <div id="activity-bar">...</div>
        <div id="sidebar">
            <div id="sidebar-header">EXPLORER</div>
            <div id="file-tree"></div>
        </div>
        <div id="main">
            <div id="tab-bar"></div>
            <div id="editor-area">
                <textarea id="editor-text" />
            </div>
        </div>
    </div>
    <div id="status-bar">
        <span id="workspace-label">No folder opened</span>
        <span id="engine-version">Lucid Engine 0.1</span>
    </div>
</body>
</rml>
```

Style with flexbox in `workbench.rcss` — RmlUI supports `display: flex`, fixed activity bar width (~48dp), sidebar (~260dp), and a flex-grow main area.

**Done when:** static layout matches the wireframe in Section 2.

---

### Step 5 — Menu bar + keyboard shortcuts

Implement **File** menu items in C++ (RmlUI buttons or native Win32 menu — native menu is fine for Milestone 0):

| Item | Shortcut | Action |
|:---|:---|:---|
| Open File… | `Ctrl+O` | Single-file picker |
| Open Folder… | `Ctrl+K Ctrl+O` | Folder picker |
| Exit | `Alt+F4` | Close app |

Use **IFileOpenDialog** with `FOS_PICKFOLDERS` for folders and `FOS_FILEMUSTEXIST` for files.

Implement in `kernel/src/ui/file_dialog.cpp`:

```cpp
std::optional<std::filesystem::path> OpenFileDialog(HWND owner);
std::optional<std::filesystem::path> OpenFolderDialog(HWND owner);
```

Wire menu clicks and shortcuts to these functions.

**Done when:** OS pickers open from the menu.

---

### Step 6 — File tree (Open Folder)

In `kernel/src/ui/file_tree.cpp`:

1. Recursively scan the chosen folder with `std::filesystem::directory_iterator`.
2. Skip hidden dirs (`.git`, `.cache`) and `build/`.
3. Build a flat or nested model: `{ name, path, is_directory, depth, expanded }`.
4. Populate `#file-tree` with clickable rows (indent by depth).

On row click (file):

- Read text via `std::ifstream`.
- Call `workbench::OpenTab(path, content)`.

On row click (folder):

- Toggle expand/collapse and refresh children.

Update `#workspace-label` with the folder name.

**Done when:** opening a repo folder shows its tree; clicking a file opens a tab.

---

### Step 7 — Tab bar

In `kernel/src/ui/workbench.cpp`:

- `OpenTab(path, content)` — add tab button to `#tab-bar`, store content map keyed by path.
- Active tab gets accent underline (`--accent` border).
- Click tab → swap `#editor-text` value.
- Close button (`×`) on tab → remove from bar; if last tab, clear editor.

**Done when:** multiple files can be open and switched.

---

### Step 8 — Copy UI assets to output directory

Add a CMake post-build step so RML/RCSS ship beside the exe:

```cmake
add_custom_command(TARGET LucidEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/ui"
        "$<TARGET_FILE_DIR:LucidEditor>/ui"
)
```

Load documents relative to exe: `ui/workbench.rml`.

**Done when:** running from `dev_build/bin/Debug/` finds styles without manual copying.

---

## 8. Smoke Test Checklist

- [ ] `cmake --build build --config Debug` succeeds
- [ ] `LucidEditor.exe` opens a dark workbench window
- [ ] Title bar shows **Lucid Editor**
- [ ] **File → Open File…** opens the OS file picker
- [ ] Selected file appears in a tab with correct text content
- [ ] **File → Open Folder…** opens the OS folder picker
- [ ] Sidebar shows folder tree with expand/collapse
- [ ] Clicking a file in the tree opens it in the editor area
- [ ] Tab switching and close (`×`) work
- [ ] Status bar shows workspace name after folder open
- [ ] Closing the window exits cleanly (no crash on shutdown)

---

## 9. Implementation Order

| # | Task | Done when |
|:--|:---|:---|
| 1 | Add RmlUI + Vulkan backend to CMake | Configure + compile |
| 2 | Kernel boot + empty RmlUI window | Dark window opens |
| 3 | Theme tokens (`theme_dark.rcss`) | Consistent dark palette |
| 4 | Workbench RML layout | Sidebar + tabs + status bar visible |
| 5 | Win32 Open File / Open Folder dialogs | Pickers work from menu |
| 6 | File tree scan + sidebar render | Folder tree displays |
| 7 | Tab bar + text panel | Multi-file open/switch |
| 8 | Copy `resources/ui` to output dir | Assets load from exe folder |
| 9 | Full smoke test (Section 8) | All boxes checked |

Each step must compile and run before moving to the next.

---

## 10. What Comes Next (Not This Plan)

| Milestone | Deliverable |
|:---|:---|
| **Plan 2 — Code editor** | WebView2 + Monaco *or* Scintilla; Luc syntax; `luc_langserver` LSP |
| **Plan 3 — Viewport panel** | Vulkan render-to-texture in RmlUI; scene / game tabs |
| **Plan 4 — Docking** | Split panes, drag-to-dock, saved layouts |
| **Plan 5 — Luc UI bridge** | Move panel logic from C++ to `engine/src/*.luc` |
| **Plan 6 — Visual mode** | Custom editor provider for `.lgraph`; Source ↔ Visual toggle |

---

## 11. Visual Mode (Placeholder)

Visual mode will be a **second editor view** for the same asset — similar to Unity's Shader Graph or Unreal Blueprints:

- **Source tab** — text (Monaco / Scintilla).
- **Visual tab** — node canvas in the editor area.

Registration via `api.workspace:register_editor_provider(".lgraph", ...)` (see MasterPlan). Detailed design goes in **Implementation Plan 2**.

No visual mode work until Milestone 0 is stable.

---

## 12. Troubleshooting

| Problem | Likely cause | Fix |
|:---|:---|:---|
| CMake can't find Vulkan | SDK not installed | Install Vulkan SDK; set `VULKAN_SDK` |
| Blank / white window | RML path wrong | Check `ui/workbench.rml` beside exe |
| Styles missing | RCSS not loaded | Verify `<link>` tags in RML head |
| RmlUI assert on shutdown | Context destroyed out of order | Shutdown: context → renderer → Backend |
| File picker returns nothing | COM not initialized | Call `CoInitializeEx` in main before dialogs |
| Tree empty after open | Path or scan filter bug | Log root path; check skip rules |
| High DPI blurry UI | DPI unaware | Use RmlUI Win32 backend DPI helpers |

---

## 13. Next Action

Start with **Step 1** — wire RmlUI into CMake and get an empty dark window:

```powershell
cd C:\Users\TaiAx\Desktop\Lucid-Game-Engine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\dev_build\bin\Debug\LucidEditor.exe
```

Then implement Steps 3–8 until the smoke test in Section 8 passes.

When Milestone 0 is done, proceed to **Implementation Plan 2** for the real code editor widget and Luc language support.
