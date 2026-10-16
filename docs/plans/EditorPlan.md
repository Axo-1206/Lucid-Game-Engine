# Editor plan

What the Lucid Editor is, what it is not, and the order it gets
built in. This note is the anchor for every editor decision. When a
question comes up that this note does not answer, answer it here
first, then implement.

See also:
- `docs/notes/EditorMetadata.md` — what the engine exposes for the
  editor to read.
- `docs/notes/PackageNaming.md` — the naming convention the editor
  must not parse.
- `docs/notes/Dataflow.md` — the runtime model the editor sits
  beside.
- `docs/notes/RenderPasses.md` — the pass model the editor reuses.
- `docs/notes/SceneFormat.md` — the scene format the editor will
  eventually edit.

---

## 1. What the editor is

The Lucid Editor is a **developer IDE for a code-first, package-
driven engine**. It is not a Unity-style scene editor with a script
editor bolted on. It is the other way around: it is a script and
package tool that will eventually grow a scene view.

The order matters. The engine's shape decides it:

- The engine is **package-driven**. Packages register nodes,
  components, structs, editor metadata. The editor's job is to
  read that metadata and present it.
- The engine is **node-centric**. The unit of behavior is a node in
  a `.lucid` graph, not a MonoBehaviour. The editor's core feature
  is the script editor.
- The engine is **table-mediated**. The scene is data; the graph is
  behavior. The editor can be useful for months before it can draw
  a scene.

So the editor is built in this order:

1. **Script editing** — open, edit, save, compile, show errors.
2. **Package browsing** — see what is loaded, read metadata.
3. **Console** — read the engine's log.
4. **Project management** — open, create, switch, persist.
5. **Visual graph editor** — edit `.lucid` graphs visually.
6. **Scene view** — edit the world's rows.

Steps 1–4 are phase 1. They are the whole product for a while.

The six items above are the priority order. The panel inventory
in §3 is the fuller list of what phase 1 builds.

---

## 2. Decisions

These are settled. Do not relitigate them without a new entry here.

### Decision 1 — ImGui + OpenGL, phase 1

The editor uses **Dear ImGui** with the **GLFW** and **OpenGL3**
backends. It is not a custom UI, not Qt, not Avalonia.

**Why:** the editor's work is forms, trees, text, and lists. ImGui
is excellent at internal tooling, and the editor is internal
tooling first. The engine already owns the render loop, so ImGui
is one more pass in the `View` list.

**Re-evaluate when:** the editor ships to non-developers, or needs
multi-window, native menus, accessibility, or OS-level polish.
Not before.

### Decision 2 — ImGui is vendored, not fetched

ImGui lives at `apps/editor/third_party/imgui/` and is committed
to the repo. It is not a `FetchContent` dependency.

**Why:**

- ImGui has no build system. Upstream expects the `.cpp` files to
  be added to the consumer's target.
- ImGui is editor-only. The engine and the game do not link it.
  Vendoring under `apps/editor/` keeps the dependency scoped and
  the root CMake free of editor concerns.
- ImGui is meant to be patched (`imconfig.h`, backend tweaks). A
  fetched copy in `build/_deps/` loses patches on the next clean
  build.
- ImGui has no transitive dependencies. The GLFW backend uses
  GLFW (already present); the OpenGL3 backend uses GLAD (already
  present). `FetchContent` gains nothing.

**Consistency:** the repo already vendors GLAD, stb, and
nlohmann/json — single-tree, no-build-system, source-drop
dependencies of the same shape. ImGui is the editor-side
equivalent.

### Decision 3 — Projects require a sentinel file

A folder is a Lucid project if and only if it contains
`lucid.project.json` at its root. The editor refuses to open any
other folder.

**Why:** opening an arbitrary folder gives the editor nothing to
show and the engine nothing to load. Godot and Unity both use a
sentinel file for the same reason. A loud refusal is better than
an empty editor.

**The file:**

```json
{
  "name": "MyGame",
  "format_version": "1",
  "engine_version": "0.1.0",
  "packages": [
    "engine.core",
    "engine.math",
    "engine.render"
  ],
  "active_scene": "scenes/main.lscene",
  "active_camera": "main"
}
```

