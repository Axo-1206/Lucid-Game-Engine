# Lucid Engine: Version Control & Git Strategy

To understand why we chose JSON for scene files, it helps to understand how professional studios use version control (like Git or Perforce) compared to hobbyist tools like Roblox.

---

## 1. How Roblox Works (The Hobbyist Way)
When you save a project in Roblox Studio, it creates a single massive file: `MyGame.rbxl`. 
* This file is a **zipped binary database**. It contains all your scripts, 3D parts, UI, and settings mashed into one unreadable file.
* **Why it's bad for professionals:** If Developer A changes a script, and Developer B moves a tree, and they both save their `.rbxl` files... one of them will overwrite and delete the other's work. You cannot "merge" two binary files together. 
* Roblox solves this by forcing everyone to work online via "Team Create" on their cloud servers, bypassing Git entirely. This is great for kids, but terrible for professional studios that need branching, code reviews, and offline work.

---

## 2. How Unity & Unreal Work (The Professional Way)
Unity and Unreal do not save your entire game into one file. They save your game as thousands of individual files inside a folder structure.
* Scripts are saved as `.cs` or `.cpp` text files.
* Scenes are saved as text files (Unity uses `.unity` files, which are actually just YAML text).
* **Why it's good for professionals:** If Developer A changes a script, and Developer B moves a tree in the scene, Git can read both text files and **merge** the changes together perfectly.

Because Lucid Engine uses JSON for its `.lucscene` files, it follows this professional standard. Your developers can use Git branches, do Pull Requests on GitHub, and work offline.

---

## 3. Can Git handle heavy assets? (Git LFS)
You are completely correct to be worried about this: **Pure Git is terrible at handling large binary files** (like 50MB `.fbx` models or 100MB `.wav` songs). If you change a 100MB file 10 times, your `.git` history folder balloons to 1GB and slows down your entire PC.

**The Industry Solution: Git LFS (Large File Storage)**
Every game studio using Git uses an extension called Git LFS. 
* When you commit a `.json` or `.luc` file, Git saves it normally.
* When you commit a heavy `.png` or `.fbx` file, Git LFS intercepts it. It uploads the heavy file to a special storage server, and just puts a tiny 1-kilobyte "pointer text" inside your local Git history. 
* This keeps your local repository lightning fast, while still tracking the history of your heavy assets in the cloud.

---

## 4. What exactly gets committed to GitHub?
In the Lucid Engine, developers will push their `user_projects/MyGame/` folder to GitHub. 

However, they will use a `.gitignore` file to tell Git to **ignore the `build/` folder**. 

```text
# Example .gitignore for a Lucid Game
build/          # Ignore cooked assets and compiled binaries!
.cache/         # Ignore editor temp files
*.log
```

**Why ignore the build folder?**
Because the `build/` folder contains all the heavy `.ltex` and `.lmesh` files created by the Asset Baker. We never commit generated files to Git. We only commit the **source** files (`.png`, `.gltf`, `.json`). When another developer pulls the repo from GitHub, their local Asset Baker will automatically cook the assets for them.
