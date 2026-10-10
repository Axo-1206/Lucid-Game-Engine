# Lucid Engine: File Format Reference

This document tracks all custom and standard file extensions used across the Lucid Engine ecosystem. Using explicit extensions allows the OS to assign proper icons and allows the engine to instantly filter assets without parsing them.

---

## 1. Source Formats (Editor & Development)
These files live in the `user_projects/MyGame/` folder. They are tracked by Git. They are highly readable text formats meant for developers.

| Extension | Internal Format | Description |
|:---|:---|:---|
| **`.luc`** | Text | The Luc compiled systems language source code. |
| **`.lucscene`** | JSON | An entire ECS Scene (level) holding entities, transforms, and components. |
| **`.prefab`** | JSON | A saved ECS Entity template (e.g., an Enemy) that can be spawned dynamically. |
| **`.lmat`** | JSON | A Material definition, linking a shader to texture UUIDs and setting color values. |
| **`extension.json`** | JSON | The manifest file for an engine extension. |
| **`engine_settings.json`** | JSON | The main configuration file for the engine IDE. |

---

## 2. Cooked Assets (Runtime Binary)
These files live inside the `build/` folder. They are **NOT** tracked by Git. They are generated automatically by `luc_asset_baker.exe` from source files (`.png`, `.gltf`). They are pure binary memory dumps designed for maximum loading speed.

| Extension | Internal Format | Source Origin | Description |
|:---|:---|:---|:---|
| **`.ltex`** | Binary | `.png`, `.tga` | Lucid Texture. A tiny 32-byte header followed by raw GPU-ready BCn compressed pixels. |
| **`.lmesh`** | Binary | `.gltf` | Lucid Mesh. An interleaved Vertex Buffer and Index Buffer ready for Vulkan `memcpy`. |
| **`.lvec`** | Binary | `.svg` | Lucid Vector. A tessellated 2D triangle mesh generated from complex math curves. |
| **`.lanim`** | Binary | `.gltf` | Lucid Animation. Extracted and resampled skeletal keyframe arrays (Quaternions/Vectors). |
| **`.spv`** | Binary | `.glsl` | Khronos SPIR-V compiled shader code required by Vulkan. |
| **`.opus`** | Binary | `.wav`, `.flac` | Extremely high-quality, heavily compressed streaming audio. |

---

## 3. Distribution & Security Formats
These are the files that end up in the hands of the final player or the extension consumer. They are packaged, signed, or encrypted.

| Extension | Internal Format | Description |
|:---|:---|:---|
| **`.lucext`** | ZIP + Signature | **Lucid Extension Package.** A zipped extension folder containing a cryptographic Ed25519 signature to prove the publisher's identity. |
| **`.lucid`** | AES-256 Encrypted | **Lucid VFS Bundle.** The final packed game data. It contains all the cooked assets (`.ltex`, `.lmesh`) mashed into one large file and encrypted to prevent asset theft. |
| **`license.lge`** | Signed JSON | An offline license ticket. Contains the player's hardware ID and a digital signature proving they bought the game. |
| **`.pem`** | AES-256 Encrypted | A developer's private or public cryptographic key (e.g., `publisher_private.pem`). Used to sign `.lucext` and `license.lge` files. |