`packages` is the project's package list. `engine_version` lets
the editor refuse a project from a newer engine. `active_scene`
and `active_camera` are the editor's convenience; both may be
empty.

### Decision 4 — User data splits: global and project-local

**Global (machine-local):**

- Windows: `%APPDATA%/Lucid/`
- Linux: `$XDG_CONFIG_HOME/Lucid/` or `~/.config/Lucid/`
- macOS: `~/Library/Application Support/Lucid/`

Contents:

```
settings.json          editor preferences
recent_projects.json   recent projects list
layouts.json           window/dock layout
themes.json            user themes
```

**Project-local:**

```
MyGame/
    lucid.project.json
    .lucid/
        editor.layout.json     per-project layout override
        cache/                 symbol index, file hashes
        generated/             build output the editor produces
```

**Rule:** recent projects, global preferences, and user themes
are machine-local. Layout has a global default and a per-project
override. Caches are project-local. `.lucid/` is gitignored by
default.

### Decision 5 — The editor compiles; the engine deserializes

Two pipelines, two processes, one file crossing between them.

**Compile pipeline (editor-driven, build-time):**

```
.lucid source (in-memory buffer)
    → engine::check_source  /  engine::compile_to_bytes
        (the engine's wrapper; see Decision 23)
    → the engine calls lucid::sema::compile(source, filename, registry)
    → CompileResult { ok, graph, diagnostics }
    → if ok: lucid::serialization::serialize → .lucgraph bytes
    → if !ok: the engine translates to engine::CompileDiagnostic
               and returns it; the editor surfaces it
```

**Deserialize pipeline (engine-driven, runtime):**

```
.lucgraph bytes
    → lucid::serialization::deserialize(bytes, registry)
    → DeserializeResult { ok, graph, diagnostics }
    → if ok: GraphRuntime holds the graph, ticks it
    → if !ok: log diagnostics (file-level, no source location)
```

The editor **does not** include a `lucid/*` header. It calls
`engine::check_source` and `engine::compile_to_bytes`, which
return `engine::CompileCheckResult` and `engine::CompileBuildResult`.
The engine translates the format library's diagnostics into
`engine::CompileDiagnostic`. The editor produces its diagnostics
by calling the engine on the in-memory buffer, in its own process;
the engine calls the format library in-process. The engine's
runtime calls `lucid::serialization::deserialize` directly on the
`.lucgraph` on disk, in its own process, because the engine is the
format library's consumer and its own code may include `lucid/*`.

**The only artifact that crosses the process boundary is the
`.lucgraph` file.** No diagnostics cross. No subprocess. No JSON
interchange.

**Consequence for the build action (phase 2):** a
`compile_project` function will be added in phase 2. It will
walk `scripts/`, call `engine::compile_to_bytes` for each file,
and write the `.lucgraph` next to the source. The editor's Build
button and a future `lucid build` CLI will both call it — one
implementation, two front ends. It does not exist in phase 1.

### Decision 6 — Two diagnostic streams, two panels

The editor has two diagnostic streams, and they go to two panels.

**Compile diagnostics** (from `engine::check_source` and
`engine::compile_to_bytes`, as `engine::CompileDiagnostic`): file,
line, column, severity, code, message. These go to:

- The **Problems panel** — structured, filterable, sortable,
  click-to-jump.
- **Inline markers** in the script editor's gutter.

**Runtime diagnostics** (from `engine::log_*`, which the engine
emits at runtime): no source location, or a file-level location
for `Ser_*` codes. These go to:

- The **Console panel** — the log ring buffer.
- The **Diagnostics panel** (phase 2) — structured, for package
  load, registry conflicts, scene load errors.

The two panels do not share data. A `Ser_FingerprintMismatch` from
a failed `deserialize` appears in the Console, not as an inline
squiggle. A `Type_Mismatch` from a compile appears in the Problems
panel, not in the Console's runtime log.

### Decision 7 — Node palettes read metadata, never parse names

The editor never parses a node type name. Every piece of UI
metadata comes from the registry and the editor metadata:

- `NodeTypeInfo` from the registry — kind, argument types, result
  type, category.
