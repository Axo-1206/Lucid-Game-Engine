# Lucid Engine: ECS Serialization Proposal

## 1. Why do we need an ECS Serialization format?

The ECS (Entity Component System) holds the entire state of your game. Every player, enemy, light bulb, and invisible trigger is an Entity made up of Components (Transform, Mesh, Health, AI_State).

If the game is closed, the RAM is cleared, and all that data is destroyed. We need a serialization format to "freeze-dry" the ECS into a file on the hard drive, and then "rehydrate" it later.

We need this for four critical reasons:
1.  **Level Editor (Scenes):** When a developer spends 3 hours building a city in the editor, we need to save all those entities to a `.lucscene` file so they can load it tomorrow.
2.  **Prefabs:** If a developer builds a complex "Space Marine" entity (with an inventory, physics collider, and animations), they need to save it as a `.prefab` file so they can spawn 100 of them easily.
3.  **Save Games:** When a player hits "Save Game", the engine needs to capture the exact position and health of every entity so the player can resume later.
4.  **Code Hot-Reloading:** When a developer edits `.luc` source code while the game is running, the engine temporarily serializes the ECS to RAM, compiles and swaps out the binary/DLL, and deserializes the ECS back. This keeps the game running seamlessly without losing state.

---

## 2. The Three Common Solutions

### Solution A: Text-Based (JSON or YAML)
Every entity and component is saved as human-readable text.
*   **Pros:** Perfect for version control (Git). If a developer moves a tree, Git shows exactly which `y_position` changed. Very easy to debug.
*   **Cons:** Very slow for the CPU to parse text back into memory. Files can get very large.

### Solution B: Raw Binary Dump (memcpy)
The engine literally takes the block of RAM holding the components and writes the raw 1s and 0s directly to the hard drive.
*   **Pros:** The fastest possible way to load. Takes almost zero CPU effort.
*   **Cons:** Very dangerous. If a developer adds a new `stamina` float to the Player component, the byte alignment shifts. All old save files instantly become corrupted and crash the game. Not human-readable.

### Solution C: Schema-Based Binary (FlatBuffers / Cap'n Proto)
A highly engineered binary format that guarantees fast loading, but allows you to add/remove variables without breaking old saves.
*   **Pros:** Fast and safe. Forward and backward compatible.
*   **Cons:** Developers have to write annoying schema definition files for every single component they create. Terrible developer experience.

---

## 3. Proposed Solution for Lucid: The Hybrid Approach

For an Indie/AA engine like Lucid, Developer Experience (DX) and Git friendliness are the most important things during development, but speed is the most important thing for the final players.

Therefore, we should use a two-step approach (similar to our Asset Pipeline):

#### Phase 1: In the Editor (JSON)
All scenes (`.lucscene`) and prefabs (`.prefab`) are saved as **JSON** in the `user_projects/` folder.
*   Developers can open `.lucscene` files in VS Code and read them.
*   Git diffs work perfectly.
*   If a team of 3 developers works on the same level, Git can merge their JSON changes easily.

*Example:*
```json
{
  "entity_id": 4092,
  "name": "Health Potion",
  "components": {
    "Transform": { "pos": [10.5, 0, -5], "rot": [0,0,0], "scale": 1.0 },
    "Item": { "heal_amount": 50.0, "is_consumable": true }
  }
}
```

#### Phase 2: Cooking for Release (MessagePack / BSON)
When the developer clicks "Build Game", the `luc_asset_baker` takes the slow JSON files and converts them into **MessagePack** (or BSON). 
*   MessagePack is a "Binary JSON" format. 
*   It operates exactly like JSON, meaning old save files don't break if you add a new variable.
*   However, it is stored in binary, so it is **much smaller and parses 5x to 10x faster** than text JSON.

#### Phase 3: Player Save Games
When a player saves the game at runtime, the engine writes the ECS state directly out as **MessagePack**. It is fast, compact, and perfectly safe from breaking when the developer releases a patch/update.
