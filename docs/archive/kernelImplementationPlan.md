# Kernel Implementation Plan

> **Source of truth:** `docs/EngineArchitecture.md`  
> **Supplementary reference:** `docs/kernel/kernel_implementation.md`  
> **Target directory:** `kernel/`  
> **Status:** All files listed below are **not yet implemented** — the `kernel/` folder is currently empty.

---

## Overview

The kernel (`luc_kernel.dll`) is the C++ bedrock of the Lucid Engine. It is the only part of the codebase that touches hardware, the OS, or third-party C++ libraries directly. Everything above it — the editor, game scripts, extensions — communicates with it exclusively through a stable C ABI exported by `LGE_GetAPI()`.

The kernel is divided into two top-level areas:

- **`kernel/include/`** — Public C-ABI headers. These define every type, struct, and function signature that crosses the DLL boundary. They must be implemented *before* any `src/` file they describe, because `src/` files `#include` them.
- **`kernel/src/`** — The subsystem implementations. These are internal C++ `.cpp` files that provide the bodies for the functions declared in `include/`. Subsystems have hard boot-time dependencies on each other, so implementation order matters (see below).

---

## Does Implementation Order Matter for `kernel/src/`?

**Yes — significantly.** The boot sequence in `kernel/src/core/kernel.cpp` (`LGE_Boot()`) initializes subsystems in strict dependency order. If subsystem B's implementation calls into subsystem A's API, then A must be implemented and functional before B can be written, tested, or integrated. The dependency chain is:

```
platform/ -> core/ (clock, console, file_watcher, checksum)
          -> security/ (key derivation, license/extension verification)
          -> vfs/ (needs security for AES key derivation)
          -> ecs/ (data backbone consumed by all runtime systems)
          -> render/ (needs platform window + reads ecs components)
          -> input/ (needs platform window handle for GLFW callbacks)
          -> physics/ (needs ecs + render for debug wireframe)
          -> audio/ (needs ecs + vfs for asset loading)
          -> ui/ (needs render for Vulkan/ImGui backends + vfs for .rml files)
          -> network/ (needs platform threads)
          -> core/ Part 2 (kernel.cpp + script_manager -- ties everything together last)
```

The sections in **Section 2 (`kernel/src/`)** are therefore ordered by this dependency chain, **not** by the order they appear in `EngineArchitecture.md`.

---

## Section 1 -- `kernel/include/`

The `include/` directory holds all public C-ABI header files. These headers define the *only* types and function signatures that cross the DLL boundary. Every header obeys three rules from `EngineArchitecture.md`:

1. **C types only** -- no STL containers, no C++ classes, no templates at the boundary.
2. **POD structs only** -- every struct is `memcpy`-safe and reflection-friendly.
3. **No `#ifdef _WIN32` inside these files** -- platform conditionals are confined to `src/platform/`.

Implement all of `include/` as a single first pass before touching any `.cpp` file in `src/`, since `src/` files `#include` these headers.

---

### `lge_arena.h`

**Purpose:** Defines `LGE_ArenaDescriptor` -- the POD struct that lets Lucid-owned arena memory be safely passed to C without per-pointer ownership tags. Also provides the `LGE_Arena_Contains()` inline ownership check.

**Key declarations:**
```c
typedef struct {
    uint8_t* base;   // start address of the arena region (immutable after creation)
    uint64_t size;   // total byte capacity in bytes
} LGE_ArenaDescriptor;

static inline int LGE_Arena_Contains(
    const LGE_ArenaDescriptor* arena, const void* ptr);
```

**Notes:**
- `LGE_Arena_Contains` is a static inline -- it lives entirely in this header, no `.cpp` counterpart.
- Field order and sizes must be bit-for-bit identical to Lucid's built-in `ArenaDescriptor` struct so no marshaling is needed when crossing the FFI boundary.
- The allocation cursor (`used`) is intentionally absent -- the Lucid runtime manages it internally; C code has no business reading or writing it.
- Every kernel subsystem that accepts Lucid arena-allocated memory includes this header and calls `LGE_Arena_Contains` in debug builds (compiled away under `NDEBUG`).

---

### `lge_platform.h`

**Purpose:** OS abstraction interface -- window creation, dynamic library loading, and thread primitives. This is the only layer where `LoadLibrary`/`dlopen`, `CreateThread`, and native window handles are allowed to appear.

**Key declarations:**
```c
void         Platform_Init(const char* title, uint32_t w, uint32_t h);
void*        Platform_GetNativeWindowHandle();

typedef void* ModuleHandle;
ModuleHandle Platform_LoadModule(const char* path);
void*        Platform_GetSymbol(ModuleHandle mod, const char* symbol);
void         Platform_UnloadModule(ModuleHandle mod);

typedef void* ThreadHandle;
ThreadHandle Platform_SpawnThread(void (*fn)(void*), void* arg);
void         Platform_JoinThread(ThreadHandle t);
```

**Notes:**
- This header is **internal only** -- not exposed to Lucid via `lge_ffi.lfi` and not part of the extension API.
- No `#ifdef _WIN32` inside this header. The conditional implementations live in `src/platform/win32_loader.cpp` and `src/platform/linux_loader.cpp`.

---

### `lge_console.h`

**Purpose:** Console command registration and dispatch. Defines the interface for registering named C++ function pointers as console commands and for invoking them by name string.

**Key declarations:**
```c
void LGE_Console_Register(
    const char* name,
    const char* description,
    void (*callback)(const char* args)
);

void LGE_Console_Exec(const char* command_string);

// IPC bridge for out-of-process execution (editor console -> running game .exe)
void LGE_Console_StartIPCServer(uint16_t port);
void LGE_Console_ConnectIPC(const char* host, uint16_t port);
```

**Notes:**
- Commands are stored internally in a flat hash map (name -> function pointer). No eval, no scripting layer -- just a direct function call at C++ speed.
- The IPC bridge enables the editor's console panel to inject commands into a separately running game `.exe` via a named pipe or socket.
- Lucid scripts call `api.console.exec()`, which routes through `LGE_Console_Exec`.

---

### `lge_ecs.h`

**Purpose:** The full Entity-Component-System public API. Defines `EntityID`, component registration, entity lifecycle, component add/get/remove, and the system iteration callback pattern.