- `NodeEditorInfo` from the package — display name, documentation,
  icon, example, operation, operation group.
- `Operation` from `engine/registry/operation.h` — the node's
  operation and its visual group.

The name `Engine_Add_Vector2_Vector2_Node` is a convention for
human readers. The editor does not split it, prefix-strip it, or
infer anything from it. A package whose names do not follow the
convention still gets a correct palette entry.

### Decision 8 — The console exists before the scene view

The engine is code-first. The console is more valuable than a
scene view for a long time, and it is far cheaper to build. It is
a phase-1 panel. The scene view is a phase-4 panel.

### Decision 9 — Phase 1 compiles against engine packages only

The editor constructs a `PackageManager`, loads the engine's
default packages (`CorePackage`, `LogicPackage`, `MathPackage`,
`RenderPackage`, `UiPackage`), and calls
`apply_registration_only()`. Its `engine::check_source` and
`engine::compile_to_bytes` calls use that registry.

A user package (`alice.math`) is listed in the package panel if
it is in the project file, but it is **not compilable in the
editor** until a package-loading mechanism exists. A `.lucid`
file that imports a user package fails to compile in the editor.

**Resolved by:** a future mechanism for the editor to load user
packages (dynamic library, project-specific editor build, or a
package manifest the editor can execute). None exists today.

### Decision 10 — Asset references are paths; a GUID mechanism is reserved

Scene references today are paths:

```json
{ "texture_path": "textures/player.png" }
```

Paths break when files move. Unity uses GUIDs; Godot uses `res://`
paths. The engine does not implement GUIDs today, but the editor
reserves the shape:

- A `.meta` file next to each asset, or
- A project-level `assets.json` mapping path → id.

The decision is deferred; the reservation is not. Do not design a
scene format that cannot carry a GUID later.

### Decision 11 — Git is deferred to phase 5

The editor does not integrate Git in phase 1. When it does:

- Phase 5a: current branch in the status bar, dirty-file dots in
  the Explorer, via `git status --porcelain` on a timer.
- Phase 5b: stage, commit, diff, log.
- Phase 5c: branch switch, blame, merge conflict UI.

Git is not on the critical path to a working script editor.

### Decision 12 — The editor links the engine; the engine never links the editor

The dependency direction:

```
lucid_ecs
    ↑
luc_engine  ────────┐
    ↑               │
luc_editor          │
    ↑               │
lucid_editor.exe    │
                    │
lucid_game.exe ─────┘
```

- `luc_engine` provides the registry, the renderer interface, the
  scene loader, the `View` type, the log, and the script compiler
  wrapper (`engine::check_source`, `engine::compile_to_bytes`). It
  has no editor headers and no ImGui.
- `luc_editor` links `luc_engine` and ImGui. It provides the
  `EditorApp`, the panels, the project system.
- `lucid_game` links `luc_engine` and the game's packages. It has
  no editor headers and no ImGui.
- The engine provides tools any app can use. The editor is one
  consumer; the game is another.

**ImGui lives in `apps/editor/third_party/`.** It is an editor
dependency, not an engine one. The engine's `Renderer` interface
has no ImGui concepts.

### Decision 13 — Recompile is debounced on edit, immediate on save

The script editor recompiles the active buffer:

- **On save**, immediately.
- **On edit**, debounced by ~300 ms after the last keystroke.
- **On open**, once.

Compilation happens on the main thread in phase 1. The format
library is fast enough for hand-written `.lucid` files. If a file
ever stutters the UI, move the compile to a worker thread and post
the result back — the `Diagnostic` struct is a value, so it is
trivially movable.

### Decision 14 — `Project` is a value, not an owner of engine state

`Project` describes a project on disk. It does not own a
`PackageManager`, a `World`, a `Renderer`, or a `Window`. The
editor's `EditorApp` owns those; `Project` just tells it which
folder to look at and which packages to load.

### Decision 15 — The project file is JSON with a versioned schema

`lucid.project.json` carries `format_version`. The editor refuses
a project whose `format_version` is newer than what it
understands. This mirrors the `.lscene` version policy: accept
older, refuse newer.

### Decision 16 — `Project::open` refuses anything that isn't a project

