# Lucid Game Engine — Master Design Reference

**Quick jump:**      
[Part 1 — Plan Review](#part-1---plan-review)  
[Part 2 — Decisions](#part-2---resolved-design-decisions)  
[Part 3 — Structures](#part-3---architecture--distribution-structures)  
[Part 4 — Resolved Items](#part-4---complete-design-decisions)  
[Part 5 — Status Summary](#part-5---decision-status-summary)  
[Part 6 — External Library Summary](#part-6---external-library-summary)

> **Related Documents:**
> *   [LUC_GRAMMAR.md](./LUC_GRAMMAR.md) — The Luc Language Specification.
> *   [ExecutionPlan.md](./ExecutionPlan.md) — The step-by-step implementation roadmap.
> *   [BuildSystem.md](./BuildSystem.md) — Directory and dependency mapping.

## Part 1 — Plan Review

### LucidEngineOverview.md

#### ✅ Strengths
- **Microkernel model** — C++ kernel for performance, Luc for logic. Clear boundary, proven pattern.
- **Vulkan-first** — skips legacy OpenGL baggage. Correct for a custom engine.
- **Phased roadmap** (C++ Bedrock → Luc Bridge → Evolution) — realistic bootstrapping order.
- **Config-based path resolution** (`engine_settings.json`) — portable, no hardcoded paths.
- **Command Registry** (VS Code approach) — right architecture for extension extensibility.

#### Gap Status

| Area | Issue | Status |
|:---|:---|:---|
| Extension manifest format | Mentioned but not defined. | ✅ Solved — see Decision 7 |
| FFI stability contract | No versioning strategy. | ✅ Solved — see Decision 2 |
| Asset pipeline | No raw→engine format conversion plan. | ✅ Solved — see `AssetPipeline.md` + Decision 11 |
| Scene/Entity model | ECS vs. Scene Tree not decided. | ✅ Solved — see Decision 1 |
| Logic Hot-Reload | File-watcher / re-link flow missing. | ✅ Solved — see Decision 5 |
| Physics library | Not named. | ✅ Solved — see Decision 3 |

---

### LucidEngineSecurity.md + SecurityKeyGuide.md

#### ✅ Strengths
- **Three-tier compilation** (Debug / AOT / Secure JIT) — mirrors Unity IL2CPP vs. Mono.
- **AES-256 encrypted VFS** with RAM-only execution — solid asset protection.
- **SHA-256 boot checksum** — anti-tamper in the right layer (kernel).
- **Signed extension manifests** with Ed25519 — prevents malicious injection.
- **LLVM `CommandLine.h` mutual exclusion** for `--release` / `--release-jit` — correct.
- **Two-Step Title Key DRM** — console-style hardware-locked licensing, no hardcoded keys in DLL.
- **Publisher Key lifecycle** — AES-256 encrypted at rest, recovery key system, key rotation documented.

#### Gap Status

| Area | Issue | Status |
|:---|:---|:---|
| AES key storage | Seed hidden in DLL = obscurity, not security. | ✅ Solved — Two-Step Derivation (Decision 10) |
| Signature authority | Who signs extension manifests? | ✅ Solved — `trusted_publishers.json` (Decision 7) |
| Online licensing | Offline vs. online not decided. | ✅ Solved — Offline-first (Decision 4) |
| Debug build leakage | No guard against shipping debug binary. | ✅ Solved — see Decision 24 |
| Save-game encryption | Runtime save files not mentioned. | ✅ Solved — see Decision 23 |

---

### ControlPanelDesign.md

#### ✅ Strengths
- **4-Tab Bottom Dock** (Terminal, Output, Problems, Engine Console) — matches VS Code / Unreal standard.
- **Engine Console is a live memory bridge**, not a text parser — professional design.
- **3-File Console Ecosystem** clearly separates: Interpreter (C++), Metadata (JSON), Shortcuts (Luc).
- **`console_interpreter.hpp` header drafted** — defines `Execute`, `RegisterCommand`, `GetSuggestions`.
- **IPC-aware** — supports both in-process (same window) and out-of-process (separate game `.exe`) modes.

#### Gap Status

| Area | Issue | Status |
|:---|:---|:---|
| Console Interpreter logic | `Execute()` body not yet drafted. | ✅ Solved — see Decision 20 |
| Symbol map format | `symbols.json` schema not defined. | ✅ Solved — see Decision 18 |
| Runtime IPC protocol | Packet format for Editor → Game communication not specified. | ⏳ Open |

---

### LucidFileFormats.md

#### ✅ Strengths
- **3-tier categorization** (Source / Cooked / Distribution) — clean boundary for Git tracking.
- **All extensions are custom** — no ambiguity with OS-registered formats.
- **Source formats are plain JSON or text** — human-readable, Git-diff-friendly.
- **Cooked formats are pure binary memory dumps** — designed for `memcpy` into GPU/RAM.
- **Distribution formats are signed or encrypted** — `.lucpkg` (Ed25519), `.lucid` (AES-256).

#### Gap Status

| Area | Issue | Status |
|:---|:---|:---|
| `.msgpack` save game format | Not listed — MessagePack for runtime saves was confirmed. | ✅ Solved — see Decision 23 |
| `.ljit` bytecode extension | No extension defined for Secure JIT bytecode output. | ✅ Solved — designated as `.ljit` |

---

### AssetPipeline.md

#### ✅ Strengths
- **Never parse at runtime** — all heavy format work happens offline in the Baker.
- **`luc_asset_baker.exe`** is a separate CLI tool — correct separation of concerns.
- **Asset caching by hash** — only re-cooks changed files, fast iteration.
- **`.lmesh` spec is explicit** — 32-byte header + raw Vulkan-compatible buffers = zero-dependency loader.
- **`IAssetExporter` interface** — extensions can create/export assets in standard formats (PNG, GLTF) without implementing the specs themselves.
- **GLTF chosen as the 3D source format** over FBX/OBJ — open standard, fast `cgltf` parser.
- **`.ltex` over `.ktx2`** — simpler 50-line loader, no external runtime dependency.

#### Gap Status

| Area | Issue | Status |
|:---|:---|:---|
| Audio baking (SFX) | Listed as `.adpcm` or `.ogg` but final choice not locked. | ✅ Solved — see Decision 22 |
| `.lmesh` LOD support | No Level-of-Detail field in the header spec. | ⏳ To add when LOD system is designed |
| Texture atlas / sprite sheet | No pipeline for 2D sprite packing. | ⏳ To define when 2D support is scoped |

---

## Part 2 — Resolved Design Decisions

---

### Decision 1 — Scene Model: ECS ✅

**Entity-Component-System** is confirmed. Benefits for Lucid Engine:
- **Data-oriented** — components are plain POD structs in contiguous arrays → cache-friendly.
- **System-parallel** — systems on different component sets can run on separate threads.
- **Extension-friendly** — extensions register new component types and systems without touching core.

```
Entity (uint64 ID)
└── Components (POD structs: Position, Velocity, Mesh, Collider, Health...)
		└── Systems (Luc or C++: PhysicsSystem, RenderSystem, ScriptSystem...)
```

---

### Decision 2 — FFI Versioning: Function Table Versioning ✅

**4 strategies exist. Strategy 3 is the right pick.**

| # | Strategy | Cross-platform | Backward-compat | Used by |
|:--|:---|:---|:---|:---|
| 1 | Integer Version Guard | ✅ | ❌ Blunt | Quake-era engines |
| 2 | SemVer Struct | ✅ | ⚠️ Partial | Most custom engines |
| **3** | **Function Table Versioning** | **✅** | **✅ Full** | **Vulkan, Chromium PPAPI** |
| 4 | Symbol Versioning (`.so`) | ❌ Linux-only | ✅ | glibc, GTK |

**How it works in Lucid Engine:**

```c
// lge_api.h — shape never changes
typedef struct {
	uint32_t version;       // e.g. 0x00010002 = v1.2
	void (*render_frame)();
	void (*spawn_entity)(uint64_t* out_id);
	void (*add_component)(uint64_t entity, uint32_t component_type, void* data);
} LGE_ExtensionAPI;

// Kernel exports ONE stable function
LGE_API const LGE_ExtensionAPI* LGE_GetAPI(uint32_t requested_version);
```

```c
// Extension side — on load
const LGE_ExtensionAPI* api = LGE_GetAPI(0x00010000);
if (!api) { return LGE_EXTENSION_INCOMPATIBLE; } // graceful fail
api->spawn_entity(&my_entity);
```

**Versioning rule:** Encode as `(major << 16) | (minor << 8) | patch`.
- `major` bump → kernel drops old table → extensions must update.
- `minor` bump → additive only, old table stays valid.
- `patch` bump → bug fix, no ABI change.

#### The Luc Bridge (Logic FFI)
While C++ extensions call `LGE_GetAPI` directly, **Luc extensions** receive a pre-negotiated `api` object. The Kernel performs the version check against the `api_version` declared in `extension.json` before passing the object to `on_load(api)`.

This ensures that Luc code is **always type-safe** and never has to deal with raw function pointers or version mismatch crashes. The `api` object in Luc is a high-level projection of the C++ function table, where each namespace (e.g., `api.ui`, `api.fs`) corresponds to a specific subsystem in the Kernel.


---

### Decision 3 — Physics Backend: Jolt Physics ✅

**6 libraries compared:**

| Library | License | Maintenance | Modding-friendly |
|:---|:---|:---|:---|
| **Jolt Physics** | **MIT** | ✅ Very active | ✅ Full callbacks |
| NVIDIA PhysX 5 | BSD-3 | ✅ Active | ⚠️ Complex API |
| Bullet Physics | zlib | ⚠️ Slowing down | ✅ Open source |
| ReactPhysics3D | zlib | ✅ Active | ✅ Clean API |
| Box2D v3 | MIT | ✅ Active | ✅ 2D only |
| Havok | Commercial | ✅ Active | ❌ Closed |

**Jolt wins:** AAA-proven (*Horizon Forbidden West*, *Death Stranding 2*), MIT license, modern C++17, built-in multi-thread support, rich callback system.

**Luc developer access via FFI:**

```c
// lge_api.h — Luc-visible physics hooks
LGE_API void LGE_SetOnCollisionCallback(void (*cb)(uint64_t a, uint64_t b));
LGE_API void LGE_SetGravity(float x, float y, float z);
LGE_API void LGE_AddForce(uint64_t entity, float fx, float fy, float fz);
LGE_API void LGE_SetBodyProperties(uint64_t entity, float mass, float friction, float restitution);
```

**Physics ↔ Rendering coupling: None.**

```
Frame loop:
1. PhysicsSystem::Update(dt)   → Jolt writes → TransformComponent
2. RenderSystem::Update()      → Vulkan reads ← TransformComponent

Jolt and Vulkan never talk directly. TransformComponent is the only shared data.
```

> [!IMPORTANT]
> Always run `PhysicsSystem` **before** `RenderSystem` per frame. The GPU must see this frame's physics result, not last frame's stale positions.

---

### Decision 4 — Licensing Model: Offline-First (with Online Migration Path) ✅

**Start offline. Design the abstraction layer now so online can be added without touching the kernel later.**

#### The Abstraction Layer (the key architectural move)

Instead of calling license verification code directly, the kernel talks to an `ILicenseVerifier` interface:

```cpp
// kernel/include/lge_license.h
class ILicenseVerifier {
public:
	virtual LicenseResult Verify() = 0;           // called at boot
	virtual bool HasFeature(uint32_t flag) = 0;   // checked at runtime
	virtual ~ILicenseVerifier() = default;
};

// kernel boots with this — swappable without recompiling the kernel
extern ILicenseVerifier* g_license_verifier;
```

Now two implementations exist and can be selected at build time or config time:

| Implementation | When | How |
|:---|:---|:---|
| `OfflineLicenseVerifier` | **Now** (Phase 1) | Reads `license.lge`, checks Ed25519 signature against public key baked in DLL |
| `OnlineLicenseVerifier` | Future (Phase 2) | Phones home to a REST endpoint, caches a short-lived JWT, falls back to offline grace period |

#### Phase 1 — Offline Implementation (now)

```
Boot sequence:
OfflineLicenseVerifier::Verify()
	→ read license.lge from engine dir
	→ verify Ed25519 signature (public key baked into luc_kernel.dll)
	→ extract { app_id, expiry_date, feature_flags, machine_fingerprint }
	→ compare machine_fingerprint against current hardware
	→ PASS → proceed  |  FAIL → Trial mode
```

**Machine fingerprinting** (prevents license file sharing):
```cpp
string machine_id = sha256(get_cpu_id() + get_motherboard_serial() + get_disk_serial());
// machine_fingerprint baked into license.lge at generation time
// Kernel compares at boot — mismatch = invalid
```

**Pros of offline-first:**
- ✅ Zero server infrastructure cost to start.
- ✅ Works on air-gapped machines, game jams, planes.
- ✅ No network code in the kernel security layer.
- ✅ Developer-friendly — engine never refuses to open due to a dead server.

#### Phase 2 — Online Migration (future, no kernel rewrite needed)

When you're ready to add online enforcement:
1. Write `OnlineLicenseVerifier` as a new `.cpp` — same interface, new implementation.
2. Switch `g_license_verifier` assignment in `kernel.cpp` based on `engine_settings.json`:
```json
{ "license_mode": "online", "license_server": "https://license.lucidengine.com" }
```
3. The rest of the kernel is untouched. The abstraction did its job.

> [!TIP]
> **The offline verifier always stays as the grace-period fallback** inside `OnlineLicenseVerifier`. If the server can't be reached, it falls back to the last cached JWT. If that expires, it gives a 72-hour offline grace period before blocking.

---

### Decision 5 — Logic Hot-Reload ✅

Hot-reload means: the developer saves a `.luc` file → the engine recompiles it → the running game reflects the change **without restarting**.

#### The System: Watch → Compile → Swap

Three components work together:

```
[FileWatcher]  →  [Compiler Subprocess]  →  [Module Swapper]
(C++)              (luc_compiler.exe)         (C++ kernel)
```

---

**Step 1 — FileWatcher (in kernel)**

The kernel spawns a background thread that watches `user_projects/<active>/src/` for file changes.

```cpp
// kernel/src/core/file_watcher.cpp
// Uses ReadDirectoryChangesW (Win32) or inotify (Linux)

void FileWatcher::Start(const fs::path& watch_dir) {
	watcher_thread = std::thread([&] {
		while (running) {
			if (HasChangedFiles(watch_dir)) {
				string changed_file = GetChangedFile();
				on_file_changed(changed_file);  // fires the compile pipeline
			}
			std::this_thread::sleep_for(100ms);
		}
	});
}
```

**Platform APIs:**
- Windows → `ReadDirectoryChangesW` (built into Win32, zero deps)
- Linux → `inotify_add_watch` (built into kernel, zero deps)

---

**Step 2 — Compiler Subprocess**

When a file change is detected, the kernel spawns `luc_compiler.exe` as a child process:

```cpp
// kernel/src/core/script_manager.cpp

void ScriptManager::RecompileModule(const string& luc_file) {
	// Output goes to a temp .dll, NOT the live .dll
	string cmd = "luc_compiler.exe --input " + luc_file 
			+ " --output ./build/hot/" + module_name + "_next.dll";
	
	PROCESS_INFORMATION pi;
	CreateProcess(NULL, cmd.c_str(), ...);
	WaitForSingleObject(pi.hProcess, INFINITE); // wait for compile
	
	DWORD exit_code;
	GetExitCodeProcess(pi.hProcess, &exit_code);
	if (exit_code == 0) {
		SwapModule(module_name); // only swap if compile succeeded
	} else {
		// Show compiler errors in the IDE log panel — don't swap
		LogCompilerErrors(module_name + "_next.dll");
	}
}
```

> [!IMPORTANT]
> Always compile into a **temp file** (`_next.dll`), never directly overwrite the live `.dll`. Only swap after a successful compile. Failed compilations must **never** crash the running game.

---

**Step 3 — Module Swapper**

The swapper waits for the current frame to finish, then atomically replaces the module:

```cpp
// kernel/src/core/script_manager.cpp

void ScriptManager::SwapModule(const string& module_name) {
	// 1. Wait for current frame to finish — do NOT swap mid-frame
	frame_fence.Wait();

	// 2. Call the old module's cleanup hook (if it has one)
	if (old_api->on_unload) old_api->on_unload();

	// 3. Unload the old DLL
	FreeLibrary(old_module_handle);

	// 4. Rename _next.dll → live .dll
	fs::rename("./build/hot/" + module_name + "_next.dll",
				"./build/hot/" + module_name + ".dll");

	// 5. Load the new DLL
	HMODULE new_handle = LoadLibrary(("./build/hot/" + module_name + ".dll").c_str());
	auto* new_api = (LGE_ExtensionAPI*)GetProcAddress(new_handle, "LGE_GetAPI")(LGE_VERSION);

	// 6. Call the new module's init hook
	if (new_api->on_load) new_api->on_load();

	// 7. Register in the live module table
	live_modules[module_name] = { new_handle, new_api };
}
```

---

**State Persistence across hot-reload**

The hardest part of hot-reload is keeping game state (player position, health, etc.) alive when the script swaps. Two approaches:

| Approach | How | Best for |
|:---|:---|:---|
| **Stateless Scripts** | Scripts don't own state — ECS components own all state. Script just reads/writes components. | ✅ Lucid Engine (ECS already solves this) |
| **Serialized State** | Script serializes its state to JSON before unload, deserializes after load. | Complex stateful extensions |

**ECS makes this free:** Because all entity state lives in `TransformComponent`, `HealthComponent`, etc. (not inside the script), a script swap doesn't lose any data. The new script picks up where the old one left off by reading the same components.

---

**Full hot-reload flow diagram:**

```
User saves player.luc
	│
	▼
FileWatcher detects change
	│
	▼
ScriptManager::RecompileModule("player")
→ spawns: luc_compiler.exe --input player.luc --output player_next.dll
	│
	├── Compile FAILED → show errors in IDE log, keep old module running
	│
	└── Compile OK
			│
			▼
		Wait for frame fence
			│
			▼
		old_api->on_unload()
		FreeLibrary(old .dll)
		rename _next.dll → .dll
		LoadLibrary(new .dll)
		new_api->on_load()
			│
			▼
		Game continues — no restart, state preserved by ECS
```

---

### Decision 6 — IntelliSense (Language Server) ✅

The engine needs code completion, go-to-definition, and error squiggles inside the Luc script editor. This is implemented as a **separate `luc_langserver` process** — the same architecture VS Code uses with its Language Server Protocol (LSP).

#### Why a separate process, not a library?

The IDE (written in Luc) cannot call C++ analysis code directly — that would create a circular dependency. A separate process communicates via stdin/stdout JSON, keeping the boundary clean.

```
IDE (engine/src/editor/script_editor.luc)
│
│  JSON over stdin/stdout (LSP-style protocol)
▼
luc_langserver.exe (C++ process, always running in background)
│
├── Reuses compiler's Lexer + Parser + Symbol Table (no code-gen)
├── Maintains an in-memory AST index of all open .luc files
└── Responds to requests:
		textDocument/completion   → list of symbols, keywords, types
		textDocument/hover        → type info, docstring
		textDocument/definition   → jump to declaration file + line
		textDocument/diagnostic   → real-time error/warning list
```

#### How it reuses the compiler

The compiler already builds an AST and symbol table in `lexer/` and `parser/`. The language server **reuses these same modules**, skipping only `codegen/` and `optimizer/`:

```
luc_compiler.exe:   Lex → Parse → Type-check → Optimize → Codegen → .dll
luc_langserver.exe: Lex → Parse → Type-check → (respond to IDE queries)
									↑
							Same code, shared as a static library
```

This means the language server's completions and errors are **always in sync** with what the compiler actually accepts — no drift between "what the IDE shows" and "what actually compiles."

#### IDE integration (Luc side)

```luc
-- engine/src/editor/script_editor.luc
-- The editor starts the langserver as a child process on engine boot

const lang_server &Process = Process:spawn("./compiler/luc_langserver.exe")

let on_keystroke (file string, cursor_pos Vec2) = {
    let request Request = Request { 
        method = "textDocument/completion"
        file   = file
        pos    = cursor_pos 
    }
    
    lang_server:write(json:serialize(request))
    let response string = lang_server:read_line()
    show_completion_popup(json:parse(response).items)
}
```

> [!NOTE]
> The langserver runs **one instance per engine session**, not per file. It indexes all `.luc` files in the active project on startup and updates incrementally as files change.

---

### Decision 7 — Extension System ✅

#### Architecture: VS Code-style, adapted for a game engine

Yes — VS Code's extension model is the right reference point. Every extension is a **self-contained folder** with a manifest and a main entry point. The key adaptations for Lucid Engine:

---

#### Extension vs. Library — they are different things

This distinction is critical and must be enforced by the engine:

| | **Extension** | **User Library** |
|:---|:---|:---|
| Has `extension.json` manifest | ✅ Required | ❌ None |
| Lifecycle hooks (`on_load`, `on_unload`) | ✅ Yes — called by kernel | ❌ No |
| Runs during development | ✅ Yes (always active while engine is open) | ❌ Only when imported |
| Adds UI, menus, commands | ✅ Yes | ❌ No |
| How it's used | Engine loads it automatically | Other `.luc` files do `import "my_lib.luc"` |
| Example | Visual Debugger, Theme Manager, Git Integration | `math_utils.luc`, `enemy_ai.luc`, `pathfinding.luc` |

**Rule:** If it modifies the engine's behavior or UI, it's an **Extension**. If it's just reusable code called by game scripts, it's a **Library** — keep it in the project's `src/libs/` folder.

---

#### Extension ID — How Unique IDs Work, Collision Handling, and Key Safety

---

##### ID Generation — Two-Layer System

**Layer 1 — Human-readable ID (in `extension.json`):**
```
com.{publisher_id}.{extension-name}
```
This is what humans read, type, and declare as a dependency.

**Layer 2 — UUID v5 (internal machine ID, auto-generated):**
```
extension_uuid = uuid5(LUCID_NAMESPACE, "com.taiax.my-extension")
→ always "a3f8c21e-5b9d-5e4f-8c3a-1d2e4f6a8b0c" for this exact string
```
UUID v5 is a deterministic SHA-1 hash of a fixed namespace + the human ID string. Same string → same UUID, always, forever. No random number, no timestamp.

```cpp
// Generated ONCE at "Create New Extension" time — never regenerated
std::string generate_extension_uuid(const std::string& human_id) {
	return uuid5(LUCID_NAMESPACE, human_id);
}
```

---

##### Case 1 — What if two developers pick the same human-readable ID?

This will happen, especially offline. There are two sub-cases:

**Sub-case A: Same full ID string (e.g., both use `com.dev.my-extension`)**

Since UUID v5 is deterministic, both produce the *identical* UUID. This is a genuine conflict.

**How the engine handles it:**

```
Extension loader startup:
for each extension in extension_registry.json:
	1. Read uuid from extension.json
	2. Check against already-loaded uuid table
	3. If CONFLICT found:
		→ Do NOT load the second extension
		→ Show in Extensions panel: ⚠️ "com.dev.my-extension" conflicts with an
		already-loaded extension (same UUID). Rename your extension ID to resolve.
		→ Write conflict to logs/extension_conflicts.log
	4. If no conflict → load normally
```

The first extension alphabetically in `extension_registry.json` wins. The engine **never crashes** — it simply refuses to load the conflicting one and tells the developer exactly why.

**Sub-case B: Different publisher_id, same extension name (e.g., `com.alice.shooter` vs `com.bob.shooter`)**

These produce **different UUIDs** — the full string `"com.alice.shooter"` ≠ `"com.bob.shooter"`, so the hashes differ. **No conflict.** This is the intended behavior.

**Online registry enforcement (no conflicts possible after registration):**

| Action | What the registry enforces |
|:---|:---|
| Register `alice` as publisher_id | `com.alice.*` is now reserved for Alice's Ed25519 key only |
| Alice submits `com.alice.shooter` | Registry checks: is `com.alice.shooter` already taken? No → accept |
| Bob tries to submit `com.alice.shooter` | Signature doesn't match Alice's registered key → rejected immediately |
| Bob submits `com.bob.shooter` | Completely different ID — accepted with no conflict |

Once online, **the only collisions that can exist are within a publisher's own namespace** — and that's their own responsibility to manage.

---

##### Case 2 — What if the developer changes machine or loses their private key?

The private key (`publisher_private.pem`) is a file on the developer's machine. If the machine dies or the drive fails, **the key is gone unless they backed it up.** This is the same problem SSH keys, PGP keys, and GPG keys have. The solution is a three-layer safety system:

---

**Layer A — Mandatory backup prompt at key generation time**

When a developer first sets up their publisher profile, the engine generates the key pair and **blocks them from continuing** until they acknowledge backup:

```
┌─────────────────────────────────────────────────────┐
│  ⚠️  IMPORTANT: Back Up Your Publisher Key          │
│                                                     │
│  Your private key has been saved to:                │
│  %APPDATA%\LucidEngine\publisher_private.pem        │
│                                                     │
│  If you lose this file, you cannot publish updates  │
│  to your extensions until you complete key recovery.   │
│                                                     │
│  → [Export Key to File...]  [Copy to Clipboard]     │
│                                                     │
│  [ ] I have backed up my key in a safe location     │
│                         [Continue] (disabled until ✓)│
└─────────────────────────────────────────────────────┘
```

The engine stores the private key in `%APPDATA%\LucidEngine\` (or `~/.lucid_engine/` on Linux). This is **not** inside the engine install folder — it survives engine updates.

---

**Layer B — Recovery key (generated alongside the main key)**

At key generation time, the engine creates **two** key pairs:
- `publisher_private.pem` — the main signing key, used every time you publish
- `publisher_recovery.pem` — stored somewhere completely separate (printed out, USB drive, password manager)

The recovery public key is registered in the registry alongside the main one:

```json
// Registry record for publisher "taiax"
{
"publisher_id": "taiax",
"keys": [
	{ "type": "main",     "public_key": "ed25519:abc123...", "status": "active" },
	{ "type": "recovery", "public_key": "ed25519:xyz789...", "status": "recovery-only" }
]
}
```

The recovery key **cannot sign extension manifests** — it can only be used to add/replace the main key.

---

**Layer C — Key rotation (add a new machine without losing access)**

Before moving to a new machine, the developer adds a new key from their old machine:

```
Old machine:
1. Generate new key pair on new machine → get new_public.pem
2. On old machine: sign a "key addition request" with old private key
	{ "action": "add_key", "new_public_key": "ed25519:new123...", "publisher": "taiax" }
3. Submit to registry → verified with old key → new key added
4. Both keys are now valid

New machine:
5. Copy new_private.pem to new machine
6. (Optional) Revoke old key from new machine using new private key
```

If the **old machine is already gone** (lost key):
```
Recovery path:
1. Use publisher_recovery.pem
2. Sign a "key replacement request" with the recovery key
3. Registry verifies the recovery key → replaces the lost main key
4. Generate and register a new main key
5. Generate a new recovery key (the old one was used — treat it as spent)
```

---

**Layer D — Key Encryption at Rest (Preventing Fraud)**

If a developer accidentally uploads their `publisher_private.pem` to a public GitHub repo, or gets malware that steals the file, malicious actors could publish fake updates to their extensions.

To prevent this, the private key is **never stored in plain text**. It is encrypted at rest using AES-256-CBC, protected by a developer-chosen passphrase.

```
-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFDjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIe3n...
...
-----END ENCRYPTED PRIVATE KEY-----
```

- When running `sign_extension.py`, the developer is prompted to type their passphrase.
- The engine caches the decrypted key in **secure memory** (cleared on exit) during the active session so they don't have to type it for every single build.
- If the `.pem` file is stolen, the attacker still cannot use it without the passphrase.

---

**Summary of protection levels:**

| Scenario | What happens | Recovery possible? |
|:---|:---|:---|
| Developer gets a new machine | Add new key from old machine before switching | ✅ Yes — key rotation |
| Old machine dies, key was backed up | Copy backup `.pem` to new machine | ✅ Yes |
| Old machine dies, no backup, has recovery key | Use recovery key to replace main key | ✅ Yes |
| Old machine dies, no backup, no recovery key | Contact Lucid team for manual identity verification | ⚠️ Slow — but possible |
| Key is compromised (stolen) | Revoke with recovery key immediately | ✅ Yes |

> [!IMPORTANT]
> **For local/offline use only:** Key loss has no consequence. The `signature` field is optional when the engine is running offline-only. The developer can regenerate a new key pair and re-sign their local extensions — no registry interaction needed.



---

#### Extension Manifest Schema (`extension.json`)

```json
{
"id": "com.yourname.my-extension",        // Human-readable — developers use this
"uuid": "a3f8c21e-5b9d-5e4f-8c3a-1d2e4f6a8b0c",  // UUID v5 — auto-generated, NEVER change
"name": "My Extension",
"version": "1.0.0",                    // SemVer
"api_version": "0x00010000",           // Minimum kernel API version required
"author": "Your Name",                // Auto-filled from user_settings.json
"publisher": "yourname",              // Auto-filled from user_settings.json
"description": "What this extension does",
"entry_point": "main.luc",            // File with on_load / on_update / on_unload
"type": "extension",                  // extension | theme | tool | network-provider
"permissions": ["ui.menu", "ecs.read", "ecs.write", "filesystem.project"],

// LAZY LOADING (When should the engine run main.luc?)
"activationEvents": [
	"onCommand:my-extension.run",
	"onView:my-extension-panel"
],

// STATIC UI DEFINITIONS (What does this add to the IDE?)
"contributes": {
	"commands": [{ "command": "my-extension.run", "title": "Run My Tool" }],
	"menus": { "editor/context": [{ "command": "my-extension.run" }] },
	"viewsContainers": {
	"activitybar": [{ "id": "my-extension-panel", "title": "My Tool", "icon": "icons/panel.svg" }]
	}
},

"signature": "base64-encoded-sig",    // Ed25519 sig over the rest of this JSON
"dependencies": [
	{ "id": "com.lucid.core", "version": ">=1.0.0" }
]
}
```

#### Why `contributes` and `activationEvents` are critical
Just like VS Code's `package.json`, this declarative approach solves two massive problems:
1. **Zero-Cost Startup:** The engine reads all `extension.json` files on boot and builds the UI menus, activity bar, and command palette *without executing a single line of Luc code*. 
2. **Lazy Loading:** `main.luc` is strictly ignored until the user clicks the button defined in `contributes`. This keeps the engine's memory footprint incredibly small, no matter how many extensions are installed.

#### How Permissions are Resolved and Enforced
Permissions are not just a warning; they are enforced at the C++ Kernel boundary.

1. **Installation (User Consent):** When a user downloads an extension, the IDE parses the `permissions` array in `extension.json`. If it contains `network.server` or `filesystem.global`, a large warning prompts the user for consent. If they decline, the installation aborts.
2. **Runtime Enforcement (The Kernel Gatekeeper):** 
When an extension's `main.luc` is eventually loaded, the engine assigns it an **Execution Context ID**. This ID maps to the approved permissions.
If the script tries to call a sensitive FFI function (e.g., `api.network.listen(8080)`), the C++ Kernel intercepts the call:
```cpp
// Inside luc_kernel.dll
LGE_API void LGE_Network_Listen(ExecutionContext context, int port) {
	if (!HasPermission(context, "network.server")) {
		LogError("Extension denied: Missing 'network.server' permission");
		return; // Hard block at the C++ level
	}
	// Proceed with binding the port...
}
```
Because the enforcement happens in C++, a malicious Luc script cannot bypass it.

**Permission whitelist** — extensions declare what they need:

| Permission | What it grants | Trust level |
|:---|:---|:---|
| `ui.menu` | Register items in the top menu bar | Low |
| `ui.panel` | Add dockable panels to the workspace | Low |
| `ecs.read` | Read component data from the world | Low |
| `ecs.write` | Modify component data | Medium |
| `filesystem.project` | Read/write inside the active project folder | Medium |
| `filesystem.global` | Read/write anywhere (warn user on install) | High |
| `compiler.hook` | Intercept the compile pipeline | High |
| `network.client` | Open outbound connections via `INetworkProvider` | Medium |
| `network.server` | Bind and listen as a server | High — explicit user consent required |



---

#### Extension Entry Point (Luc side)

```luc
-- my-extension/main.luc — the required lifecycle contract

export const on_load (api &Api) = {
    -- Called when the extension is activated
    api.menu:register("View/My Extension Panel", open_panel)
    api.commands:register("my-extension.run", run_action)
    io.printl("My Extension loaded!")
}

export const on_update (dt float) = {
    -- Called every editor frame (optional — omit if not needed)
}

export const on_unload () = {
    -- Called before the extension is deactivated or hot-reloaded
    -- Clean up any registered hooks
}
```

---

#### Extension UI in the Engine — "Extensions" Panel

The Extensions panel is divided into two sections as you described:

```
┌─────────────────────────────────────────┐
│  EXTENSIONS                         [+] │  ← '+' = Create new / Add existing folder
├─────────────────────────────────────────┤
│  MY EXTENSIONS                          │  ← Extensions you authored or added locally
│  ┌─────────────────────────────────┐   │
│  │ 🟢 Visual Debugger    v1.2.0   │   │  ← green dot = active
│  │ 🔴 Git Integration    v0.9.0   │   │  ← red dot = inactive / error
│  │ [+ Create New Extension]       │   │
│  └─────────────────────────────────┘   │
├─────────────────────────────────────────┤
│  OTHER EXTENSIONS                       │  ← Community / marketplace extensions
│  ┌─────────────────────────────────┐   │
│  │ 🟢 Theme Manager      v2.1.0   │   │
│  │ 🟢 Shader Graph       v1.0.3   │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

**"+ Create New Extension" flow:**
1. Engine reads `user_settings.json` → pre-fills `author` and `publisher` fields.
2. User enters only: Extension Name + Description.
3. Engine generates the full folder structure:
```
user_extensions/my-extension/
├── extension.json     ← auto-generated with all fields filled in
└── main.luc           ← starter template with on_load/on_unload stubs
```
4. Extension immediately appears in "My Extensions" and is ready to edit.

**"+ Add Existing Folder" flow:**
1. File picker → user selects a folder containing an `extension.json`.
2. Engine validates the manifest (checks schema, api_version compatibility).
3. If valid → adds to the extension registry (`user_extensions/` or symlinks the folder).

---

#### No Database Needed — Pure Local JSON Storage

**Do not use a database.** Everything lives as local files:

```
%APPDATA%\LucidEngine\                  ← or ~/.lucid_engine/ on Linux
├── user_settings.json                  ← developer profile (author, publisher, theme prefs)
└── extension_registry.json             ← list of installed extension IDs and their folder paths
```

**`user_settings.json`** (auto-filled into new extension manifests):
```json
{
"display_name": "Tai Ax",
"publisher_id": "taiax",
"email": "",
"preferred_theme": "dark",
"editor_font_size": 14
}
```

**`extension_registry.json`** (the engine's installed extension list — no server needed):
```json
{
"installed": [
	{ "id": "com.taiax.visual-debugger", "path": "./user_extensions/visual-debugger/", "active": true },
	{ "id": "com.lucid.theme-manager",   "path": "./core_extensions/theme-manager/",   "active": true }
]
}
```

**Advantages of flat-file storage:**
- ✅ Zero dependencies — no SQLite, no embedded DB.
- ✅ Human-readable and version-controllable.
- ✅ Works fully offline — aligns with the licensing model.
- ✅ Users can inspect/edit it if something goes wrong.
- ✅ No user data is ever sent to a server — privacy by default.

> [!NOTE]
> `user_settings.json` is stored in `%APPDATA%` (not the engine folder) so it persists across engine updates and is shared across all projects on that machine.

---

#### Extension Distribution Lifecycle

This section covers the complete end-to-end flow: from writing an extension to a user installing it. It is designed for **Phase 1 (now — closed group)** with clear markers for **Phase 2 (future — public website)**.

---

##### Phase 1 — Closed Group Distribution (Current)

Since Lucid is a small project, the distribution channel is a **trusted group** (Discord, private GitHub, shared link). The infrastructure is minimal — just a JSON file hosted on GitHub Gist.

```
Infrastructure needed right now:
├── GitHub Gist (or raw GitHub file)  ← trusted_publishers.json
├── Discord server / GitHub Releases  ← where extension zip files are shared
└── scripts/sign_extension.py            ← already in your repo
```

**`trusted_publishers.json`** — hosted at a fixed URL, maintained by you:
```json
{
"_comment": "Lucid Engine trusted publishers registry — Phase 1",
"_updated": "2026-04-25",
"publishers": [
	{
	"publisher_id": "taiax",
	"display_name": "Tai Ax",
	"public_key": "ed25519:abc123def456...",
	"recovery_key": "ed25519:xyz789uvw012...",
	"status": "active"
	},
	{
	"publisher_id": "alice",
	"display_name": "Alice Dev",
	"public_key": "ed25519:mno345pqr678...",
	"recovery_key": "ed25519:stu901vwx234...",
	"status": "active"
	}
]
}
```

The engine fetches this file **once per session** (or uses a cached copy if offline). This is the only "server" needed for Phase 1.

> [!TIP]
> Host `trusted_publishers.json` on a GitHub Gist. The raw URL is stable and free. When Phase 2 arrives, you just change `trusted_publishers_url` in `engine_settings.json` to point to your real API endpoint. **Zero code change in the engine.**

---

##### Step-by-Step: Publisher Key Setup (One-Time)

```
Step 1 — Generate key pair (engine does this automatically on first publish)
scripts/generate_publisher_key.py --publisher taiax

Prompt: "Enter a strong passphrase to encrypt your private key:"
Prompt: "Confirm passphrase:"

→ Creates: %APPDATA%\LucidEngine\publisher_private.pem   ← ENCRYPTED (AES-256)
→ Creates: %APPDATA%\LucidEngine\publisher_recovery.pem  ← KEEP OFFLINE
→ Prints:  publisher_public.pem content to console        ← SHARE THIS

Step 2 — Register with Lucid team (Phase 1: send a Discord DM)
→ Send your publisher_id + public key to the Lucid team
→ Team adds your entry to trusted_publishers.json on GitHub Gist
→ You are now a "trusted publisher"

Step 3 — Verify registration
→ Open engine → Extensions panel → "Verify Publisher Key" button
→ Engine fetches trusted_publishers.json and confirms your key is listed
→ ✅ "Publisher 'taiax' verified"
```

---

##### Step-by-Step: Signing and Publishing an Extension

```
Step 4 — Write your extension
user_extensions/my-extension/
├── extension.json   ← id, uuid, permissions declared
└── main.luc      ← on_load / on_update / on_unload

Step 5 — Sign the extension
scripts/sign_extension.py --extension ./user_extensions/my-extension/ --key publisher_private.pem

What the script does internally:
	a. Reads extension.json (without the "signature" field)
	b. SHA-256 hashes every file in the extension folder
	c. Builds a payload:
		{ "manifest": {...extension.json...}, "file_hashes": { "main.luc": "sha256:abc..." } }
	d. Signs the payload with Ed25519 private key
	e. Base64-encodes the signature
	f. Writes it into extension.json → "signature": "base64:..."
	g. Writes a sidecar: extension.json.sig (for human inspection)

Step 6 — Package for distribution
scripts/pack_extension.py --extension ./user_extensions/my-extension/
→ Creates: my-extension-v1.0.0.lucext   ← a renamed zip

Step 7 — Distribute (Phase 1: Discord / GitHub Release)
→ Upload my-extension-v1.0.0.lucext to the Lucid Discord #extensions channel
	or as a GitHub Release asset
→ Post: extension name, version, permissions list, and a short description
→ Users download the .lucpkg file
```

---

##### Step-by-Step: Installing an Extension (User Side)

```
Step 8 — User downloads the .lucpkg file

Step 9 — User adds it to the engine
Engine Extensions panel → [+] → "Add .lucext file"

Engine install flow:
	a. Unzip .lucext → extract extension folder to user_extensions/
	b. Read extension.json → extract publisher_id and signature
	c. Fetch trusted_publishers.json (cached or live from Gist URL)
	d. Look up the publisher's public key

		publisher_id = "taiax"
		public_key   = "ed25519:abc123..."  ← from trusted_publishers.json

	e. Rebuild the signed payload from the extracted files
	f. Verify Ed25519 signature:
		PASS → extension is authentic, files are unmodified
		FAIL → show warning and block install

	g. Permission review dialog shown to user:

		┌─────────────────────────────────────────────┐
		│  Install "Visual Debugger" by taiax?        │
		│  Version: 1.2.0  |  Publisher: ✅ Verified  │
		├─────────────────────────────────────────────┤
		│  This extension requests:                    │
		│    • ui.panel      — Add editor panels      │
		│    • ecs.read      — Read entity data       │
		│    • ecs.write     — Modify entity data     │
		├─────────────────────────────────────────────┤
		│            [Cancel]    [Install]            │
		└─────────────────────────────────────────────┘

	h. User clicks Install → extension added to extension_registry.json → active
```

---

##### What happens with an unknown publisher (not in trusted_publishers.json)?

```
Unverified extension install flow:
→ Signature check: public key NOT found in trusted_publishers.json

┌─────────────────────────────────────────────────┐
│  ⚠️  Unverified Publisher                       │
│                                                 │
│  "My Sketchy Extension" claims publisher "unknown" │
│  This publisher is not in the trusted list.     │
│                                                 │
│  Only install extensions from sources you trust.   │
│                                                 │
│  [Cancel]   [Install Anyway — I trust this]    │
└─────────────────────────────────────────────────┘

→ If user accepts: extension installed with a ⚠️ badge in the Extensions panel
→ Engine logs: "Unverified extension installed: com.unknown.extension"
```

This allows full local flexibility while still making the trust boundary visible.

---

##### Phase 2 Migration — When You're Ready for a Public Website

When the project grows, migration is **four changes only**. Nothing in the kernel changes.

| What changes | How |
|:---|:---|
| `engine_settings.json` → `trusted_publishers_url` | Point from GitHub Gist URL to `https://api.lucidengine.com/publishers` |
| `engine_settings.json` → `extension_repository_url` | Add `https://extensions.lucidengine.com` for browse/search |
| `scripts/sign_extension.py` | Stays identical — signing process doesn't change |
| Engine Extensions panel | Add a "Browse" tab that fetches from the new extension API |

Everything else — the key format, the signature algorithm, the manifest schema, the `.lucext` format, `trusted_publishers.json` structure — is **identical between Phase 1 and Phase 2**. Phase 1 extensions work in Phase 2 without re-signing.

```json
// engine_settings.json — the only thing that changes between phases
{
"trusted_publishers_url": "https://gist.githubusercontent.com/.../trusted_publishers.json",
// ↑ Phase 1: GitHub Gist
// ↓ Phase 2: just change this one line
"trusted_publishers_url": "https://api.lucidengine.com/v1/publishers",
"extension_repository_url": "https://extensions.lucidengine.com"
}
```

---

### Decision 8 — Network Extension API ✅

**Yes — allow developers to write their own server extensions, choose their own provider, and implement their own transport.** The engine provides a clean **Network Abstraction Layer (NAL)** — the same pattern as the `ILicenseVerifier` and the Vulkan RHI.

#### Why this is the right call

Different game types need radically different networking:
- A **real-time FPS** needs low-latency UDP with custom reliability.
- A **turn-based strategy** needs reliable TCP, can tolerate latency.
- A **co-op RPG** might use a hosted service like Steam Relay or Epic Online Services.
- A **custom MMO backend** needs a fully custom server.

No single networking solution covers all of these. The engine should not pick one.

---

#### The Network Abstraction Layer

The kernel defines one interface. Developers implement it.

```cpp
// kernel/include/lge_network.h

class INetworkProvider {
public:
	// Lifecycle
	virtual bool   Initialize(const NetworkConfig& config) = 0;
	virtual void   Shutdown() = 0;

	// Connection
	virtual ConnHandle Connect(const char* address, uint16_t port) = 0;
	virtual void       Disconnect(ConnHandle conn) = 0;
	virtual bool       Listen(uint16_t port) = 0;  // server mode

	// Data transfer — the engine only hands you a flat byte buffer
	virtual void   Send(ConnHandle conn, const uint8_t* data, uint32_t size, SendFlags flags) = 0;
	virtual int32_t Receive(ConnHandle conn, uint8_t* buffer, uint32_t buffer_size) = 0;

	// Events (polled per frame, not callback-based — keeps it thread-safe)
	virtual NetworkEvent PollEvent() = 0;

	virtual ~INetworkProvider() = default;
};

// Flags for Send()
enum SendFlags : uint32_t {
	SEND_RELIABLE    = 1 << 0,  // TCP-style: guarantee delivery and order
	SEND_UNRELIABLE  = 1 << 1,  // UDP-style: best-effort, low latency
	SEND_ENCRYPTED   = 1 << 2,  // Extension handles its own encryption layer
};
```

#### Built-in providers (shipped with engine core)

| Provider | Protocol | Use case |
|:---|:---|:---|
| `TcpNetworkProvider` | TCP | Turn-based, low-frequency data |
| `UdpNetworkProvider` | Raw UDP | Custom real-time, maximum control |

#### Developer-written providers (network extensions)

A developer writes their own `INetworkProvider` implementation as an extension:

```json
// extension.json for a Steam networking extension
{
"id": "com.taiax.steam-network",
"type": "network-provider",
"permissions": ["network.client", "network.server"],
"entry_point": "steam_provider.luc"
}
```

```luc
// steam_provider.luc — Luc wraps the Steamworks C++ SDK via FFI

import "steam_sdk.luc"   // FFI bindings to Steamworks

on_load(api) {
	// Register this as the active network provider
	api.network.set_provider(SteamNetworkProvider)
}

// The provider itself — implements the INetworkProvider contract via FFI
SteamNetworkProvider {
	connect(address, port) { return steam.CreateConnection(address) }
	send(conn, data, flags) { steam.SendMessage(conn, data) }
	poll_event() { return steam.ReceiveMessages() }
}
```

Developers can implement providers for:
- **Steam Networking Sockets** (Relay + P2P)
- **Epic Online Services** (Crossplay sessions)
- **WebSocket** (browser clients, web dashboards)
- **WebRTC** (browser P2P)
- **Custom UDP + QUIC** (full control)
- **Any cloud provider** (AWS GameLift, Photon, Mirror, etc.)

---

#### Security boundaries the engine enforces (regardless of provider)

The kernel manages the buffer layer — extensions cannot bypass it:

```cpp
// kernel/src/network/network_manager.cpp

void NetworkManager::Send(ConnHandle conn, const uint8_t* data, uint32_t size) {
	// 1. Enforce max packet size — prevent buffer overflow attacks
	if (size > MAX_PACKET_SIZE) { LogError("Packet too large"); return; }

	// 2. Rate limiting — prevent accidental (or malicious) flood
	if (!rate_limiter.Allow(conn)) { LogWarn("Rate limit hit"); return; }

	// 3. Hand off to the extension's provider — the kernel never sees the bytes after this
	active_provider->Send(conn, data, size, flags);
}
```

**What the kernel enforces:**
- ✅ Max packet size (configurable, default 64KB)
- ✅ Send rate limiting per connection
- ✅ Connection count limits
- ✅ `network.client` / `network.server` permission checks before any call

**What the developer controls:**
- ✅ Transport protocol (TCP, UDP, WebSocket, custom)
- ✅ Encryption (TLS, DTLS, custom AES, or none — their choice)
- ✅ Reliability / ordering layer (if using raw UDP)
- ✅ Server provider (self-hosted, Steam, Epic, Photon, etc.)
- ✅ Serialization format (JSON, Protobuf, Flatbuffers, raw binary)

> [!IMPORTANT]
> The engine **never** reads or inspects packet payloads. It only enforces size and rate limits at the boundary. All encryption, serialization, and protocol logic is entirely the developer's responsibility — this is intentional.

---

### Decision 9 — IDE Bottom Panel & Console Ecosystem ✅

**Architecture: The VS Code / Unreal "Unified Feedback" Model.** 
The IDE docks a primary feedback panel at the bottom, split into four distinct contexts.

| Tab | Context | Data Source | Interaction |
|:---|:---|:---|:---|
| **Terminal** | OS Shell | PowerShell / Bash | Bi-directional (Run git, build scripts) |
| **Output** | Engine Logs | Kernel stdout/stderr | Read-only (Filters: Error, Warn, Info) |
| **Problems** | Compilation | `luc_langserver` | Read-only (Clickable diagnostics) |
| **Engine Console** | Runtime Commands | `console_interpreter.cpp` | Interactive REPL (Reflected memory access) |

#### The Console Ecosystem (The 3-File System)
The "Engine Console" is not a text parser; it is a live memory bridge.

1.  **The Interpreter (`luc_console.cpp`):** Core kernel logic. Receives string → Tokenizes → Finds memory via Symbols → Executes.
2.  **The Metadata (`symbols.json`):** Generated by the Luc compiler. Maps human names (e.g., `player.health`) to binary offsets (e.g., `0xAC + 8`).
3.  **The Shortcuts (`debug_commands.luc`):** User-defined Luc functions for complex testing (e.g., `spawn_boss()`).

---

### Decision 10 — AES Key Hardening (Two-Step DRM) ✅

**Architecture: The "Console-Style" Decoupled Key Model.**
Instead of hiding a single master key in the C++ DLL, the engine uses a two-step derivation process to protect game assets (`.lucid` bundles) while allowing hardware-locked distribution.

| Component | Protection | Distribution |
|:---|:---|:---|
| **Title Key** | AES-256 | Hidden inside the `license.lge` ticket. |
| **VFS Bundle** | AES-256 | Encrypted once with Title Key (Same 50GB file for all users). |
| **License Ticket** | PBKDF2/HKDF | Encrypted with **(Machine UUID + AppID)**. Unique per user. |

#### Why this works:
1.  **No Hardcoded Seeds:** The kernel derives the Ticket Key from the machine hardware at runtime. There is no secret string for a hacker to find in the DLL.
2.  **Multi-Device Friendly:** Store platforms (Steam/Epic) can generate multiple 1KB tickets for a single user (one for Desktop, one for Steam Deck), all containing the same Title Key.
3.  **Hardware Locked:** Copying the game folder to a different PC fails because the hardware fingerprint won't unlock the `license.lge` ticket.

---

### Decision 11 — UI Icon Architecture ✅

**Architecture: Pure SVG (Vector) with Kernel Rasterization.**
To achieve a premium, VS Code-like aesthetic with infinite DPI scaling and seamless Light/Dark theme switching, the engine **will not use PNG files for UI icons**. All IDE and extension icons must be SVG.

#### How it works (The Architecture):
1. **The Rasterizer:** The C++ Kernel embeds a lightweight, single-header SVG library (e.g., [NanoSVG](https://github.com/memononen/nanosvg)) into `externals/`.
2. **The Pipeline:** When the Luc UI layer requests `ui.draw_svg("icon.svg", 24, 24)`, the kernel reads the file, rasterizes the vector math into a 24x24 RGBA texture, uploads it to the GPU via Vulkan, and caches it.
3. **Core Icons:** All default engine icons (Explorer, Debug, Git, folder icons, file type icons) are stored centrally inside `core_extensions/theme_manager/icons/`.

#### How developers create an Icon Theme (Step-by-Step):
Just like VS Code, users can install "Icon Theme Extensions" to completely replace the core icons. 
1. **Create the Extension:** Generate a new extension folder.
2. **Define the Type:** In `extension.json`, declare it as an icon theme:
```json
"contributes": {
	"iconThemes": [{ "id": "my-cool-icons", "label": "Cool Icons", "path": "icon_map.json" }]
}
```
3. **Provide the SVGs:** Create a folder inside the extension (e.g., `icons/`) and drop in the pure white SVG files.
4. **Map the IDs:** Create the `icon_map.json` file. This tells the engine which SVG maps to which core feature:
```json
{
	"iconDefinitions": {
		"explorer_icon": { "iconPath": "icons/my_explorer.svg" },
		"luc_file": { "iconPath": "icons/luc_script.svg" }
	},
	"core": {
		"activityBar.explorer": "explorer_icon"
	},
	"fileExtensions": {
		"luc": "luc_file"
	}
}
```
5. **Activate:** When the user goes to `Settings > Icon Theme` and selects "Cool Icons", the engine reads `icon_map.json` and swaps the SVGs instantly.

#### Color Themes vs. Icon Themes
**Can one extension handle both? Yes.** 
Color Themes (which change the background colors and text colors) and Icon Themes (which change the SVGs) are two separate *settings* in the engine, but they can be bundled inside the **same extension package**. 
Your `extension.json` can contribute both:
```json
"contributes": {
	"colorThemes": [{ "id": "my-dark-theme", "path": "colors.json" }],
	"iconThemes": [{ "id": "my-cool-icons", "path": "icon_map.json" }]
}
```
The user will still activate them separately in the engine settings (e.g., Active Color Theme: *My Dark Theme*, Active Icon Theme: *My Cool Icons*).

#### Conflict Resolution (What overwrites what?)
What happens if two extensions try to change the same icon? The engine has strict rules to prevent "extension wars":

**Rule 1: Core Icons (e.g., Explorer, Search)**
Standard extensions **cannot** overwrite core icons. Only the single, user-selected **Active Icon Theme** can change core icons. Therefore, conflicts are impossible because only one Icon Theme is active at a time in `engine_settings.json`.

**Rule 2: File Type Icons (e.g., `.json` files)**
If two regular extensions both try to register an icon for `.json` files via the Luc API, the engine uses the following hierarchy:
1. **The Active Icon Theme wins.** If the user's selected icon theme has a `.json` icon, it overrides everything else.
2. **Standard Extension Registration.** If the icon theme doesn't have one, but Extension A and Extension B both register an icon for `.json`, the engine picks the one that loads last (based on the alphabetical order of their Extension UUIDs). The engine will log a quiet warning to the developer console: `Warning: Extension B overwrote the .json file icon previously registered by Extension A.`

#### How Colors are Handled:
Because SVGs are drawn dynamically, **all UI SVGs should be solid white (`#FFFFFF`)**.
When the Luc UI code calls `ui.draw_svg`, it passes an active theme color. The Vulkan shader multiplies the white texture by this color. 
- If the dark theme is active, it tints the white SVG to `rgb(200, 200, 200)`. 
- If the user hovers over the icon, the shader dynamically tints it to `rgb(50, 150, 255)` (Lucid Blue). 
This allows users to change the engine's color palette without ever having to touch the icon files!

#### Extension Icons: 
User extensions bundle their own `.svg` files in their folder and register them via the `api` during the `on_load` hook:
```luc
export const on_load (api &Api) = {
    api.ui:register_activity_bar("Animation", "resources/anim_panel.svg", open_anim)
    api.ui:register_file_icon(".lanim", "resources/anim_file.svg")
}
```


**Why this is the right call:** While PNGs are easier to implement initially (since the texture loader already exists), they scale poorly on 4K/Retina displays and fail when the UI theme changes (e.g., black PNGs vanish on a dark theme). SVGs allow the Vulkan shader to dynamically tint the math-generated curves to perfectly match the active theme.

---

### Decision 12 — The Extension API Surface (The `api` object) ✅

**Architecture: The "God Object" passed via FFI.**
To achieve "infinite scale for infinite demands," the C++ Kernel exposes a massive, modular API surface to `main.luc`. When the engine loads an extension, it passes an `api` object into the `on_load(api)` function.

The features available inside the `api` object are **strictly determined by the permissions** requested in `extension.json`.

#### The Subsystem Namespaces:
If an extension requests all permissions, the `api` object contains:

*   **`api.workspace`**: Manage IDE tabs. Open files, register **Custom Editor Providers** (like your custom drawing tab), read the active file, or listen for drag-and-drop events from the Explorer.
*   **`api.ui`**: Draw custom panels, buttons, and most importantly, instantiate a **Vulkan Canvas** (`api.ui.create_canvas()`) to draw raw pixels, lines, and shapes directly to the GPU.
*   **`api.fs`**: Read and write files. Create directories, or prompt the user with a File Save Dialog.
*   **`api.input`**: Read raw mouse X/Y and keyboard states for interactive custom tabs.
*   **`api.render`**: Hook into the Vulkan rendering pipeline. Override the default shaders, add post-processing effects, or change how shadows are calculated.
*   **`api.physics`**: Hook into Jolt Physics. Change the gravity solver, intercept collision events, or modify rigidbodies.
*   **`api.compiler`**: Hook into the LSP (Language Server). Register custom **Autocomplete Providers**, linting rules, or code-generation tools.

#### Example: Building a Custom Drawing Tool
Here is exactly how `main.luc` would look for an extension that lets the user open `.ldraw` files, draws pixels in a custom workspace tab, handles drag-and-drop folders, and saves the file.

```luc
-- main.luc (The Drawing Extension)
package drawing_tool

use engine.api

-- Extensions use on_load as their entry point
export const on_load (api &Api) = {
    -- 1. Register a provider for .ldraw files
    -- We pass the constructor reference DrawingEditor:new
    api.workspace:register_editor_provider(".ldraw", DrawingEditor:new)
}

pub struct DrawingEditor {
    file_uri   string
    canvas     &Canvas
    pixel_data [*]byte
}

pub impl DrawingEditor {
    -- Static constructor called by the engine when a tab opens
    new (file_uri string, tab &Tab) DrawingEditor = {
        return DrawingEditor {
            file_uri   = file_uri
            canvas     = api.ui:create_canvas(tab)
            pixel_data = api.fs:read_file(file_uri)
        }
    }

    -- Called every frame to draw the UI inside the tab
    on_ui_render (ui &Ui) = {
        -- 1. Draw a Toolbar
        ui:begin_toolbar()
        
        if ui:button("Save Drawing") {
            api.fs:write_file(file_uri, pixel_data)
        }
        
        -- Drag-and-drop target to change the save location
        let out_path string = ""
        if ui:drop_target("Drag Folder Here", &out_path) {
            file_uri = out_path + "/new_drawing.ldraw"
        }
        
        ui:end_toolbar()

        -- 2. Draw the interactive Canvas
        canvas:draw_pixels(pixel_data)
        
        -- 3. Handle Mouse Input for painting
        if api.input:is_mouse_down(MouseButton.Left) {
            let pos Vec2 = api.input:mouse_position(canvas)
            pixel_data:set_pixel(pos.x, pos.y, Color.Red)
        }
    }
}
```

#### Why this scales infinitely:
Because the C++ Kernel exposes the raw `api.ui:create_canvas()` and `api.render` hooks, the engine doesn't need to know *what* a drawing tool is. The engine simply provides a blank tab and a GPU canvas, and the Luc script defines the logic. This is exactly how VS Code's `CustomEditor` API allows extensions to build hex editors, 3D model viewers, and UI designers entirely via extensions.

---

### Decision 13 — Engine Branding & App Icons ✅

**Architecture: Hybrid Vector/Raster Branding.**
To maintain the engine's "premium" feel while complying with OS limitations, the branding system uses a dual-path approach.

#### 1. Internal Engine Icons (SVG)
The main engine logo (shown in the top-left corner of the window, the splash screen, and the "About" dialog) is stored as a **pure white SVG**.
- **Location:** `core_extensions/theme_manager/branding/logo.svg`
- **Logic:** Like UI icons, it is rasterized by NanoSVG and tinted via Vulkan shaders to match the user's active theme.

#### 2. External OS Icons (ICO/PNG)
Windows and Linux cannot use the engine's internal rasterizer for the desktop shortcut or taskbar icon.
- **Location:** `dist/images/app_icon.ico` (Windows) and `dist/images/app_icon.png` (Linux).
- **Logic:** These are pre-baked raster files. They do not change with the theme because the OS manages their display before the engine even boots.

#### 3. How to load the App Icon in Luc
The engine branding is exposed via the `api.kernel` namespace:
```luc
let logo &Texture = api.kernel:get_branding_logo()
-- Then draw it anywhere in the UI
ui:image(logo, 64, 64)
```

---

### Decision 14 — Unified UI Architecture (RmlUI + Luc Bridge) ✅

**Architecture: The "Unified UI Kernel" Model.**
To achieve a modern, minimal VS Code-like aesthetic for the editor while providing a powerful, design-first workflow for game developers, the engine standardizes on **RmlUI** (v6.0+) as its core UI backend.

#### 1. The Dual-Role Architecture
RmlUI serves as the high-performance renderer for two distinct contexts:
*   **The Engine Shell:** The IDE's activity bar, sidebar, status bar, and tabs are built using RmlUI in the Kernel. This ensures a consistent, modern "App" feel with native support for Flexbox, glassmorphism (blur), and dynamic themes via CSS variables.
*   **The In-Game UI:** Developers build their own HUDs and menus using the same system. The engine provides a **Visual UI Designer** that generates RML/RCSS files, which are then controlled via Luc code.

#### 2. The Luc UI Bridge (Declarative UI)
Extension and game developers do not write HTML/CSS directly. Instead, they use a high-level, declarative Luc API that maps to RmlUI elements. This ensures type safety and performance while maintaining the flexibility of a DOM-based system.

```luc
-- Example: A custom panel in the Activity Bar
export const on_render_panel (ui &Ui) = {
    ui:div("sidebar-container", () = {
        ui:header("SEARCH")
        
        ui:input_field("Search query...", &this.query)
        
        ui:scroll_view("results-list", () = {
            for result in this.results {
                ui:button("result-item", result.label, () = {
                    api.workspace:open_file(result.path)
                })
            }
        })
    })
}
```

#### 3. Why RmlUI?
*   **Modern Aesthetics:** Supports `box-shadow`, `filter: blur()`, and `conic-gradient` out of the box.
*   **Performance:** Renderer-agnostic backend; generates simple vertex/index batches for our Vulkan pipeline.
*   **Standard-Based:** Uses HTML/CSS paradigms, making it easy to recruit designers who already understand web-style layouts.
*   **MIT License:** Fully permissive and actively maintained.

---

### Decision 15 — The Lucid-UI Bridge & Core Component Library ✅

**Architecture: The "Reflected DOM" Model.**
To ensure extensions can build modern UIs without fighting raw RmlUI pointers, the engine provides a high-level **Reactive Bridge**. The Kernel manages the RmlUI element tree, while Luc interacts with it via safe, handle-based references.

#### 1. The UI Bridge API (`ui.luc`)
The bridge maps the C++ DOM to a set of scoped Luc functions. This allows developers to build UIs using a structure that mirrors the visual hierarchy.

```luc
-- Example: Defining a reusable Component in Luc
pub const ToolCard (title string, desc string, on_click () -> void) = {
    ui:div("card", () = {
        ui:header("card-title", title)
        ui:text("card-desc", desc)
        ui:button("card-action", "Open", on_click)
    })
}
```

#### 2. The Core UI Library (Standard Components)
The engine ships with `core_lib/ui_std.luc`, a library of "Lucid-Styled" components that ensure all extensions look like they belong in the engine.
*   **Containers:** `panel`, `grid`, `flex_row`, `scroll_view`.
*   **Controls:** `button`, `toggle`, `slider`, `input_field`, `dropdown`.
*   **Feedback:** `progress_bar`, `spinner`, `tooltip`, `toast`.

These components are pre-styled with the **Lucid Design System** (Inter font, 4px rounding, semi-transparent backgrounds).

#### 3. Infinite Extensibility (The Expansion Model)
Extensions can expand the UI system in two ways:
1.  **Global Styles (RCSS):** An extension can provide a `.rcss` file that adds new CSS classes to the global namespace.
    *   *Example:* A "Neon Theme" extension injects global styles that add glows to all `ui:button` instances.
2.  **Custom Layout Builders:** Extensions can define their own complex Luc functions (like `ToolCard` above) and export them for other extensions to use.

#### 4. Data Binding (The Bridge Magic)
The bridge supports **Reactive Binding**. When a Luc variable is bound to a UI element, the Kernel updates the RmlUI property only when the variable changes, ensuring zero-cost UI updates.

```luc
let health int = 100
-- Binding the 'health' variable to a progress bar
ui:progress_bar("health-bar", &health) 
-- Any change to 'health' in Luc is instantly reflected in the HUD
```

---

### Decision 16 — The Math Library (GLM + Luc Projection) ✅

**Architecture: The "Dual-Citizen" Math Model.**
To ensure high-performance rendering and physics while maintaining a world-class Luc developer experience, the engine standardizes on **GLM (OpenGL Mathematics)** as its internal C++ foundation.

#### 1. C++ Foundation (The Kernel)
The Kernel uses GLM for all transformations, projection matrices, and physics calculations.
*   **Zero Overhead:** GLM is header-only and SIMD-accelerated.
*   **Vulkan-Native:** Memory layouts of `glm::vec3` and `glm::mat4` match Vulkan/GLSL expectations exactly.

#### 2. The Luc Projection (`math.luc`)
The engine exposes these types to Luc as primitive structs. Heavy math operations are projected via FFI to GLM's optimized C++ implementation.

```luc
-- core_lib/math.luc
package math

-- @packed ensures Luc memory matches GLM memory exactly
@packed
pub struct Vec3 {
    x float
    y float
    z float
}

pub impl Vec3 {
    -- Heavy math calls the C++ GLM backend
    dot (other Vec3) float = {
        return api.math:vec3_dot(this, other)
    }
}
```

#### 3. Why GLM?
*   **Industry Standard:** Widely used in custom engines, Vulkan tools, and research.
*   **Stability:** Decades of bug fixes and performance optimizations.
*   **Interoperability:** Eases the bridge between Luc scripting and the Vulkan renderer.

---

### Decision 17 — Input Architecture (The GLFW ↔ Luc Bridge) ✅

**Architecture: The "Foundation-Surface" Model.**
To ensure the engine is highly portable across Desktop, Mobile, and Console while maintaining a premium developer experience, the input system is split into two layers.

#### 1. The Low-Level Foundation (C++ Kernel)
The Kernel uses **GLFW** (on Desktop) as a platform provider. It captures raw OS events and normalizes them into a unified internal format.
*   **Decoupling:** GLFW is treated as an implementation detail. The Kernel uses an abstract `IInputProvider` interface, allowing us to swap GLFW for native **Android NDK**, **iOS UIKit**, or **Console SDKs** without changing any Luc code.

#### 2. The High-Level Surface (Luc `io` Library)
Developers interact with the beautiful, callback-based `io` library. This is the "Surface" that remains constant regardless of the hardware.

```luc
-- Example: Desktop-first logic that works on Mobile via re-mapping
io.key.W.onHeld(() {
    player:move_forward()
})

-- Mobile touch simulation on PC
io.mouse.left.onPressed(() {
    io.printl("Touch/Click at: " + string(io.mouse.x()))
})
```

#### 3. Cross-Platform Portability (The "Swappable Backend")
*   **PC Testing:** Developers can test Mobile/Console logic on PC by mapping mouse/keyboard events to virtual touch/gamepad events.
*   **Native Performance:** Because the Kernel talks directly to the platform (GLFW/NDK/SonySDK), input latency is kept to an absolute minimum, while the Luc layer remains elegant and high-level.

---

### Decision 18 — ECS Reflection & The Property Inspector ✅

**Architecture: The "Component Metadata" Model.**
The Property Inspector (docked on the right) is a Luc-based UI that dynamically generates its fields by querying the **ECS Metadata** of the selected entity.

#### 1. How Selection Works
When you click an object in the 3D Scene View, the engine performs a **Raycast** (via Jolt Physics). The Kernel returns the `EntityID` to the Luc Editor:
```luc
on_scene_click (pos Vec2) = {
    let entity_id EntityID = api.physics:raycast_from_camera(pos)
    api.editor:set_selection(entity_id)
}
```

#### 2. The Property Inspector (Right Dock)
The Inspector uses the **Reactive Bridge (Decision 15)** to bind UI elements directly to the ECS memory. 

```luc
-- engine/src/editor/inspector_panel.luc
export const render_inspector (entity &Entity) = {
    ui:panel("inspector", () = {
        -- Physics Component
        if entity:has_component(PhysicsComponent) {
            let phys = entity:get_component(PhysicsComponent)
            ui:header("PHYSICS")
            ui:number_input("Mass (kg)", &phys.mass)
            ui:vec3_input("Velocity", &phys.velocity)
            ui:checkbox("Use Gravity", &phys.use_gravity)
        }

        -- Material Component
        if entity:has_component(MaterialComponent) {
            let mat = entity:get_component(MaterialComponent)
            ui:header("MATERIAL")
            ui:dropdown("Type", ["Glass", "Stone", "Metal"], &mat.type_id)
            ui:asset_picker("Albedo Texture", &mat.texture_path)
        }
    })
}
```

#### 3. Automatic UI Generation (Reflection)
The engine reuses the `symbols.json` schema to know which fields in a component are editable. This allows the editor to automatically generate sliders, checkboxes, and pickers for any custom component added by an extension.

---

### Decision 19 — The Core Component Suite ✅

To ensure the engine is ready for both 2D and 3D out of the box, we define a standard set of "High-Performance" components in the Kernel.

#### 1. Core Structural Components
*   **TransformComponent:** Position, Rotation (Quat), Scale (Vec3). (Z-axis locked for 2D).
*   **CameraComponent:** FOV, Orthographic/Perspective toggle, clipping planes.
*   **RelationshipComponent:** Tracks entity hierarchy (Parent/Child).

#### 2. Visual & Media Components
*   **SpriteComponent:** 2D-optimized quad with sorting layer and texture path.
*   **MeshComponent:** 3D-optimized model link (`.lmesh`) and Material pointer.
*   **AnimationComponent:** Controls state playback for skeletal or sprite-sheet animations.
*   **AudioComponent:** Handles 2D/3D spatialized audio playback (`.ogg`).

#### 3. Physics & Interaction Components
*   **PhysicsComponent:** Mass, Friction, Restitution, Velocity (Dynamic/Static/Kinematic).
*   **ColliderComponent:** Defines the collision shape (Box, Sphere, Polygon).

---

### Decision 20 — The Console Execution Pipeline ✅

**Architecture: The "Tri-Level Dispatcher."**
The `ConsoleInterpreter::Execute()` function acts as a triage center, processing every command string through three increasingly deep layers of the engine:

#### Layer 1: The Command Registry (Fast-Track)
The interpreter first checks a `std::unordered_map` of registered "Engine Commands" (ConCmds). These are high-performance C++ functions used for engine-level control (e.g., `quit`, `r.wireframe 1`). This avoids any compilation overhead.

#### Layer 2: The Symbol Metadata Bridge (Live Memory)
If the input is an assignment (e.g., `player.speed = 25`), the interpreter enters **Reflection Mode**. It uses `symbols.json` to find the exact memory offset in the C++ ECS component store and writes the new value directly into RAM. Strict type-checking is enforced before writing.

#### Layer 3: The Luc REPL (Scripting Layer)
If the command is complex logic (e.g., calling macros like `world.spawn_horde(50)`), the interpreter passes the string to the **Luc JIT Compiler**. The JIT engine compiles and executes the snippet on the fly.

---

### Decision 21 — The Distribution Model ✅

**Architecture: "Stable Kernel, Dual-Mode Logic Distribution."**
The engine supports two distinct compilation paths for game logic, allowing developers to balance peak performance against cross-platform portability.

| Component | Format | Compilation | Shipping Responsibility |
|:---|:---|:---|:---|
| **Kernel** | `luc_kernel.dll` | C++ Native | The pre-compiled bedrock. |
| **Logic (AOT)** | `game.lmod` | **AOT** (Native) | Best for performance. Requires per-platform builds (Win/Linux). |
| **Logic (JIT)** | **`game.ljit`** | **JIT** (Bytecode) | Best for portability. One build runs on any kernel-supported OS. |
| **Assets** | `.pck` | Encrypted VFS | All textures, meshes, and audio bundles. |

**Rationale:**
- **AOT (`.lmod`):** Compiles Luc directly to platform-specific machine code (Win-x64, Linux-ARM64). Offers the highest possible performance and best obfuscation.
- **JIT (`.ljit`):** Compiles Luc to secure LLVM Bytecode. The `luc_kernel` JIT-compiles this to memory on boot. This enables "Build Once, Run Anywhere" and is the ideal format for cross-platform distribution and modding.

---

### Decision 22 — The Audio Pipeline ✅

**Architecture: "Fast-Trigger SFX + Compressed Streams."**
The engine uses **miniaudio** (C++) to handle low-latency playback. Audio is baked into two optimized formats to balance CPU and disk usage.
*   **`.lsfx` (Short Sounds):** Uses **MS-ADPCM** compression. Decoding is near-instant, allowing hundreds of concurrent sounds (footsteps, impacts) with zero CPU hit.
*   **`.lstream` (Long Sounds):** Uses **Ogg Vorbis** compression. Highly compressed for music and ambience; streamed in chunks from the disk to minimize RAM usage.

#### Why miniaudio?
It is a single-header, MIT-licensed backend that handles all OS-level audio drivers (WASAPI, ALSA, CoreAudio) natively without external dependencies.

---

### Decision 23 — The Save-Game Security Model ✅

**Architecture: "Policy-Based Binary Saves."**
Saves are stored in **MessagePack** format. The Kernel enforces one of three security levels defined in `engine_settings.json`:
*   **Level 0 (Plain):** No encryption. Encourages modding and community tools.
*   **Level 1 (Encrypted):** Uses **AES-256-GCM** with a Project-Unique Key. Prevents casual cheating while allowing players to share saves.
*   **Level 2 (Locked):** Uses **AES-256-GCM** keyed to the local **Hardware UUID**. Prevents "save trading" across different PCs.

---

### Decision 24 — Production Build Integrity ✅

**Architecture: "The Secure Header Check."**
To prevent developers from accidentally shipping insecure debug builds (which allow console memory injection), the Kernel performs a boot-time check on the `game.lmod` binary.
*   **The Guard:** Shipping builds of `luc_kernel.dll` will refuse to load any module that contains the `DEBUG_SYMBOLS` flag.
*   **Checksum:** The Kernel validates the module's SHA-256 hash against the manifest signature before execution.

---

### Decision 25 — Network Provider Strategy ✅

**Architecture: "Lean Core, Modular Web."**
*   **Core:** The Kernel provides high-performance **TCP and UDP** sockets via `api.net`.
*   **Extensions:** Modern web protocols like **WebSockets** and **gRPC** are provided as **Core Extensions** (`lucid.net.ws`). This keeps the base Kernel binary small for projects that don't need web connectivity.

---

### Decision 26 — Visual Programming Architecture ✅

**Architecture: "The Functional Data Graph."**
Visual programming in Lucid does not use a secondary virtual machine. It is a one-way generator that maps visual nodes directly to Luc's functional paradigms (`->` pipelines and `+>` composition).

#### 1. The Metadata Scanner (No Special Files Required)
Developers expose custom code to the visual editor using structured comments (`---@node`). The core compiler ignores these, ensuring cross-version stability, while the `luc_langserver` parses them to build the visual palette.

```luc
---@node(category="Math", name="Calc Damage", color="red")
pub const calc_damage (base float, type string) float = { ... }
```

#### 2. Visualizing the Flow (Arrows = Operators)
Because Luc treats functions as types, the graph's arrows map directly to code syntax:
*   **Data Wires (Thin):** Dragging an output to an input generates a Pipeline expression (`NodeA -> NodeB`).
*   **Composition Wires:** Connecting functions without executing them generates a composite (`math.add +> math.clamp`).
*   **Execution Wires (Thick):** Sequential side-effects map to standard block scopes (Line 1, then Line 2).

#### 3. Structs as Action Centers & Recursive Data
*   **Action Nodes:** Because structs in Luc use `impl` blocks for behavior, a Struct Node visually displays both **Data Pins** (properties) and **Action Pins** (methods).
*   **Recursive UI:** The UI handles nested data (e.g., Arrays containing Structs that contain Arrays) using collapsible accordion sections inside the node to prevent screen clutter.

#### 4. File Structure: Paging & Imports (draw.io style)
*   **Paging:** A single `.lgraph` file can contain multiple "Pages" (tabs at the bottom) to organize logic (e.g., "Movement", "Combat"). The compiler flattens all pages into a single `.luc` package.
*   **Import/Export UI:** The editor includes a visual manager for `use` (imports) and `export` to manage dependencies between visual graphs and raw code without manual typing.

---

### Decision 27 — The Vulkan RHI Architecture ✅

**Architecture: "The Bindless Abstraction Layer."**
The RHI (Render Hardware Interface) acts as the bridge between the engine logic and the raw Vulkan SDK. It is designed to be "opinionated," hiding Vulkan's verbosity while maintaining maximum performance.

#### 1. Core Responsibilities
*   **Memory:** Uses **VMA (Vulkan Memory Allocator)** for all GPU allocations (Buffers, Images).
*   **Sync:** Internal management of Fences and Semaphores. The engine kernel never touches `VkSubmitInfo` directly.
*   **Caching:** Automatic caching of Pipeline State Objects (PSOs) to prevent "shader stutter" during gameplay.

#### 2. The Bindless Model
To ensure maximum compatibility with Luc's dynamic nature, the RHI uses a **Bindless Descriptor** model.
*   All textures and buffers are stored in a global array on the GPU.
*   Shaders access resources via a simple integer index (e.g., `textureSampler[textureID]`).
*   This eliminates the need for complex per-object descriptor set binding, significantly reducing CPU overhead.

#### 3. Simplified Lifecycle
The RHI provides a high-level API for the Kernel:
*   `BeginFrame()` / `EndFrame()` / `Submit()` / `Present()`
*   Handles swapchain recreation (window resizing) automatically without crashing the renderer.

---

### Decision 28 — The Core Component Schema ✅

**Architecture: "The POD Bedrock."**
To maintain maximum performance and cache locality, all core engine components are defined as POD (Plain Old Data) structs. This ensures they can be serialized, mirrored to Luc, and processed by C++ systems with zero heap allocation.

#### 1. The Relationship System (ERS)
Instead of a dynamic scene tree, Lucid uses a **Doubly Linked-List Hierarchy** embedded in the components. This allows for an infinite number of children per entity with zero memory fragmentation.

```cpp
struct RelationshipComponent {
    EntityID parent;         // 0 if root
    EntityID first_child;    // Entry point to child list
    EntityID next_sibling;   // Next child in the same parent
    EntityID prev_sibling;   // Prev child in the same parent
    uint32_t depth;          // Hierarchy depth for sorted updates
};
```

#### 2. Visual & Geometry Components
*   **`MeshComponent`**: Links to a `.lmesh` asset and a material.
*   **`PrimitiveComponent`**: Procedural shapes (Box, Sphere, Capsule) for rapid prototyping.
*   **`SpriteComponent`**: 2D quad with texture, pivot, and sorting layer support.

#### 3. Media & Interaction Components
*   **`AudioComponent`**: Handles 2D/3D spatialized audio (`.lsfx` or `.lstream`).
*   **`AnimationComponent`**: Manages state, playback rate, and blending for skeletal/sprite animations.
*   **`UIComponent`**: Attaches an **RmlUI** context (World-space or Screen-space) to an entity.

#### 4. Physics & Colliders (Jolt Bridge)
*   **`PhysicsComponent`**: Stores mass, friction, velocity, and motion type (Static/Dynamic).
*   **`ColliderComponent`**: Defines the physical volume (Box, Sphere, Capsule, Mesh) and local offset.

---

### Decision 29 — Development Workflow & SDK Distribution ✅

**Architecture: "The Live Workspace Model."**
To ensure a frictionless transition from "Engine Developer" to "Game Developer," we define a dual-layered build and testing strategy.

#### 1. The Development Workspace (`dev_build/`)
Instead of manually moving binaries, the C++ build system (CMake) is configured to output directly into a `dev_build/` directory at the repository root.
*   **Binaries:** `luc_kernel.dll` and `luc_compiler.exe` are updated in `dev_build/bin/` upon every successful C++ compile.
*   **Live Logic:** To prevent redundant copying, the `core_lib/` and `engine/` folders are **symlinked** into `dev_build/`. This allows Luc code changes to be reflected instantly without a re-compile.
*   **Shortcut:** Developers can create a desktop shortcut to `dev_build/bin/LucidEditor.exe` for one-click engine testing.

#### 2. The SDK Installer (`LucidSetup.exe`)
For public distribution, a dedicated packaging tool (`tools/package_sdk.py`) gathers the following components into a single installer:
*   Pre-compiled Kernel and Compiler binaries.
*   The standard `core_lib` and official extensions.
*   **Environment Setup:** The installer registers the `.luc` file association, adds the compiler to the system `%PATH%`, and initializes the `%APPDATA%/LucidEngine` directory.

#### 3. Testing Strategy
*   **Kernel Tests (`tests/kernel/`)**: C++ unit tests for RHI, ECS, and VFS cores (GoogleTest/Catch2).
*   **Logic Tests (`tests/logic/`)**: Functional Luc scripts that verify engine features (Physics, Rendering, Input).
*   **Performance Benchmarks**: Automated frame-time and memory tracking within the `dev_build/` environment.

---

## Part 3 — Architecture & Distribution Structures

To clarify the "Modular Build Model" (Decision 21), we define three distinct folder structures. This separates the engine development environment from the end-user game package.

### A. The Engine Source Repository (GitHub)
This is the repository that the engine developers (you) work in. It is compiled once.

```text
Lucid-Game-Engine/
├── kernel/                             ← C++ → luc_kernel.dll
│   ├── include/                        ← Public C-ABI (FFI contract)
│   │   ├── lge_api.h                   ← LGE_ExtensionAPI struct, LGE_GetAPI()
│   │   ├── lge_render.h
│   │   ├── lge_input.h
│   │   ├── lge_physics.h               ← Jolt bridge headers
│   │   └── lge_vfs.h
│   └── src/                            ← Internal implementation
│       ├── core/
│       │   ├── kernel.cpp              ← Boot sequence, frame loop
│       │   ├── clock.cpp               ← Delta-time, V-Sync pacing
│       │   ├── checksum.cpp            ← SHA-256 boot integrity
│       │   ├── file_watcher.cpp        ← Hot-reload file monitor
│       │   ├── script_manager.cpp      ← Compile → swap pipeline
│       │   └── console_interpreter.cpp ← String → Command execution logic
│       ├── ecs/
│       │   ├── world.cpp               ← Entity registry, archetype storage
│       │   ├── component_store.cpp     ← Contiguous component arrays
│       │   ├── system_scheduler.cpp    ← System ordering, parallel groups
│       │   └── components/             ← Core Component Implementations
│       │       ├── transform.cpp
│       │       ├── physics_body.cpp
│       │       ├── sprite_renderer.cpp
│       │       └── audio_source.cpp
│       ├── render/
│       │   ├── vulkan_rhi.cpp
│       │   └── sdf_font.cpp
│       ├── physics/
│       │   ├── physics_bridge.cpp      ← Jolt integration, LucContactListener
│       │   └── debug_renderer.cpp      ← Jolt → Vulkan Editor Pass wireframes
│       ├── input/
│       │   └── input_bridge.cpp
│       ├── vfs/
│       │   ├── vfs_reader.cpp          ← RAM-only AES-256 decrypt
│       │   └── vfs_packer.cpp          ← Pack raw assets → .lucid bundle
│       ├── security/
│       │   ├── license_verifier.cpp    ← Offline RSA/Ed25519 license check
│       │   ├── key_derivation.cpp      ← PBKDF2/HKDF(AppID + machine UUID)
│       │   └── extension_verifier.cpp  ← Signature check for extensions
│       └── platform/
│           ├── win32_loader.cpp        ← LoadLibrary, GetProcAddress
│           └── linux_loader.cpp        ← dlopen, dlsym
│       ├── platform/
│           ├── win32_loader.cpp        ← LoadLibrary, GetProcAddress
│           └── linux_loader.cpp        ← dlopen, dlsym
├── engine/                             ← The IDE Editor (Written in Luc)
│   └── src/                            
│       ├── main.luc
│       ├── ui/
│       │   └── workspace_manager.luc
│       ├── editor/
│       │   ├── scene_view.luc
│       │   ├── inspector_panel.luc
│       │   └── console_manager.luc
│       └── visual_programming/
│           └── node_graph.luc
├── core_lib/                           ← Standard library (math.luc, io.luc)
├── externals/                          ← Jolt, GLFW, Vulkan, RmlUI, GLM
│   └── luc_compiler/                   ← Submodule: https://github.com/Axo-1206/Luc-Compiler.git
└── CMakeLists.txt                      ← Builds both Kernel and Compiler
```

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

---

## Part 4 — Complete Design Decisions

> [!NOTE]
> **100% ARCHITECTURE COMPLETE.** All major design decisions have been locked in.

| # | Item | Status | Resolution |
|:--|:---|:---|:---|
| 1 | **Debug build guard** | ✅ Solved | Decision 24 (Secure Header Checks) |
| 2 | **Save-game data policy** | ✅ Solved | Decision 23 (AES-256-GCM Policy) |
| 4 | **Built-in network providers** | ✅ Solved | Decision 25 (TCP/UDP Core + WS Ext) |
| 5 | **Console `Execute()` body** | ✅ Solved | Decision 20 (Tri-Level Dispatch) |
| 6 | **`symbols.json` schema** | ✅ Solved | Decision 18 (ECS Reflection) |
| 8 | **Audio SFX format** | ✅ Solved | Decision 22 (ADPCM vs Vorbis) |
| 9 | **JIT bytecode extension** | ✅ Solved | Designated as `.ljit` |

---

## Part 5 — Decision Status Summary

| Decision | Choice | Status |
|:---|:---|:---|
| Scene model | ECS | ✅ Confirmed |
| FFI versioning | Function Table Versioning (Strategy 3) | ✅ Confirmed |
| Physics library | Jolt Physics (MIT) | ✅ Confirmed |
| Physics ↔ Render coupling | None — shared TransformComponent only | ✅ Confirmed |
| Licensing model | Offline-first, `ILicenseVerifier` interface for online migration | ✅ Confirmed |
| Scripting hot-reload | FileWatcher → Compiler Subprocess → Module Swapper | ✅ Confirmed |
| IntelliSense | `luc_langserver` process, LSP-style protocol, reuses compiler frontend | ✅ Confirmed |
| Extension system | VS Code-style, UUID v5 IDs, lifecycle hooks, Extension ≠ Library, local JSON | ✅ Confirmed |
| Extension ID uniqueness | UUID v5(LUCID_NAMESPACE, human_id) + publisher namespace for online | ✅ Confirmed |
| Network extension API | `INetworkProvider` interface, kernel enforces limits, dev owns protocol | ✅ Confirmed |
| Asset pipeline format | Custom binary formats (`.ltex`, `.lmesh`, `.lanim`), GLTF for source | ✅ Confirmed |
| ECS serialization format | Hybrid: JSON (Editor/Source) / MessagePack (Runtime/SaveGames) | ✅ Confirmed |
| Extension signing authority | `trusted_publishers.json` (Gist for Phase 1, API for Phase 2) | ✅ Confirmed |
| Publisher namespace registration | Discord DM (Phase 1), API registration (Phase 2), AES-256 encrypted keys | ✅ Confirmed |
| AES key hardening | Two-Step Derivation (Title Key + Hardware-locked License Ticket) | ✅ Confirmed |
| IDE Bottom Panel | 4-Tab Dock (Terminal, Output, Problems, Console) | ✅ Confirmed |
| Console Architecture | 3-File Ecosystem (Interpreter, Metadata, Shortcuts) | ✅ Confirmed |
| UI Icon Architecture | Pure SVG with NanoSVG rasterization in Kernel | ✅ Confirmed |
| Unified UI Architecture | RmlUI (CSS-based) for Shell + In-Game HUDs | ✅ Confirmed |
| Lucid-UI Bridge & Components | Reactive Luc Bridge + Core Styled Library | ✅ Confirmed |
| Math library | GLM (C++ Bedrock) + Luc Type Projection | ✅ Confirmed |
| Input Architecture | GLFW Foundation + Swappable Luc Surface | ✅ Confirmed |
| ECS Reflection | Auto-generating UI via `symbols.json` metadata | ✅ Confirmed |
| Core Component Suite | Standard blocks for 2D/3D (Transform, Audio, Physics...) | ✅ Confirmed |
| Console Architecture | Tri-Level Dispatch (ConCmds → Memory Map → Luc VM) | ✅ Confirmed |
| Distribution Model | C++ Kernel + AOT/JIT Luc Logic Modules | ✅ Confirmed |
| Audio Pipeline | Miniaudio with `.lsfx` (ADPCM) and `.lstream` (Vorbis) | ✅ Confirmed |
| Save-game Security | Policy-Based (Plain, Encrypted, Hardware-Locked) | ✅ Confirmed |
| Build Integrity | Kernel boot-time signature + DEBUG block | ✅ Confirmed |
| Network Architecture| Core TCP/UDP + Extensions (WebSockets, Steam, etc.) | ✅ Confirmed |
| JIT Bytecode Format | Standardized to `.ljit` extension | ✅ Confirmed |
| Visual Programming | Functional Data Graph, Recursive Structs, Paging | ✅ Confirmed |
| Vulkan RHI Architecture | Bindless, VMA-backed Resource Management | ✅ Confirmed |
| Core Component Schema | POD ERS, Visual, Media, and Physics blocks | ✅ Confirmed |
| Development Workflow | Live dev_build Workspace + SDK Installer | ✅ Confirmed |

---

## Part 6 — External Library Summary

To maintain the engine's "Microkernel" philosophy, we strictly limit external dependencies to libraries that provide high performance with minimal architectural "opinion."

| Library | Role | License | Rationale |
|:---|:---|:---|:---|
| **Vulkan SDK** | Graphics RHI | Apache 2.0 | Explicit GPU control, cross-platform, zero legacy baggage. |
| **VMA** | GPU Memory | MIT | Industry standard for efficient Vulkan memory allocation. |
| **GLM** | Math Library | MIT | Header-only, SIMD-accelerated. Supports Vulkan, DirectX, and Metal via config flags. |
| **GLFW** | Window & Input | Zlib | Lightweight, cross-platform; handles OS windowing and raw input. |
| **Jolt Physics** | Physics Engine | MIT | AAA-proven, extremely fast multi-threaded performance. |
| **RmlUI** | UI System | MIT | CSS-based layouts for the editor shell and in-game HUDs. |
| **NanoSVG / NanoVG** | Vector Graphics | zlib | Tiny footprint, used for UI icons and hardware-accelerated shapes. |
| **LLVM** | Compiler Backend | Apache 2.0 | Powers the Luc compiler's AOT and Secure JIT compilation. |
| **cgltf** | GLTF Loader | MIT | Single-header, high-performance parser for 3D source assets. |
| **MessagePack** | Serialization | Apache 2.0 | Fast binary serialization for runtime ECS and save games. |
| **nlohmann/json** | JSON Parser | MIT | Used for human-readable project metadata and source configs. |
| **miniaudio** | Audio Engine | MIT | Single-header, handles SFX and Streaming with zero external deps. |
| **MbedTLS** | Security | Apache 2.0 | Lightweight crypto for AES-256 (VFS) and Ed25519 (Extensions). |

### Dependency Management Strategy:
1.  **Vendor-In Tree:** All libraries (except large ones like LLVM/Vulkan SDK) are stored in `externals/` as source code. This ensures the engine is self-contained and builds are reproducible.
2.  **Static Linking:** We prefer static linking for core dependencies to minimize DLL-hell and improve startup time.
3.  **No "Bloat" Policy:** If a library includes features we don't need (e.g., a massive math suite or networking stack), we only include the specific headers/files required for our use case.