**Key declarations:**
```c
typedef uint64_t EntityID;

uint32_t LGE_RegisterComponent(const char* name, size_t size, size_t align);

EntityID LGE_CreateEntity();
void     LGE_DestroyEntity(EntityID id);
void*    LGE_AddComponent(EntityID id, uint32_t component_type);
void*    LGE_GetComponent(EntityID id, uint32_t component_type);
void     LGE_RemoveComponent(EntityID id, uint32_t component_type);

void LGE_ForEach(
    uint32_t* component_types,
    uint32_t  type_count,
    void (*callback)(EntityID*, void** component_ptrs, uint32_t count)
);
```

**Notes:**
- `EntityID` is a plain `uint64_t`. No C++ class, no inheritance.
- `LGE_RegisterComponent` is called at boot (and by extensions at load time) to register component types without touching the ECS core.
- `LGE_ForEach` delivers a flat array of components per archetype bucket -- no pointer chasing per entity, cache-linear iteration.
- All component structs are POD (no constructors, no virtual methods, `memcpy`-safe).

---

### `lge_render.h`

**Purpose:** Vulkan RHI submission API. Defines texture and mesh upload/management, per-frame draw call submission, and the frame lifecycle hooks called by `kernel.cpp`.

**Key declarations:**
```c
typedef uint32_t TextureID;
typedef uint32_t BufferID;

TextureID LGE_UploadTexture(const void* pixels, uint32_t w, uint32_t h, PixelFormat fmt);
void      LGE_FreeTexture(TextureID id);

BufferID  LGE_UploadMesh(const Vertex* verts, uint32_t vcount,
                          const uint32_t* indices, uint32_t icount);

void LGE_DrawMesh(BufferID mesh, TextureID albedo, const Mat4* transform);

// Frame lifecycle -- called by kernel.cpp frame loop only
void LGE_RHI_BeginFrame();
void LGE_RHI_EndFrame();
void LGE_RHI_Present();
```

**Notes:**
- The RHI uses a **bindless descriptor** model -- all textures/buffers are registered into a global GPU-side array at load time and addressed by integer index. This removes per-draw descriptor set binding from the hot path.
- Pipeline State Objects (PSOs) are cached on first use and reused every frame -- no mid-game shader compilation stutter.
- `LGE_RHI_BeginFrame/EndFrame/Present` are called only by `kernel.cpp`'s frame loop. No other system drives the frame lifecycle directly.

---

### `lge_input.h`

**Purpose:** Input polling and callback registration. Maps GLFW keyboard, mouse, and gamepad events to a frame-synchronized snapshot accessible via Lucid scripts.

**Key declarations:**
```c
bool  LGE_Input_IsKeyHeld(KeyCode key);
bool  LGE_Input_IsKeyPressed(KeyCode key);    // true only on the first frame the key is down
bool  LGE_Input_IsKeyReleased(KeyCode key);
Vec2  LGE_Input_MousePosition();
Vec2  LGE_Input_MouseDelta();
float LGE_Input_MouseScrollDelta();

void LGE_Input_OnKeyPressed(KeyCode key, void (*callback)());
void LGE_Input_OnMouseButton(
    MouseButton btn, void (*on_press)(), void (*on_release)());
```

**Notes:**
- Raw GLFW callbacks fire on the OS message thread. The bridge buffers events into a frame-local state snapshot to decouple them from ECS/game-logic threads.
- `IInputProvider` is the swappable interface behind this header -- GLFW on desktop, Android NDK on mobile, console SDKs on consoles. Swapping providers requires no Lucid code changes.

---

### `lge_physics.h`

**Purpose:** Jolt Physics bridge API. Exposes body creation/destruction, force application, raycasting, and collision callbacks without exposing any Jolt C++ types across the boundary.

**Key declarations:**
```c
typedef uint32_t BodyID;

BodyID LGE_Physics_CreateBody(EntityID entity, const PhysicsBodyDef* def);
void   LGE_Physics_DestroyBody(BodyID id);
void   LGE_Physics_SetVelocity(BodyID id, Vec3 velocity);
void   LGE_Physics_AddForce(BodyID id, Vec3 force);

bool   LGE_Physics_Raycast(
    Vec3 origin, Vec3 dir, float max_dist, RaycastHit* out_hit);

void   LGE_Physics_SetContactCallback(
    void (*on_contact)(EntityID a, EntityID b, Vec3 contact_point));
```

**Notes:**
- All Jolt C++ types (`JPH::Body`, `JPH::PhysicsSystem`, etc.) stay behind the bridge -- none of them appear in this header.
- Physics results (positions, rotations) are written back into `TransformComponent` each frame via `LGE_GetComponent`. `RenderSystem` reads `TransformComponent` next in the frame -- Jolt and Vulkan never communicate directly.
- `LGE_Physics_Raycast` is used by the editor inspector for click-to-select entity picking.

---

### `lge_audio.h`

**Purpose:** Miniaudio-backed SFX and streaming audio mixer API. Covers loading, playback control, volume, and 3D spatialization.

**Key declarations:**
```c
typedef uint32_t AudioSourceID;

AudioSourceID LGE_Audio_LoadSFX(const char* lsfx_path);
AudioSourceID LGE_Audio_LoadStream(const char* lstream_path);
void          LGE_Audio_Play(AudioSourceID id, bool loop);
void          LGE_Audio_Stop(AudioSourceID id);
void          LGE_Audio_SetVolume(AudioSourceID id, float volume);
void          LGE_Audio_SetPosition(AudioSourceID id, Vec3 world_pos);
void          LGE_Audio_SetListenerTransform(Vec3 pos, Vec3 forward);
```

**Notes:**
- `.lsfx` files (short SFX, ADPCM format) are fully loaded into RAM at startup for zero-latency playback.
- `.lstream` files (long music/ambience, Ogg Vorbis) are decoded progressively from VFS -- never fully loaded into RAM.
- `LGE_Audio_Play` posts a command to a lock-free queue; the mixing thread consumes it asynchronously. The game thread never waits on audio.

---

### `lge_network.h`

**Purpose:** Core TCP/UDP network provider interface. Defines the functions for connecting, listening, sending, and polling network events behind the swappable `INetworkProvider` abstraction.

**Key declarations:**
```c
typedef void* NetworkHandle;

NetworkHandle LGE_Network_Connect(const char* host, uint16_t port);
NetworkHandle LGE_Network_Listen(uint16_t port);
void          LGE_Network_Send(NetworkHandle handle, const void* data, size_t len);
void          LGE_Network_Poll(
    NetworkHandle handle, void (*on_event)(const void* data, size_t len));
void          LGE_Network_Close(NetworkHandle handle);
```