No "open folder" fallback. No partial open. If
`lucid.project.json` is missing or malformed, `open` returns
`nullptr` and the launch panel shows the error. This is the
decision from §Decision 3, restated at the code level.

### Decision 17 — `Project::create` writes the whole skeleton

Creating a project makes `lucid.project.json`, `assets/`,
`scenes/`, `scripts/`, `packages/`, `.lucid/`,
`.lucid/cache/`, `.lucid/generated/`, and `scenes/main.lscene`.
The default scene has one orthographic camera and no entities.
An empty project is a valid project.

### Decision 18 — The project owns nothing at construction

`Project`'s constructor is private. The only ways to get one are
`Project::open` and `Project::create`. This makes it impossible
to have a `Project` that doesn't correspond to a real folder on
disk.

### Decision 19 — Machine-local state lives in the OS user folder

Global editor settings, recent projects, themes, and layouts live
in a single machine-local directory:

```
Windows:  %APPDATA%/Lucid/
Linux:    $XDG_CONFIG_HOME/Lucid/ or ~/.config/Lucid/
macOS:    ~/Library/Application Support/Lucid/
```

The path is computed by `editor::paths::global_config_dir()`. The
function does not create the directory; the caller
(`SettingsStore`) creates it on write.

If the environment variable is unset, the function falls back to
the platform's default. If that is also missing, it returns
`./lucid_editor_config/` in the current working directory and the
editor logs a warning at startup.

### Decision 20 — Settings loads never fail

`SettingsStore::load_global` and `load_recent` return a
default-constructed value on any error: missing file, malformed
JSON, wrong field type, I/O error. The editor starts with
defaults rather than refusing to start because a user's settings
file is corrupt.

`save_global` and `save_recent` return `bool` and an
out-parameter error. The caller reports the failure. A save that
fails is a user-visible problem; a load that fails is not.

### Decision 21 — The project file is the only sentinel

A folder is a Lucid project if and only if it contains
`lucid.project.json`. `Project::open` refuses anything else.
There is no "open folder" fallback.

This was Decision 16 in code; this decision restates the
sentinel rule for the settings side. The recent-projects list
stores paths to project folders, not to arbitrary folders; a
stale entry whose project file has been deleted is dropped on
the next successful open, or logged as missing when the user
clicks it.

### Decision 22 — `Settings` and `RecentProjects` are plain data

Both are value types with public fields and no behavior beyond
defaults. `SettingsStore` is a stateless collection of static
functions. There is no `SettingsManager` object and no observer
pattern. A caller that wants to react to a settings change reads
the value again; a caller that wants to change a setting modifies
a `Settings`, calls `save_global`, and notifies the panels that
care. This is small and sufficient for phase 1; if the editor
grows a need for change notification, that is a phase-2 decision.

### Decision 23 — The editor calls the engine's script compiler wrapper

The editor does not include a `lucid/*` header and does not call
`lucid::sema::compile` directly. It calls two engine functions:

```
engine::check_source(source, filename, packages)
    -> CompileCheckResult { ok, diagnostics }

engine::compile_to_bytes(source, filename, packages)
    -> CompileBuildResult { ok, diagnostics, bytes }
```

Both return `engine::CompileDiagnostic` (an engine type). The
engine translates the format library's diagnostics behind the
wrapper.

Rationale: the editor's dependency is on the engine, and only the
engine. The format library is an implementation detail of the
engine from the editor's point of view. The wrapper is about 100
lines and it makes the boundary explicit.

The rule for future wrappers: the engine wraps a format-library
entry point if and only if (a) a non-engine caller exists or is
committed, and (b) the wrapper's signature contains no `lucid::*`
type. Wrap what has a caller, not what might have one. `format`,
`parse`, and `deserialize` are not wrapped today; add
`engine::format_source` when the editor gets format-on-save, and
`engine::list_symbols` when the editor gets symbol search.

This decision is recorded in `docs/Decision.md` as Decision 19
(engine-side). This entry is the editor-side counterpart; the two
documents have independent numbering.

### Decision 24 — The launch panel and main shell are the first UI

The editor has two modes:

- **Launch** — no project loaded. A single full-viewport panel
  with a recent-projects list, "Open Project", "New Project",
  and "Quit". The user is choosing what to open.
