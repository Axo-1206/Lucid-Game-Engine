# Lucid Engine: Asset Pipeline Proposal

The goal of the Asset Pipeline is to never parse complex files (like FBX or PNG) during game runtime. Parsing is slow and uses massive amounts of RAM. 

Instead, we "cook" (or bake) source assets offline into **GPU-ready binary formats** that the engine can simply `memcpy` directly from the hard drive into memory.

Here is the proposed architecture for the Lucid Asset Pipeline:

---

## 1. Format Conversions

| Asset Type | Source Format (Editor) | Cooked Format (Runtime VFS) | Why this cooked format? |
|:---|:---|:---|:---|
| **Textures** | `.png`, `.tga`, `.exr` | **`.ktx2` (Block Compression)** | The GPU cannot read PNGs; it must decompress them to raw RGBA, inflating a 2MB PNG to 16MB of VRAM. `.ktx2` stores data in BCn formats (BC7, BC1, BC3). The GPU reads BCn natively. This drastically reduces load times and VRAM usage. |
| **Meshes** | `.fbx`, `.gltf`, `.obj` | **`.lmesh` (Custom Binary)** | FBX/GLTF require heavy parsing. A `.lmesh` file is simply a raw memory dump of exactly what Vulkan needs: an interleaved Vertex Buffer (Pos, Normal, UV) and an Index Buffer. The engine just copies it straight to the GPU. |
| **Audio (SFX)** | `.wav` | **`.adpcm` or `.ogg` (Vorbis)** | WAV is too large. ADPCM is very fast to decode for rapid sound effects (footsteps, guns). Vorbis is excellent for general audio. |
| **Audio (Music)** | `.wav`, `.flac` | **`.opus`** | Opus (MIT licensed) is the modern standard for streaming audio. Incredibly high quality at very low bitrates. |
| **Shaders** | `.glsl`, `.hlsl` | **`.spv` (SPIR-V)** | Vulkan requires SPIR-V. We compile shaders offline using `glslangValidator` or `dxc`. |
| **Vector UI** | `.svg` | **`.lvec` (Tessellated Geometry)** | GPUs cannot draw mathematical curves natively. The baker tessellates the SVG into a flat 2D mesh (triangles) and saves it as `.lvec`. At runtime, the UI system just renders colored triangles. |
| **Animations** | `.gltf`, `.fbx` | **`.lanim` (Keyframe Tracks)** | The baker extracts skeletal hierarchy and keyframe tracks (rotation, translation, scale). It resamples complex curves into optimized, linear arrays of quaternions and vectors for fast CPU blending. |

---

## 2. Deep Dive: Format Comparisons

### Meshes: Why GLTF over FBX and OBJ?

*   **FBX (The Bad):** FBX is a proprietary, closed-source format owned by Autodesk. It is notoriously difficult to parse, undocumented, and requires linking a massive, slow C++ SDK.
*   **OBJ (The Bad):** OBJ is an ancient text format. It is huge, slow to read, and does not support skeletons, animations, or modern material setups.
*   **GLTF / GLB (The Winner):** GLTF is the "JPEG of 3D." It is an open standard designed specifically for real-time rendering. The data is already laid out exactly how GPUs like it. You can parse it in seconds using tiny, open-source libraries like `cgltf`.

### Textures: `.ktx2` vs Custom `.ltex`

| Format | Pros | Cons |
|:---|:---|:---|
| **`.ktx2`** | It is the official Khronos standard. Supports all modern BCn and ASTC formats. | Requires integrating the `KTX-Software` C++ library into the engine, which is large and complex. |
| **Custom `.ltex`** | Extremely simple. Just a 32-byte header (width, height, mip levels) followed by raw BC7 data. Zero dependencies to load at runtime. | You have to write the baker logic yourself (using a library like DirectXTex or Compressonator offline). |

**Recommendation:** Go with **Custom `.ltex`**. For a custom engine, having a 50-line C++ loader is much better than pulling in a massive external dependency just to read texture headers. 

---

## 3. The Cooking Pipeline

The pipeline is handled by a separate CLI tool, `luc_asset_baker.exe`.

### The Flow:
1. **Asset Detection:** The editor notices a new `hero.gltf` in the `assets/` folder.
2. **Meta File Creation:** The editor generates `hero.gltf.meta`. This JSON file stores import settings (e.g., "Generate Normals: True", "Scale: 0.01").
3. **Cooking:** `luc_asset_baker.exe` reads the GLTF and the Meta file. It extracts the raw vertices, interleaves them, and saves `build/cooked/hero.lmesh`.
4. **VFS Packing:** `vfs_packer.cpp` takes `hero.lmesh`, encrypts it (AES), and stuffs it into the final `data_01.lucid` bundle.

### Asset Caching (Crucial for iteration speed)
Cooking takes time (especially compressing BC7 textures). The Baker hashes the source file and the `.meta` file. 
* If the hash matches the cache, it skips cooking and reuses the old `.lmesh`.
* If you change a texture, it only re-cooks that specific texture, not the whole project.

---

## 4. The `.lmesh` (Lucid Mesh) Format Spec

To give you an idea of how simple the cooked formats should be, here is what a `.lmesh` file looks like in binary:

```cpp
struct LMeshHeader {
    uint32_t magic = 0x4D43554C; // "LUCM"
    uint32_t version = 1;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t bounds_min[3];      // For frustum culling
    uint32_t bounds_max[3];
};

// File layout on disk:
// [ LMeshHeader ]
// [ Vertices array (Pos, Normal, UV) ]  <-- Exact size = vertex_count * sizeof(Vertex)
// [ Indices array (uint16 or uint32) ]  <-- Exact size = index_count * sizeof(Index)
```
**Runtime Load Code:**
```cpp
// 1. Read header
// 2. Allocate Vulkan buffers based on header sizes
// 3. memcpy the rest of the file directly into mapped GPU memory. Done.
```

---

## 5. Extension Asset Exporters (Asset Creation Extensions)

If a developer writes an extension to create assets inside the Lucid Editor (e.g., a pixel art drawing tool, or a voxel terrain generator), they need a way to save their work as standard source formats (`.png`, `.gltf`) so the Asset Baker can cook them.

Writing a GLTF or PNG exporter in pure Luc code would be highly complex and tedious. Instead, the Engine Kernel provides an **`IAssetExporter` interface**, exposed to Luc via the FFI.

The C++ backend uses tiny, fast, open-source libraries (`stb_image_write.h` for PNG, `cgltf_write` or `tinygltf` for GLTF) to do the heavy lifting.

**Example: A Pixel Art Extension saving a PNG**
```luc
// inside an extension's main.luc
on_save_button_clicked() {
    // Get raw pixel data from the extension's internal canvas
    pixels = my_canvas.get_raw_rgba_array()
    
    // Call the engine API to encode and save the PNG
    api.assets.export_png("assets/sprites/hero.png", pixels, width, height)
    
    // The engine's file watcher will notice hero.png and automatically trigger the Baker!
}
```

**Example: An Extension saving a 3D Model**
```luc
on_export_model() {
    // The extension generates vertices and indices
    mesh_data = voxel_generator.build_mesh()
    
    // Call the engine API to build a fully compliant GLTF file
    api.assets.export_gltf("assets/models/level.gltf", mesh_data.vertices, mesh_data.indices)
}
```

By providing these standard export functions in the `api.assets` table, extension developers can build powerful creation tools without needing to understand the complex specifications of PNG or GLTF files. The engine handles the conversion, and then the Baker cooks it just like any other asset.
