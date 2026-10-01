**Architecture & Scripting Layer**

* **Scripting Strategy:** Transitioning temporarily from `lucid-lang` to a high-level language layer while keeping the C++ microkernel pure is structurally sound.
* **Language Choice (AssemblyScript):** Selected **AssemblyScript** over standard TypeScript or Lua. It provides TypeScript-like syntax and strict static typing while compiling to WebAssembly (`.wasm`), avoiding heavy JavaScript engine bloat.
* **Execution Pipeline (JIT to AOT):**
* *Development Phase (JIT/Dynamic):* Run `.wasm` scripts dynamically using a lightweight embedded Wasm runtime (e.g., `wasm3` or `Wasmtime`).
* *Production Phase (AOT):* Convert `.wasm` bytecode to native C/C++ code via tools like `wasm2c` for zero-overhead ahead-of-time compilation.


* **Future `lucid-lang` Compatibility:** Because the kernel interacts strictly via a flat C ABI boundary (`LGE_GetAPI()`), swapping AssemblyScript for `lucid-lang` (targeting an LLVM backend) later will require zero architectural changes to the C++ core.



---

**UI Framework & Footprint Control**

* **Rejection of Electron:** Electron/Chromium is ruled out for engine tooling and game interfaces due to severe memory inflation (>1 GB) and multi-process CPU/GPU competition.
* **UI Tool Split:**
* **Dear ImGui:** Chosen for debug tools, inspectors, and editor layout panels (using `imgui_docking` and custom styling for a clean IDE appearance under ~20 MB RAM).
* **RmlUi:** Chosen for player-facing HUDs and menus requiring HTML/CSS layouts, running natively in C++ without browser engine overhead.


* **Target RAM Budget:** By avoiding Electron and managed JS runtimes in favor of lightweight Wasm and native UI, total engine editor memory usage will easily stay well under the **500 MB** target.