- **Project** — a project loaded. A menu bar, a toolbar, a
  full-viewport dockspace, and a status bar. The panels that
  fill the dockspace arrive in later commits.

`EditorApp` owns the mode and the transition. `LaunchPanel` and
`MainShell` are panels; they draw and call back, they do not
own the project or the settings.

**File dialogs.** "Open Project" and "New Project" use an ImGui
modal with a text field, not a native file dialog. Native dialogs
are a phase-2 concern. The modal is cross-platform, adds no
dependency, and is sufficient for the developer-facing editor.

### Decision 25 — The editor uses the ImGui docking branch, not master

The editor's panel layout requires docking. ImGui's docking API
(`ImGui::DockSpace`, `ImGuiDockNodeFlags_*`, `ImGuiWindowFlags_DockNodeHost`)
is not in the master branch; it lives on the `docking` branch.

The vendored copy at `apps/editor/third_party/imgui/` is from the
docking branch, pinned to a specific commit. The commit hash is
recorded in `apps/editor/third_party/imgui/VERSION.txt`. Updates
are deliberate: read the changelog, pick a new commit, replace
the folder, rebuild.

This mirrors the master-branch vendoring decision (Decision 2),
with one addition: the commit hash is recorded, because the
docking branch has no release tags.

### Decision 26 — The console is fed by the engine's log sink

The Console panel registers a sink with `engine::add_log_sink` at
`EditorApp::init` and removes it in its destructor. The sink
copies each `LogRecord` into a `ConsoleBuffer`, a ring buffer
capped at 10,000 lines.

The buffer is thread-safe: the sink may be called from any
thread that logs, so `ConsoleBuffer::push` locks a mutex. The
panel takes a snapshot under the lock and draws without holding
it.

**Filtering.** A single minimum-level combo (Trace / Debug / Info
/ Warn / Error / Fatal, default Info) plus a substring text
filter. No per-level checkboxes; the combo is simpler and matches
how most consoles work.

**Auto-scroll.** The panel auto-scrolls to the bottom while the
user is at the bottom. If the user scrolls up, auto-scroll
pauses; when the user scrolls back to the bottom, it resumes.
The state is a single bool, not a complex scroll-position
heuristic.

**Where the panel draws.** The Console is an ImGui window named
"Console". It is not a child of the main shell. It floats over
the shell by default; the user docks it into the dockspace, or a
default layout docks it in step 11.

### Decision 27 — The editor has project roots

The editor discovers projects by scanning **project roots**. A
root is a directory the editor looks in for project folders. The
editor scans each root one level deep; a subdirectory that
contains `lucid.project.json` is a project.

Two kinds of root:

- **The platform default root.** A per-platform directory the
  editor creates on first use:
  - Windows: `%USERPROFILE%/Lucid Projects/`
  - Linux: `~/Lucid Projects/`
  - macOS: `~/Lucid Projects/`

  New projects are created here by default. It is never removed.

- **User roots.** Any directory a project has been opened from,
  or the user has added. Recorded in `settings.json` under
  `project_roots`. A root whose directory no longer exists is
  dropped on load; a root that exists but contains no projects
  is kept.

The launch panel shows the union of:

- Every project found by scanning the roots.
- Every project in the recent-projects list that is not under
  any current root.

When a project is opened or created, the editor records the
project's parent directory as a user root if it isn't already
one. This means a project outside the default root is
discoverable on the next launch, not just via the recents list.

The scan is shallow (one level). Recursive scanning is rejected:
a root like `~/Lucid Projects/` might contain arbitrary nested
content, and scanning it recursively would be unpredictable and
slow. A user who wants a deeper tree adds a specific project's
parent as a root.

The scan parses each `lucid.project.json` only to read its
`name` field. A malformed project file yields an entry marked
invalid, so the user sees the folder and can click it to see
the error. The scan does not otherwise validate the project.

Scanning happens on launch, on project close, and on a Refresh
button. Not every frame.

### Decision 28 — The Explorer is rooted at the project folder

The Explorer shows the project's file tree, rooted at the project
folder (the folder that contains `lucid.project.json`). It is
not rooted at the engine, the editor, or any user home
directory. A user opening two projects sees two different
Explorers.

