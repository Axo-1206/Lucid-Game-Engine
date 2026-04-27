# LucidEngine Security & IP Protection Plan (V2)

## 1. Developer Compilation Interface
The Luc compiler provides three distinct build profiles via command-line arguments. This allows developers to choose the balance between security, performance, and flexibility.

### A. Debug Mode (Default)
* **Command:** `luc_compiler main.luc`
* **Method:** Just-In-Time (JIT) Compilation.
* **Security:** Minimal. Includes full symbol tables and line mappings for error reporting.

### B. Standard Release (`--release`)
* **Command:** `luc_compiler main.luc --release`
* **Method:** Ahead-Of-Time (AOT) Compilation.
* **Goal:** Maximum Security & Performance.
* **Process:** * Converts Luc code directly to platform-specific machine code (binary).
    * **LLVM O3 Optimization:** Logic flow is scrambled and optimized for the hardware.
    * **Full Symbol Stripping:** Removes all function and variable names from the binary.

### C. Secure JIT Release (`--release-jit`)
* **Command:** `luc_compiler main.luc --release-jit`
* **Method:** Hardened JIT Compilation.
* **Goal:** Modding Support & Portability.
* **Process:**
    * **Bytecode Obfuscation:** The compiler mangles all internal identifiers (e.g., `UpdatePlayerHealth` becomes `_Z9uph01`).
    * **Encryption:** The resulting bytecode is wrapped in an AES-256 encrypted package.
    * **Portable Distribution:** Ship a single bytecode file that runs on any OS supported by the Lucid Kernel.

---

## 2. Content & Asset Protection (Encrypted VFS)
To prevent "asset theft" (stealing 3D models, textures, or scripts), LucidEngine uses a locked Virtual File System.

* **AES-256 Encryption:** All game files are bundled into a proprietary encrypted format (e.g., `.lucid` or `.pkg`).
* **The Key Handshake:** Decryption keys are never stored as plain text. The C++ kernel derives the key at runtime using:
    1.  A hidden seed embedded in the `luc_kernel.dll`.
    2.  The developer’s unique AppID.
* **RAM-Only Execution:** Files are decrypted directly into a memory buffer. No unencrypted data is ever written to the user's hard drive, preventing "disk sniffing."

---

## 3. Kernel-Level "Shield" Architecture
The C++ Kernel acts as the final gatekeeper for security.

### FFI Visibility Control
* **Explicit Exports:** The engine uses the `LUC_API` macro (white-listing). Only essential functions are exposed to the FFI. Internal security or engine logic remains hidden from symbol-peeking tools like *Dependency Walker*.
* **Anti-Tamper Validation:** Upon boot, the kernel calculates a SHA-256 checksum of the game package. If the files have been modified (to bypass licensing or inject cheats), the engine terminates.

### Extension Security
* **Signed Manifests:** To prevent malicious extensions from stealing source code during the JIT process, the kernel requires a cryptographic signature for every external module.

---

## 4. Summary of Protection Levels

| Feature | Debug | `--release` (AOT) | `--release-jit` (Secure JIT) |
| :--- | :--- | :--- | :--- |
| **Logic Visibility** | High (Source-like) | **None (Binary)** | Low (Obfuscated Bytecode) |
| **Reverse Engineering** | Easy | **Extremely Difficult** | Difficult |
| **Asset Security** | Unencrypted | **Encrypted** | **Encrypted** |
| **Modding Support** | High | Low | **High** |
| **Cross-Platform** | High | Build-specific | **High (One build for all)** |

***

### Implementation Note for the Compiler
When building the compiler in C++, ensure the `main` function uses the `LLVM Support/CommandLine.h` library to handle these flags, ensuring that `--release` and `--release-jit` are mutually exclusive to avoid build errors.