**Notes:**
- `INetworkProvider` is the swappable interface -- the built-in implementation provides TCP/UDP. Extensions can register alternatives (Steam Networking, WebSockets) at load time without touching this header.
- Extensions wishing to use networking declare `network.client` or `network.server` in their `extension.json` manifest; the kernel checks permissions before honoring `LGE_Network_Listen` calls.

---

### `lge_vfs.h`

**Purpose:** Encrypted Virtual File System API. A single unified file-access surface that works identically in development (loose files on disk) and production (AES-256 encrypted `.pck` bundles).

**Key declarations:**
```c
typedef void* VFSFile;

void    LGE_VFS_Mount(const char* bundle_path_or_dir);
VFSFile LGE_VFS_Open(const char* virtual_path);
size_t  LGE_VFS_Read(VFSFile file, void* out_buf, size_t bytes);
size_t  LGE_VFS_Size(VFSFile file);
void    LGE_VFS_Close(VFSFile file);
bool    LGE_VFS_Exists(const char* virtual_path);
bool    LGE_VFS_ListDir(
    const char* virtual_dir, void (*on_entry)(const char* name, bool is_dir));
```

**Notes:**
- In production, `LGE_VFS_Mount` decrypts the `.pck` header table into a RAM index. Files are decrypted on read into the caller's buffer -- never written to disk in decrypted form.
- In development, `LGE_VFS_Mount` points at a loose directory; `VFS_Open` is a plain file open, no decryption. Same API, different backend.
- The AES-256 decryption key is derived at runtime by the security layer (`key_derivation.cpp`) from the publisher key + machine UUID -- never stored raw.
- `LGE_VFS_ListDir` is called by the editor's Asset Browser panel.

---

### `lge_security.h`

**Purpose:** License verification, boot integrity check, and VFS key derivation interface. Implements a swappable `ILicenseVerifier` so the offline verification path ships first and an online path can be dropped in later without changing any other code.

**Key declarations:**
```c
struct ILicenseVerifier {
    bool (*Verify)(const char* license_path);
};

void LGE_Security_SetVerifier(const ILicenseVerifier* verifier);
bool LGE_Security_VerifyBoot();

void LGE_Security_DeriveVFSKey(
    const uint8_t* publisher_key, size_t key_len,
    const char*    machine_uuid,
    uint8_t*       out_aes_key     // 32-byte output buffer
);
```

**Notes:**
- `LGE_Security_VerifyBoot()` computes a SHA-256 hash of `luc_kernel.dll` at boot and compares it against a baked-in expected value -- detecting binary tampering before any game logic starts.
- The offline `ILicenseVerifier` checks an Ed25519-signed `.lic` file against a public key baked into the kernel binary.
- `LGE_Security_DeriveVFSKey` implements PBKDF2/HKDF -- a `.lic` file from machine A will fail to derive the correct AES key on machine B because the machine UUID differs, making assets machine-bound.

---

### `lge_api.h`

**Purpose:** The master public header. Defines `LGE_ExtensionAPI` -- the single flat C function table struct that aggregates every subsystem -- and `LGE_GetAPI()`, the one stable symbol the kernel exports. This is the *only* header extensions are ever allowed to include.

**Key declarations:**
```c
struct LGE_ExtensionAPI {
    uint32_t version;

    // ECS
    EntityID (*create_entity)();
    void*    (*add_component)(EntityID, uint32_t type);
    void*    (*get_component)(EntityID, uint32_t type);
    void     (*destroy_entity)(EntityID);

    // Render
    TextureID (*upload_texture)(const void* pixels, uint32_t w, uint32_t h, PixelFormat fmt);
    void      (*draw_mesh)(BufferID mesh, TextureID tex, const Mat4* transform);

    // Physics, Input, Audio, VFS, Console, Network, Security ...
    // (one function pointer per subsystem call -- mirrors all other lge_*.h declarations)
};

// The one stable symbol the kernel exports from luc_kernel.dll
LGE_ExtensionAPI* LGE_GetAPI(uint32_t requested_version);
```

**Notes:**
- `lge_api.h` is the **last header to be finalized** -- it aggregates all other subsystem function pointers, so all other `lge_*.h` headers must be stable before `lge_api.h` is locked.
- If the kernel's major version is higher than the extension's `requested_version`, `LGE_GetAPI` returns `nullptr` -- the extension fails gracefully, not with a crash.
- This is the same versioned function-table model used by Vulkan.
- Lucid bindings (`bindings/*.luc`) import raw FFI declarations from `kernel/ffi/lge_ffi.lfi`, which is auto-generated from these headers by `tools/lge_header_parser/`.

---

## Section 2 -- `kernel/src/`

> **Implementation order matters.** The subsections below are ordered by dependency -- earlier subsystems must be functional before later ones can be written or tested. **2.1** is implemented before **2.2**, **2.2** before **2.3**, and so on.

---

### 2.1 -- `kernel/src/platform/`

**Implements:** `lge_platform.h` (internal only)  
**Depends on:** Nothing -- this is the absolute foundation layer.

This is the first subsystem implemented because every other system depends on it. It is the **only** place in the entire codebase where `#ifdef _WIN32`, `LoadLibrary`, `dlopen`, `CreateThread`, and native window handles are permitted to appear.

#### `win32_loader.cpp`

**Purpose:** Windows-specific implementation of the platform abstraction.

**Key responsibilities:**
- `Platform_LoadModule` -> `LoadLibrary`
- `Platform_GetSymbol` -> `GetProcAddress`
- `Platform_UnloadModule` -> `FreeLibrary`
- `Platform_SpawnThread` -> `CreateThread` (or thin `std::thread` wrapper)
- `Platform_GetNativeWindowHandle` -> returns the `HWND` from GLFW's native accessor (`glfwGetWin32Window`)
- Provides `ReadDirectoryChangesW` watcher implementation used by `core/file_watcher.cpp`

**Notes:**
- GLFW handles Win32 window creation; this file does not duplicate that. It wraps what GLFW does *not* abstract -- module loading and raw thread handles.
- Only one of `win32_loader.cpp` or `linux_loader.cpp` is compiled per build target; CMake selects based on `CMAKE_SYSTEM_NAME`.

#### `linux_loader.cpp`

**Purpose:** Linux-specific mirror of `win32_loader.cpp`.

**Key responsibilities:**
- `Platform_LoadModule` -> `dlopen`
- `Platform_GetSymbol` -> `dlsym`
- `Platform_UnloadModule` -> `dlclose`
- `Platform_SpawnThread` -> `pthread_create` or `std::thread`
- `Platform_GetNativeWindowHandle` -> `glfwGetX11Window`
- Provides `inotify` watcher implementation used by `core/file_watcher.cpp`