The tree is built eagerly when the project is opened and rebuilt
on a Refresh button. It is not rebuilt per frame.

The walk skips:

- Hidden directories: any name starting with `.`
- Common build/tool directories: `build`, `out`, `node_modules`
- Compiled output: `*.lucgraph`
- Temp files: `*.tmp`, `*.bak`, `*~`
- Symlinks: not followed, not shown

The Explorer does not filter by file extension. It shows every
file (except the skipped ones). The script editor (step 8)
decides what it can open.

Double-clicking a file records an open request.
`EditorApp::frame` consumes the request with
`take_open_request()`. Before step 8, the request is logged and
discarded. Step 8 replaces the log with an editor tab.

### Decision 29 — Every panel has a menu toggle and a shortcut

Every dockable panel has a close button (ImGui's `Begin` with
`&visible_`), a Window menu toggle (a `MenuItem` bound to the
same bool), and a keyboard shortcut.

The shortcuts match VS Code where a convention exists:

- Explorer: `Ctrl+Shift+E`
- Console:  `Ctrl+Shift+C`
- Problems: `Ctrl+Shift+P`  (arrives in step 9)
- Package:  `Ctrl+Shift+G`  (arrives in step 10)

The `Window` menu's items show their shortcut text on the right.
The checkmark reflects the panel's visibility.

**Shortcuts are processed centrally in `EditorApp::frame`**, not
inside the panels. Each shortcut flips the panel's `visible_`
bool. This avoids each panel having to know about the keyboard.

**Visibility is not persisted yet.** Panels reset to visible on
launch. Persisting them goes in `settings.json` in step 11
(layout persistence).

### Decision 30 — Docking is on; the default layout is deferred

The editor enables `ImGuiConfigFlags_DockingEnable` at
`ImGuiLayer::init`. ImGui handles the dock-drag-and-drop
mechanism: dragging a window's title bar over the dockspace
shows a preview overlay, and dropping docks the window. ImGui
records the result in its own `imgui.ini` file. The editor does
not implement dock-zone detection or dock-state tracking.

**The default layout is deferred to step 11.** Until then, a
first-run user sees the panels floating and docks them by hand.
Step 11 adds a `DockBuilder` sequence that runs once on first
launch (when `imgui.ini` has no docking data) to place Explorer
left, Console bottom, and the remaining panels center. The
builder ratios are fractions of the dockspace's size, not pixels.

**Rationale for the split:** the mechanism is ImGui's, and it
costs one flag. The layout depends on which panels exist, so it
is deferred until the panel set is complete. Deferring the
layout does not defer the mechanism.

### Decision 31 — The script editor is tabbed and prompts on unsaved changes

The Script Editor panel opens text files in tabs. It owns its
`Document`s; a `Document` is a path, a text buffer, and a dirty
flag. The panel does not compile the buffer; the compile loop
(step 9) reads from the documents.

**Extensions.** The panel opens `.lucid`, `.lscene`, `.json`,
`.txt`, `.md`, and `.cfg`. Other extensions are refused with an
error logged to the Console. A "File > Open File..." action that
bypasses the filter is deferred to phase 2.

**Tabs.** `ImGui::BeginTabBar` with `Reorderable`,
`AutoSelectNewTabs`, and `FittingPolicyScroll`. A dirty tab shows
`*` after its name and sets the `UnsavedDocument` flag.

**Unsaved changes.** Any action that would close dirty documents
— closing a tab, closing the project, quitting the editor —
routes through `request_close_tab` or `request_close_all`. If
any document is dirty, the panel shows a modal prompt with
**Save / Discard / Cancel**. The action's callback runs only
after the prompt is resolved. Save tries to save all dirty
documents; on a save failure, the prompt is dismissed and the
action aborts.

**Shortcuts.** `Ctrl+S` saves the active document. `Ctrl+Shift+S`
saves all. Shortcuts are routed globally (`ImGuiInputFlags_RouteGlobal`)
and are skipped while any popup is open (to prevent the save
shortcut from firing inside the unsaved-changes prompt).

**The panel does not get a toggle shortcut.** It is always open
while a project is loaded. It appears in the Window menu without
a shortcut.

---

## 3. Panel inventory

Phase 1 panels, in the order they are built:

| Panel         | Phase | Purpose                                                         |
| ------------- | ----- | --------------------------------------------------------------- |
| Launch        | 1     | Recent projects, Open, New, Quit. No project loaded.            |
| Main shell    | 1     | Menu bar, toolbar, dock space, status bar.                      |
| Console       | 1     | Engine log ring buffer, level filters.                          |
| Explorer      | 1     | Tree of `assets/`, `scenes/`, `scripts/`, `packages/`.          |
| Script editor | 1     | Tabs, open, save, find-in-file.                                 |
| Problems      | 1     | Compile diagnostics, filterable, click-to-jump.                 |
| Package       | 1     | Read-only list of loaded packages.                              |
| Status bar    | 1     | Project name, package count, FPS.                               |
| Diagnostics   | 2     | Structured runtime diagnostics (package load, registry, scene). |
| Symbol search | 2     | `Ctrl+P` files, `Ctrl+Shift+O` symbols.                         |
| Preferences   | 2     | Edit `settings.json` in the UI.                                 |

Phase 3–5 panels (palette, graph canvas, node inspector, scene
view, entity inspector, Git) are not listed here; they arrive
with their phases.

---

## 4. Engine surface the editor uses

| Editor need                           | Engine API                                    | Status          |
| ------------------------------------- | --------------------------------------------- | --------------- |
| Check a `.lucid` buffer (diagnostics) | `engine::check_source`                        | exists          |
| Compile a `.lucid` buffer (bytes)     | `engine::compile_to_bytes`                    | exists          |
| Engine version (About, project file)  | `engine::kEngineVersion`                      | exists          |
| Format a `.lucid` buffer              | `lucid::formatter::format` (library)          | not wrapped yet |
| Deserialize a `.lucgraph`             | `lucid::serialization::deserialize` (library) | not wrapped yet |
| Read the registry                     | `PackageManager::registry()`                  | exists          |
| List packages                         | `PackageManager::package_infos()`             | exists          |
| Node display metadata                 | `PackageManager::node_editor_info(type)`      | exists          |
| Table display metadata                | `PackageManager::table_editor_info(name)`     | exists          |
| Operation name and group              | `operation_name(op)`, `operation_group(op)`   | exists          |
| Registry conflicts                    | `PackageManager::conflicts()`                 | exists          |
| Log stream                            | `engine::add_log_sink`, `LogRecord`           | exists          |
| Route format diagnostics to log       | `engine::log_diagnostics`                     | exists          |
| Load a scene                          | `SceneLoader_load`, `Scene::apply_to`         | exists          |
| Render a view                         | `render_view(view, ctx, dt, renderer)`        | exists          |

The engine wraps the format library's compile path for the
editor: `engine::check_source` and `engine::compile_to_bytes` are
the editor's compile entry points, and they return
`engine::CompileDiagnostic` (no `lucid/*` type in the editor's
view). Format and deserialize are not wrapped until the editor has
a caller for them (Decision 23). Every other engine-side surface
the editor needs already exists.

---

## 5. Phases

Phases are logical groupings of work, not commits. The commit
sequence in §6 interleaves them; the ImGui vendor, for example,
is a Phase 1 activity that ships before the Phase 0 project
system in commit order. Both orderings are deliberate.

### Phase 0 — Project system and settings (no UI)

- `Project`, `ProjectFile`, `Project::open`, `Project::create`.
- `Settings`, `SettingsStore`, `RecentProjects`.
- `PlatformPaths` (`%APPDATA%`, `~/.config`, `~/Library/Application Support`).
- `lucid.project.json` schema and validation.
- Tests: create a project in a temp dir, reopen it, verify.
  Load/save settings.

No ImGui. No window. Testable without a display.

### Phase 1 — ImGui shell

- Vendor ImGui under `apps/editor/third_party/imgui/`.
- `ImGuiLayer`: init, begin, end, shutdown.
- `EditorApp` with `Mode::Launch` and `Mode::Project`.
- Launch panel.
- Main shell: menu bar, toolbar, dock space, status bar.
- Explorer panel.
- Script editor panel (tabs, open, save, find).
- Console panel with log sink.
- Problems panel with compile loop.
- Package panel (read-only).
- Build action.