---

### 2.2 -- `kernel/src/core/` (Part 1 -- Infrastructure Utilities)

**Implements:** Clock, console dispatch, SHA-256 checksum, file watcher  
**Depends on:** `platform/`

`core/` is split across two implementation passes. This first pass covers the infrastructure utilities that other subsystems depend on. The boot orchestrator (`kernel.cpp`) and script manager come last in **2.12**, once all other subsystems are ready.

#### `clock.cpp`

**Purpose:** Subsystem delta-time frame clocks. Provides a high-resolution timer abstraction used by the frame loop, physics, and audio to compute per-frame `dt`.

**Key responsibilities:**
- Query OS high-resolution clock (`QueryPerformanceCounter` on Windows, `clock_gettime` on Linux, both via the platform layer).
- Expose `Clock::GetDeltaTime()` returning elapsed seconds as a `double`.
- Support per-subsystem clock instances so physics and audio can maintain independent `dt` accumulators.

**Notes:**
- No heap allocations. Clock state is a small POD struct held statically or on the caller's stack.
- Must be ready before `kernel.cpp` (used in the frame loop) and before `physics_bridge.cpp` (which passes `dt` to Jolt's step function).

#### `console_interpreter.cpp`

**Purpose:** Console command registration and dispatch. Implements the flat hash map of `name -> callback` that backs `LGE_Console_Register` and `LGE_Console_Exec`.

**Key responsibilities:**
- Maintain a static hash map: `const char* name -> void(*)(const char* args)`.
- `LGE_Console_Register` inserts a named command into the map.
- `LGE_Console_Exec` tokenizes the input string (command name + raw args substring), looks up the name, and calls the function pointer directly -- no eval, no scripting overhead.
- `LGE_Console_StartIPCServer` / `LGE_Console_ConnectIPC` -- starts a named-pipe or socket server/client (on a background platform thread) so the editor can send commands into a separately running game process.

#### `checksum.cpp`

**Purpose:** SHA-256 self-integrity check of `luc_kernel.dll`. Called by `LGE_Security_VerifyBoot()` before any game logic starts.

**Key responsibilities:**
- Read the `luc_kernel.dll` binary from disk via raw OS file I/O (not through VFS -- VFS is not mounted yet at this boot stage).
- Compute SHA-256 over its bytes.
- Compare against a compile-time baked-in expected hash constant.
- Return `true` if they match, `false` (triggering boot abort) if they differ.

**Notes:**
- SHA-256 implementation: use a small embedded single-header library or the platform's CNG/`libcrypto` API. No OpenSSL dependency.

#### `file_watcher.cpp`

**Purpose:** Background directory watcher that fires events when `.luc` source files change on disk, triggering hot-reload in the Script Manager.

**Key responsibilities:**
- `FileWatcher::Watch(dir, callback)` -- starts a background platform thread monitoring a directory using `ReadDirectoryChangesW` (Win32) or `inotify` (Linux), provided by the platform layer.
- On a `.luc` file change event, invoke the registered callback with the changed file path.
- `FileWatcher::Stop()` -- signals the background thread to exit and joins it.

**Notes:**
- The watcher runs on a platform thread -- it never stalls the frame loop while waiting for OS events.
- Only fires for `.luc` file saves -- other file extensions are filtered out to reduce noise.

---

### 2.3 -- `kernel/src/security/`

**Implements:** `lge_security.h`  
**Depends on:** `platform/`, `core/` (checksum)

Security must be in place before VFS (which uses key derivation to unlock `.pck` bundles) and before any game logic loads (license must be verified first).

#### `key_derivation.cpp`

**Purpose:** Implements `LGE_Security_DeriveVFSKey` -- the PBKDF2/HKDF pipeline that derives the AES-256 decryption key used by the VFS to unlock `.pck` asset bundles.

**Key responsibilities:**
- Accept the publisher's master key bytes and the machine's UUID string.
- Apply PBKDF2 (password-based key stretching) then HKDF (context mixing) to produce a 32-byte AES-256 key.
- Write the result into the caller-provided output buffer. The key is never stored between calls.

**Notes:**
- A valid `.lic` file on machine A will produce a different AES key on machine B (different UUID) -- making derived keys machine-bound by design.
- Use `BCryptDeriveKeyPBKDF2` (Windows CNG) or `PKCS5_PBKDF2_HMAC` (libcrypto) or an embedded implementation -- no full OpenSSL dependency.

#### `license_verifier.cpp`

**Purpose:** Implements the offline `ILicenseVerifier` -- verifies an Ed25519-signed `.lic` file against the public key baked into `luc_kernel.dll`.

**Key responsibilities:**
- Read the `.lic` file from a known OS path (not through VFS -- VFS is not mounted yet at this boot stage).
- Parse the file: extract the signed payload and the 64-byte Ed25519 signature.
- Verify the signature against the baked-in compile-time public key constant.
- Return `true` on valid, `false` on invalid, missing, or expired.

**Notes:**
- The public key is a compile-time constant embedded in the binary -- not in a config file, not in the VFS.
- A future `OnlineLicenseVerifier` can be swapped in by changing one `ILicenseVerifier*` assignment in `kernel.cpp` -- no other code changes required.

#### `extension_verifier.cpp`

**Purpose:** Ed25519 signature verification for extension packages before they are allowed to load.

**Key responsibilities:**
- `LGE_Security_VerifyExtension(path)` -- reads the extension's `extension.json` manifest, extracts the embedded signature, and verifies it against the engine's extension-signing public key.
- Return `true` on valid, `false` to block loading.

**Notes:**
- Unsigned or tampered extensions are rejected before `Platform_LoadModule` is called -- they never enter the process address space.
- The extension signing key is separate from the license key so a compromise of one does not compromise the other.

---

### 2.4 -- `kernel/src/vfs/`

**Implements:** `lge_vfs.h`  
**Depends on:** `platform/`, `security/` (AES key derivation from `key_derivation.cpp`)

VFS must be mounted before the Script Manager loads any `.lmod` files. Key derivation from `security/` must be functional before mount can decrypt the bundle header.

#### `vfs_reader.cpp`

**Purpose:** Memory-resident AES-256 decrypter and virtual file system reader. Implements the full `lge_vfs.h` API surface at runtime.

**Key responsibilities:**
- **Development mode:** `LGE_VFS_Mount(dir)` sets a base directory root. All subsequent `LGE_VFS_Open` calls resolve to `dir/virtual_path` via normal OS file I/O -- no decryption.
- **Production mode:** `LGE_VFS_Mount(bundle.pck)` calls `LGE_Security_DeriveVFSKey` to obtain the AES-256 key, then decrypts the `.pck` header table (file index: name hashes, offsets, sizes, per-file IV values) into a RAM lookup structure. `LGE_VFS_Open` seeks into the packed file; `LGE_VFS_Read` decrypts the requested chunk in place into the caller-provided buffer. Files are never written to disk in decrypted form.
- `LGE_VFS_ListDir` iterates the in-RAM directory index and invokes the callback per entry.

**Notes:**
- Same code path handles both modes -- the only difference is how `LGE_VFS_Mount` sets up its internal state struct.
- All decryption happens into caller-provided buffers. No internal heap allocation for file content.

#### `vfs_packer.cpp`

**Purpose:** The offline build-time CLI tool that creates `.pck` encrypted asset bundles from a directory of cooked assets.

**Key responsibilities:**
- Walk a source directory recursively, collect all asset files.
- Write a `.pck` header containing file count, per-file name hashes, byte offsets, sizes, and per-file AES-GCM IV values.
- AES-256-GCM encrypt each file's contents and append it to the bundle.
- Finalize with authenticated checksums covering the entire header.

**Notes:**
- `vfs_packer.cpp` is a CLI tool, not a runtime subsystem. It is compiled as a separate executable and invoked during the asset pipeline (e.g. called by `tools/package_sdk.py`).
- Shares the AES key derivation logic from `security/key_derivation.cpp`.

---

### 2.5 -- `kernel/src/ecs/`

**Implements:** `lge_ecs.h`  
**Depends on:** `platform/` (memory/thread primitives), `core/` (console for debug commands)

ECS is the data backbone of every runtime system. Physics, render, audio, and input all read or write ECS components, so this must be functional before any of those subsystems are implemented or tested.

#### `world.cpp`

**Purpose:** ECS entity lifecycle management. Implements `LGE_CreateEntity`, `LGE_DestroyEntity`, `LGE_ForEach`, and the global `EntityID -> ArchetypeSlot` lookup table.

**Key responsibilities:**
- Maintain a generational index table: `EntityID = (generation << 32) | slot_index`. Reusing a slot increments the generation, automatically invalidating stale `EntityID` handles held by old references.
- `LGE_CreateEntity` -- allocates the next free slot, returns a fresh `EntityID`.
- `LGE_DestroyEntity` -- swaps the entity's components out of their archetype (swap-with-last to avoid gaps), increments the slot's generation, returns the slot to the free list.
- `LGE_ForEach` -- iterates all archetype buckets matching the requested component type set and invokes the callback with flat array slices -- no pointer indirection per entity.

**Notes:**
- `LGE_ForEach` is the hot path -- it must iterate N entities over a flat array with no cache misses. Entity destruction during `LGE_ForEach` is deferred to end-of-iteration to avoid invalidating the array being walked.

#### `component_store.cpp`

**Purpose:** Archetype sequential array storage. Implements `LGE_RegisterComponent`, `LGE_AddComponent`, `LGE_GetComponent`, and `LGE_RemoveComponent`.

**Key responsibilities:**
- **Archetype table:** Maps a sorted component-type bitmask to an `Archetype` -- a collection of parallel SoA (Structure of Arrays) buffers, one per component type, storing component data for all entities in that archetype contiguously.
- `LGE_RegisterComponent(name, size, align)` -- allocates a new component type ID and records its size and alignment for archetype buffer sizing.
- `LGE_AddComponent(entity, type)` -- migrates the entity from its current archetype to the archetype with one additional type: allocates a slot in the target archetype, copies existing components via `memcpy`, returns a pointer to the new component's uninitialised memory (caller fills it).
- `LGE_RemoveComponent` -- inverse migration back to the archetype with one fewer type.
- `LGE_GetComponent` -- O(1): `EntityID -> slot index -> direct offset into the SoA buffer`.

**Notes:**
- All archetype SoA buffers are pre-allocated in large chunks (no per-entity `malloc`). Buffer overflow doubles capacity -- no STL dependency.
- Components are `memcpy`-safe POD -- migration between archetypes is a plain `memcpy`, never a copy constructor call.

#### `system_scheduler.cpp`

**Purpose:** Multithreaded system execution planner. Determines which systems can run concurrently (those whose component read/write sets do not overlap) and dispatches them to a platform thread pool.

**Key responsibilities:**
- Accept a list of registered systems, each declaring its read and write component type sets.
- Build a static dependency graph: two systems conflict if one writes what the other reads or writes.
- Dispatch non-conflicting systems concurrently on platform threads each frame.
- Guarantee deterministic ordering within a dependency level, regardless of OS thread scheduling jitter.

**Notes:**
- No locks are held during system iteration -- the declared component sets statically prove non-conflict before dispatch.
- Example: `AudioSystem` (reads `AudioSourceComponent`, `TransformComponent`) and `RenderSystem` (reads `MeshComponent`, `TransformComponent`) can run in parallel if `RenderSystem` only reads `TransformComponent`. `PhysicsSystem` (writes `TransformComponent`) runs in a separate phase from both.

#### `components/transform.cpp`

**Purpose:** Registers `TransformComponent` as a core ECS component type.

**`TransformComponent` fields (POD):**
```cpp
struct TransformComponent {
    float position[3];   // world-space position (x, y, z)
    float rotation[4];   // quaternion (x, y, z, w)
    float scale[3];      // non-uniform scale
};
```

**Notes:**
- Written by `PhysicsSystem` (Jolt body positions), read by `RenderSystem` (draw call transform matrix) and `AudioSystem` (3D spatialization position). These systems never call each other -- they communicate only through this shared component.
- Registered with `LGE_RegisterComponent` during `ECSWorld::Init()` inside `kernel.cpp`.

#### `components/physics_body.cpp`

**Purpose:** Registers `PhysicsBodyComponent` -- stores the Jolt body handle and physics simulation state on an entity.

**`PhysicsBodyComponent` fields (POD):**
```cpp
struct PhysicsBodyComponent {
    uint32_t body_id;           // opaque Jolt BodyID handle (integer, not a C++ type)
    float    linear_vel[3];
    float    angular_vel[3];
    bool     is_static;
};
```

#### `components/sprite_renderer.cpp`

**Purpose:** Registers `SpriteRendererComponent` -- describes a 2D sprite or billboard mesh to be submitted by `RenderSystem`.

**`SpriteRendererComponent` fields (POD):**
```cpp
struct SpriteRendererComponent {
    uint32_t texture_id;    // TextureID from LGE_UploadTexture
    float    color[4];      // RGBA tint multiplier
    float    uv_offset[2];  // UV animation scroll offset
    float    uv_scale[2];
};
```

#### `components/audio_source.cpp`

**Purpose:** Registers `AudioSourceComponent` -- describes an audio clip attached to an entity, including playback state and 3D spatialization parameters.

**`AudioSourceComponent` fields (POD):**
```cpp
struct AudioSourceComponent {
    uint32_t source_id;   // AudioSourceID from LGE_Audio_LoadSFX / LoadStream
    float    volume;      // 0.0 - 1.0
    bool     loop;
    bool     playing;
    bool     is_3d;
};
```

**Notes:**
- `AudioSystem` reads `AudioSourceComponent` + `TransformComponent` each frame to update 3D spatialization parameters via `LGE_Audio_SetPosition`.

---

### 2.6 -- `kernel/src/render/`

**Implements:** `lge_render.h`  
**Depends on:** `platform/` (window handle for Vulkan surface creation), `ecs/` (reads `TransformComponent`, `SpriteRendererComponent`), `vfs/` (atlas texture loading for SDF fonts)

The render subsystem requires a platform window before it can create a Vulkan surface, and it reads ECS component data per frame during draw submission.

#### `vulkan_rhi.cpp`

**Purpose:** The full Vulkan RHI -- device setup, swapchain, command pools, VMA memory allocator, texture/buffer management, PSO cache, and frame submission pipeline.

**Key responsibilities:**
- **Initialization:** Create `VkInstance`, select physical GPU, create `VkDevice`, create `VkSwapchainKHR` using the platform-provided native window handle, initialize VMA (`VmaAllocator`).
- **Bindless descriptor heap:** Allocate a global `VkDescriptorSet` with a large `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` array. `LGE_UploadTexture` writes the new texture's `VkImageView` into the next free slot and returns its array index as a `TextureID`.
- **PSO cache:** On the first `LGE_DrawMesh` call for a given shader + vertex-format + blend-mode combination, compile a `VkPipeline` and insert it into the cache. Subsequent frames reuse the cached PSO -- no mid-game shader compilation stutter.
- **Frame lifecycle:**
  - `LGE_RHI_BeginFrame` -- `vkAcquireNextImageKHR`, reset the frame's command buffer pool.
  - `LGE_RHI_EndFrame` -- `vkEndCommandBuffer`, `vkQueueSubmit`.
  - `LGE_RHI_Present` -- `vkQueuePresentKHR`, handle `VK_SUBOPTIMAL_KHR` and window resize events.
- **Resource management:** `LGE_UploadMesh` creates a VMA-backed vertex + index buffer pair. `LGE_DrawMesh` records `vkCmdDrawIndexed` into the active command buffer.

**Notes:**
- All Vulkan handle types (`VkDevice`, `VkCommandBuffer`, etc.) are private to this file -- they never appear in `lge_render.h`.
- Double or triple buffering is managed internally; the frame loop in `kernel.cpp` is not aware of how many frames are in flight.
- ImGui and RmlUI share the same `VkDevice` and submit into the same command buffer via their own backends initialized against this RHI.

#### `sdf_font.cpp`

**Purpose:** Multi-channel SDF (Signed Distance Field) font rendering for the in-game HUD, debug overlays, and console text.

**Key responsibilities:**
- Load a pre-generated MSDF font atlas from VFS.
- Upload the atlas via `LGE_UploadTexture`.
- For a given string + font size, compute glyph quad geometry and UV coordinates from the atlas glyph table.
- Submit the quads as a `LGE_DrawMesh` call using an SDF alpha-test pipeline PSO that uses the signed distance channel for crisp edge rendering at any scale.

**Notes:**
- SDF rendering produces crisp text at any scale (8px to 200px+) from a single atlas texture -- no need to pre-rasterize multiple font sizes.
- Depends on `vfs/` (to load the atlas file) and `vulkan_rhi.cpp` (for `LGE_UploadTexture` and `LGE_DrawMesh`).

---

### 2.7 -- `kernel/src/input/`

**Implements:** `lge_input.h`  
**Depends on:** `platform/` (GLFW window handle for callback registration)

Input is fairly standalone -- it only needs the platform window handle to register GLFW callbacks. It has no dependency on ECS or rendering.

#### `input_bridge.cpp`

**Purpose:** Maps GLFW keyboard, mouse, and gamepad OS events to a frame-synchronized input state buffer exposed via `lge_input.h`.

**Key responsibilities:**
- `InputBridge::Init(GLFWwindow* win)` -- register GLFW callbacks: `glfwSetKeyCallback`, `glfwSetCursorPosCallback`, `glfwSetMouseButtonCallback`, `glfwSetScrollCallback`.
- Each callback writes into a double-buffered state struct (current frame state + previous frame state).
- At the start of each frame (called by `kernel.cpp`): swap buffers -- previous <- current -- and clear one-shot flags (`IsKeyPressed`, `IsKeyReleased`) from the current buffer.
- `LGE_Input_IsKeyHeld` -> current frame's key bitmask.
- `LGE_Input_IsKeyPressed` -> `current & ~previous` for this key (newly down this frame only).
- `LGE_Input_MouseDelta` -> current mouse position minus previous frame's position.

**Notes:**
- Raw GLFW callbacks fire on the OS message thread. The double-buffered swap in the frame loop decouples them from ECS/game threads -- no mutex needed in the polling hot path.
- The `IInputProvider` interface sits between `input_bridge.cpp` and the GLFW calls, making it possible to swap the entire backend (Android NDK, console SDK) by implementing a new provider -- Lucid scripts require no changes.

---

### 2.8 -- `kernel/src/physics/`

**Implements:** `lge_physics.h`  
**Depends on:** `ecs/` (reads/writes `TransformComponent`, `PhysicsBodyComponent`), `render/` (debug wireframe renderer uses `LGE_DrawMesh`), `core/` (clock for `dt`, console for debug toggle command)

Physics depends on ECS to synchronize simulation results with game state, and on the render system to draw debug wireframes.

#### `physics_bridge.cpp`

**Purpose:** Jolt Physics integration. Initializes the Jolt `PhysicsSystem`, steps the simulation each frame, and synchronizes results back into ECS components.

**Key responsibilities:**
- `PhysicsBridge::Init()` -- create a Jolt `PhysicsSystem` with a broad-phase layer configuration, a body interface, and a contact listener (`LucContactListener`).
- `PhysicsBridge::Update(float dt)` -- step the Jolt simulation by `dt` seconds.
- After each step: iterate all active Jolt bodies, fetch updated positions/rotations from Jolt, write them into `TransformComponent` via `LGE_GetComponent` using the stored `EntityID` mapping.
- `LGE_Physics_CreateBody` -- create a `JPH::BodyCreationSettings` from the POD `PhysicsBodyDef`, add it to the physics world, store the returned opaque `BodyID` integer in `PhysicsBodyComponent`.
- `LGE_Physics_Raycast` -- call Jolt's `NarrowPhaseQuery::CastRay`, translate the result to the POD `RaycastHit` struct.
- `LGE_Physics_SetContactCallback` -- store the user's callback; `LucContactListener::OnContactAdded` invokes it on collision events.

**Notes:**
- All Jolt C++ types (`JPH::BodyID`, `JPH::PhysicsSystem`) are private to this file -- none appear in `lge_physics.h`.
- `PhysicsBridge::Update()` runs before `RenderSystem` each frame so transforms are current before draw calls are recorded. Jolt and Vulkan never communicate directly.
- Arena memory from Lucid (`LGE_ArenaDescriptor`) may be accepted for bulk body data loads -- validated with `LGE_Arena_Contains` in debug builds.

#### `debug_renderer.cpp`

**Purpose:** Jolt debug wireframe visualization. Implements Jolt's `DebugRenderer` interface to render collision shape outlines, contact normals, and bounding boxes during development.

**Key responsibilities:**
- Inherit from `JPH::DebugRenderer` and implement `DrawLine`, `DrawTriangle`, `DrawText3D`.
- Each draw call converts Jolt world-space geometry into a `LGE_DrawMesh` call using a wireframe PSO.
- Toggled by a console command (`physics debug on/off`) registered via `LGE_Console_Register`.

**Notes:**
- Only compiled/linked in debug and development builds -- stripped entirely from shipping builds via a preprocessor guard.
- Depends on `render/vulkan_rhi.cpp` for `LGE_DrawMesh` and `core/console_interpreter.cpp` for the debug toggle command registration.

---

### 2.9 -- `kernel/src/audio/`

**Implements:** `lge_audio.h`  
**Depends on:** `ecs/` (reads `AudioSourceComponent` + `TransformComponent`), `vfs/` (loads `.lsfx` / `.lstream` files from the bundle), `platform/` (thread for the mixing loop)

Audio depends on ECS for spatialization data and on VFS for loading audio asset files.

#### `audio_system.cpp`

**Purpose:** Miniaudio-backed SFX and streaming audio mixer running on a dedicated mixing thread.

**Key responsibilities:**
- `AudioSystem::Init()` -- open the OS audio device via `ma_device_init`, start the background mixing thread via the platform layer.
- **SFX path:** `LGE_Audio_LoadSFX` reads a `.lsfx` (ADPCM) file from VFS into a RAM buffer -- fully decoded at load time for zero-latency playback.
- **Stream path:** `LGE_Audio_LoadStream` opens a `.lstream` (Ogg Vorbis) VFS file handle. The mixing thread decodes the next chunk each audio callback -- the file is never fully loaded into RAM.
- `LGE_Audio_Play` -- posts a `Play` command to a lock-free MPSC ring buffer. The main thread never waits on the audio thread.
- **3D spatialization:** `LGE_Audio_SetPosition` recomputes panning and attenuation based on the source world position relative to the listener's `pos` and `forward` vectors. Applied by the mixing thread before writing to the output buffer.
- **Per-frame ECS sync:** `AudioSystem::UpdateSpatial()` (called by `kernel.cpp` frame loop) reads `AudioSourceComponent` + `TransformComponent` for each audio entity and calls `LGE_Audio_SetPosition` to keep 3D parameters current.

**Notes:**
- The miniaudio mixing callback fires on a strict OS audio timer thread. Never allocate memory or acquire locks inside it.
- All play/stop/volume/position commands travel via the lock-free MPSC queue -- the game thread produces, the audio thread consumes.

---

### 2.10 -- `kernel/src/ui/`

**Implements:** Internal only (no public `lge_*.h` surface -- called only from `kernel.cpp` and the editor layer)  
**Depends on:** `render/` (both backends require the initialized Vulkan device and swapchain), `vfs/` (RmlUI loads `.rml`/`.rcss` files from VFS), `ecs/` (UISystem reads entity UI components each frame)

Both UI backends require the Vulkan RHI to be up before they can initialize.

#### `imgui_shell.cpp`

**Purpose:** ImGui frame lifecycle management for the editor shell. Bridges ImGui's immediate-mode draw calls into the RHI's command buffer.

**Key responsibilities:**
- `ImGuiShell::Init(VkSwapchain* swapchain)` -- initialize ImGui with the Vulkan backend (`imgui_impl_vulkan`), upload the font atlas, and set up GLFW input forwarding (`imgui_impl_glfw`).
- `ImGuiShell::BeginFrame()` -- called each frame after `LGE_RHI_BeginFrame`; calls `ImGui::NewFrame()`.
- `ImGuiShell::EndFrame()` -- calls `ImGui::Render()`, then `ImGui_ImplVulkan_RenderDrawData()` to flush the ImGui vertex/index batch into the active RHI command buffer.
- Editor panel C++ code (inspector, hierarchy, console, asset browser) runs between `BeginFrame` and `EndFrame`, accumulating ImGui draw calls.

**Notes:**
- ImGui is initialized against the same `VkDevice` and `VkRenderPass` as the main RHI -- it shares the swapchain rather than owning a separate window.
- ImGui runs only when the editor is active; shipped games use only RmlUI.

#### `rmlui_backend.cpp`

**Purpose:** RmlUI Vulkan rendering backend for in-game HUD and menus.

**Key responsibilities:**
- Initialize RmlUI's `SystemInterface` and `RenderInterface` with a Vulkan backend that submits geometry into the RHI command buffer.
- `LGE_UI_CreateContext(width, height)` -- create a new RmlUI context (one per entity with a `UIComponent`).
- `LGE_UI_LoadDocument(ctx, rml_path)` -- load an `.rml` markup file from VFS into the context's DOM.
- `LGE_UI_SetProperty(ctx, element_id, property, value)` -- reactively update an element's CSS property, used by Lucid UI bindings to drive animations and state changes without rebuilding the DOM.
- `LGE_UI_UpdateAndRender(ctx)` -- call `Rml::Context::Update()` then `Rml::Context::Render()`, submitting vertex batches via the `RenderInterface` into the active RHI command buffer. Called by UISystem per frame for each entity with a `UIComponent`.

**Notes:**
- RmlUI and ImGui both emit draw calls into the same Vulkan command buffer but at distinct render passes -- they never composite over each other in the same screen region.

---

### 2.11 -- `kernel/src/network/`

**Implements:** `lge_network.h`  
**Depends on:** `platform/` (socket abstraction and thread primitives for the poll loop)

Network is the most standalone subsystem -- it only needs platform thread primitives and sockets. It has no ECS or render dependencies.

#### `network_manager.cpp`

**Purpose:** Core TCP/UDP network provider. Implements `INetworkProvider` with platform Berkeley sockets and manages connection lifecycle.

**Key responsibilities:**
- `LGE_Network_Connect(host, port)` -- open a TCP socket, connect, return a `NetworkHandle`.
- `LGE_Network_Listen(port)` -- bind a socket, accept incoming connections on a background accept thread.
- `LGE_Network_Send(handle, data, len)` -- write bytes to the socket (non-blocking, with a send queue if the socket buffer is full).
- `LGE_Network_Poll(handle, on_event)` -- drain the receive queue and invoke `on_event` for each received message.
- `LGE_Network_Close(handle)` -- close the socket, clean up the handle entry.

**Notes:**
- `INetworkProvider` is swappable -- Steam Networking, WebSockets, or other transports can register as extensions without modifying this file.
- Extension permissions (`network.client`, `network.server`) are checked by the kernel before `LGE_Network_Listen` is honored.
- Uses `Winsock2` (Windows) or POSIX sockets (Linux) behind the platform abstraction -- no direct `#ifdef _WIN32` in this file.

---

### 2.12 -- `kernel/src/core/` (Part 2 -- Boot Orchestrator)

**Implements:** `LGE_Boot`, `LGE_GetAPI`, main frame loop, and hot-reload script manager  
**Depends on:** All preceding subsystems (2.1 through 2.11) -- this is the final integration step.

`kernel.cpp` and `script_manager.cpp` are implemented last because they are the files that call every other subsystem's `::Init()` function and wire them together into a running engine. None of their logic can be tested in isolation until all dependencies are functional.

#### `kernel.cpp`

**Purpose:** The microkernel boot entry point. Implements `LGE_Boot()` -- the ordered initialization sequence -- and the main frame loop.

**Key responsibilities:**
- `LGE_Boot(const LGE_BootConfig& cfg)` -- initialize subsystems in strict dependency order:
  1. `Platform::Init` -- window creation, OS handles
  2. `RHI::Init` -- Vulkan device, swapchain, VMA
  3. `ImGuiShell::Init` -- attaches ImGui to the RHI swapchain
  4. `VFS::Mount` -- mounts asset bundle or dev directory
  5. `ECSWorld::Init` -- archetype storage, registers core component types
  6. `PhysicsBridge::Init` -- Jolt physics world
  7. `AudioSystem::Init` -- mixing thread, OS audio device
  8. `InputBridge::Init` -- GLFW callbacks registered
  9. `Security::VerifyBoot` -- SHA-256 self-check + license validation
  10. `ScriptManager::LoadEntry` -- loads editor or game `.lmod` entry point
  11. `RunFrameLoop()` -- blocks until window close or `LGE_Shutdown()`
- **Frame loop** (inside `RunFrameLoop`) -- per frame, in order:
  1. `InputBridge::SwapBuffers()` -- snapshot input state for this frame
  2. `PhysicsBridge::Update(dt)` -- step physics simulation, write `TransformComponent`
  3. `AudioSystem::UpdateSpatial()` -- read `AudioSourceComponent` + `TransformComponent`, update 3D positions
  4. `LGE_RHI_BeginFrame()` -- acquire swapchain image, reset command buffer
  5. `ImGuiShell::BeginFrame()` -- open ImGui frame (editor mode only)
  6. `ScriptManager::DispatchUpdate(dt)` -- call game/editor scripts' `on_update` hook
  7. `ImGuiShell::EndFrame()` -- flush ImGui draw calls to command buffer
  8. `LGE_RHI_EndFrame()` -- submit command buffer to GPU queue
  9. `LGE_RHI_Present()` -- present swapchain image, handle resize
- `LGE_GetAPI(uint32_t requested_version)` -- fill `LGE_ExtensionAPI` with all subsystem function pointers and return it. Return `nullptr` on major version mismatch.

**Notes:**
- `LGE_Boot` is the *only* public entry point the platform bootstrap (`LucidEditor.exe` / shipped game `.exe`) calls.
- The frame loop never returns until the window is closed or `LGE_Shutdown()` is called.
- `LGE_GetAPI` requires all preceding subsystem `.cpp` files to be compiled and linked -- this is why `kernel.cpp` is the final file written.

#### `script_manager.cpp`

**Purpose:** Lucid module hot-reload manager. Loads compiled `.lmod`/`.dll` files, invokes their lifecycle hooks, and swaps them atomically when source files change on disk.

**Key responsibilities:**
- `ScriptManager::LoadEntry(const char* module_path)` -- call `Platform_LoadModule`, resolve `on_load` / `on_update` / `on_unload` function pointers, call `on_load` once at startup.
- `ScriptManager::DispatchUpdate(float dt)` -- invoke the active module's `on_update(dt)` each frame (called by `kernel.cpp` frame loop).
- **Hot-reload path** (triggered by `FileWatcher` callback from `core/file_watcher.cpp`):
  1. Spawn `luc_compiler.exe` as a child process targeting the changed `.luc` file. Output goes to `module_next.dll` -- never overwrites the live file.
  2. On successful compile: wait for the current frame to finish, call old module's `on_unload`, call `Platform_UnloadModule`, atomically rename `module_next.dll` -> `module.dll`, call `Platform_LoadModule` on the new file, call its `on_load`.
  3. On compile failure: leave the live module untouched, pipe the compiler error output to the console via `LGE_Console_Exec`.
- Track all live modules in a `LiveModule` table:

```cpp
struct LiveModule {
    ModuleHandle handle;
    LGE_ModuleAPI api;           // on_load, on_update, on_unload function pointers
    char         source_path[512];
};
```

**Notes:**
- Because all game state lives in ECS components (not inside the script module's own memory), the newly loaded module picks up exactly where the old one left off -- there is no script state to migrate.
- The compiler subprocess writes to `_next.dll` only. The atomic rename happens only after a confirmed successful compile and after the current frame completes, ensuring the live module is never in a partial state.

---

*End of Kernel Implementation Plan.*