**Exit criteria:** open a project, edit and save a `.lucid` file,
see compile errors in the Problems panel, see engine logs in the
Console, list loaded packages. No scene view, no graph editor.

### Phase 2 — Workflow depth

- Run action: launch `lucid_game.exe` with the project path,
  stream its stdout into the console.
- Reload Packages: re-run registration, rebuild the registry.
- Diagnostics panel: structured runtime diagnostics.
- Symbol search: `Ctrl+P`, `Ctrl+Shift+O`.
- Find in files: `Ctrl+Shift+F`.
- Preferences window.
- Hot reload scripts (after Run works).

### Phase 3 — Visual graph editor

- Node palette from `NodeEditorInfo` + `Operation` +
  `NodeTypeInfo::category`.
- Graph canvas, ports, wires, node inspector.
- Save/load `.lucid` through the format library.

### Phase 4 — Scene view

- `View::Pass::World2D` in an ImGui panel.
- Camera controls, selection, inspector, gizmos.
- `SceneLoader_load` / `SceneSaver_from_world`.

### Phase 5 — Git and extensions

- Explorer status colors, branch in status bar.
- Commit UI, diff view, log.
- Extension API (design deferred).
- Marketplace (very late; possibly never).

---

## 6. Commit sequence

The order the editor is built in. Each commit is small, testable,
and useful on its own.

```
1. docs: record editor decisions and phases
2. build: vendor ImGui, add luc_editor + lucid_editor targets,
          draw ImGui demo window, close cleanly
3. editor: Project, ProjectFile, Project::open/create
4. editor: PlatformPaths, Settings, SettingsStore, RecentProjects
5. editor: EditorApp with Launch/Project modes, launch panel
6. editor: Console panel + log sink
7. editor: Explorer panel
8. editor: Script editor panel (no compile)
9. editor: compile loop + Problems panel
10. editor: Package panel
11. editor: status bar, menu bar, layout persistence
12. editor: Build action
```

Commit 1 is this document. Commit 2 is the first code.

---

## 7. What the editor is not

To prevent scope creep:

- **Not a scene editor first.** The scene view is phase 4. The
  script editor is phase 1.
- **Not a package authoring tool.** The package panel is
  read-only in phase 1. Packages are C++ objects linked into the
  game; the editor does not create, edit, or install them.
- **Not a runtime debugger.** The console shows the engine's log;
  it does not set breakpoints, inspect live state, or step
  through a graph. That is a future product, not this one.
- **Not a Git client.** Git is phase 5 and even then is a small
  slice (status, commit, diff).
- **Not a general-purpose IDE.** The script editor edits
  `.lucid` and `.lscene` and `.json`. It does not need to edit
  C++. It does not implement an LSP; it calls
  `engine::check_source` for diagnostics, and adds a small
  `engine::list_symbols` if completion is needed later.
- **Not cross-platform-polished.** GLFW + ImGui work on Windows,
  Linux, and macOS. The editor is developed on Windows first.
  Linux and macOS support are "should work" until proven
  otherwise.

---

## 8. Open questions

Recorded here so they are not forgotten. Each has a phase by
which it must be answered.

- **User-package loading in the editor** (phase 2). How does the
  editor compile a `.lucid` file that imports `alice.math`, if
  `alice.math` is a C++ object linked into the game and not the
  editor?
- **Asset GUIDs** (phase 4, before the scene editor ships). Path
  references break on file move. The reservation is in Decision
  10; the mechanism is not designed.
- **Multi-view scene editing** (phase 4). The editor will want a
  scene view, a game view, and possibly more. `View` supports
  it; the editor's panel layout must.
- **Undo/redo across the editor** (phase 3). The graph editor
  will need it. The script editor may want it. The project
  system may want it. Design a single undo stack or accept
  per-panel undo; the decision is deferred.
- **The `.lucid` file watcher** (phase 2, with hot reload). Watch
  `scripts/` for changes and recompile. Requires a file watcher
  (platform-specific) or a poll. Defer the choice.