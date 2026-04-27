# Lucid Engine: Publisher Key Guide

This guide explains what Publisher Keys are, how they work in the Lucid Engine, and how you must protect them. If you are developing extensions for the engine, read this carefully.

---

## 1. What is a Publisher Key?

A Publisher Key is your **digital identity** as an extension developer. Technically, it is an Ed25519 cryptographic key pair consisting of two parts:
* **Public Key:** Your public ID. You share this with the world. It proves that an extension was actually made by you.
* **Private Key (`publisher_private.pem`):** Your secret signing tool. It is the ultimate source of truth for your identity.

### The Private Key: In a Nutshell

Imagine you are a king sending a letter. 
* Your **Public Key** is like your royal crest that everyone recognizes. 
* Your **Private Key** is the actual physical wax seal stamp that sits on your desk. 

When you make an extension, you use your Private Key to stamp it. When players download your extension, their engine looks at the wax seal (the signature). Because everyone knows your crest (Public Key), they know the extension is safely from you.

**Can someone copy my stamp just by looking at the wax seal?**
No. This is the magic of modern cryptography. Looking at a wax seal tells you *who* made it, but it does not tell you the exact atomic shape of the stamp used to make it. In computer terms, the math is a "one-way street." Anyone can use your Public Key to verify the math, but **it is mathematically impossible to reverse-engineer your Private Key** from the signature.

**What happens if you lose the Private Key?**
If you lose your physical stamp, you can never stamp another letter. The engine will literally prevent you from releasing updates to your own extensions, because you can no longer prove you are you. 

**What happens if someone steals your Private Key?**
If someone steals your stamp, they can write a terrible virus, stamp it, and everyone will think you wrote it. Your reputation would be ruined, and players would be infected.

Because of this, the Private Key is the most critical file on your computer.

---

### Technical Details: How the Private Key is Stored

The private key is simply a text file, usually named `publisher_private.pem`, generated and stored locally on your machine (e.g., in `%APPDATA%\LucidEngine\`). 

Because it is so dangerous if stolen, the Lucid Engine **never** stores it as plain text. When you generate the key, the engine asks you for a strong password. It scrambles the file using that password (AES-256 encryption). 

The file on your hard drive looks like this:

```text
-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFDjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIe3nxkP4...
(Many lines of scrambled characters)
Z8YtX5pQwEaV9uL2sK7mN4...
-----END ENCRYPTED PRIVATE KEY-----
```

Because it is scrambled, **even if a hacker steals this file, they cannot use it.** They must know your password to unlock the wax seal stamp inside.

### Why is it needed?
In a game engine with deep access to the system, security is critical. When a user installs your extension, the engine needs mathematical proof that:
1. **Identity:** The extension was actually built by the person who owns the `com.yourname` namespace.
2. **Integrity:** The extension files have not been modified by malware, a bad download, or a third party after you built them.

### What is it used for?
Every time you pack an extension for release, the engine runs a script (`sign_extension.py`) that does the following:
1. It asks you for your passphrase.
2. It temporarily unlocks your `publisher_private.pem` in memory.
3. It takes a mathematical snapshot (SHA-256 hash) of every file in your extension folder.
4. It uses your unlocked private key to "sign" those hashes. 
5. This signature is attached to your extension's `extension.json`. 

When a user tries to install the extension, the engine checks this signature against your Public Key. If it matches, the extension is verified and safe to install.

---

## 2. Becoming a Trusted Publisher

By default, anyone can create an extension. However, extensions without a trusted signature will trigger a large warning when users try to install them: *"⚠️ Unverified Publisher. Do you trust this?"*

### How to become trusted (Phase 1)
Currently, the Lucid Engine uses a curated "Trusted Publishers" list.
1. **Generate your key:** Run the key generation tool in the engine. It will give you your Public Key.
2. **Contact the Team:** DM your Public Key and your `publisher_id` (e.g., `taiax`) to the Lucid Engine team on Discord.
3. **Get Listed:** The team will add your Public Key to the official `trusted_publishers.json` list.
4. **Verified Status:** From that moment on, any extension you sign will install seamlessly for all users with a ✅ **Verified** badge.

### How your extensions relate to your key
Your key grants you exclusive control over your "namespace." If your publisher ID is `alice`, all of your extensions will start with `com.alice.*` (e.g., `com.alice.visual-debugger`). 
Because only *you* have the Private Key, **no one else can ever publish an extension starting with `com.alice.*`**. The engine will reject any imposter.

---

## 3. Protecting Your Key: Best Practices

Your Private Key (`publisher_private.pem`) is the absolute most important file you own as a developer. **If someone else gets it, they become you.**

### How to protect your key
1. **Use a Strong Passphrase:** When you generate the key, the engine will ask for a passphrase. Do not skip this. The passphrase encrypts the file (AES-256) on your hard drive. If a hacker steals the file, it is useless to them without the passphrase.
2. **Never Commit It:** Do NOT accidentally upload your `publisher_private.pem` to GitHub, Discord, or any public server. Add `*.pem` to your `.gitignore` file immediately.
3. **Keep the Recovery Key Offline:** When you generate your key, you also get a `publisher_recovery.pem`. Save this to a USB drive or print it out, and put it in a drawer. Do not leave it on your daily computer.

### Consequences of Key Loss or Leakage

**What if I lose my Private Key? (Hard drive crash, lost laptop)**
* **Consequence:** You can no longer publish updates to your existing extensions. The engine will reject updates because the new signature won't match the old one.
* **The Fix:** You must use your offline `publisher_recovery.pem` to sign a request telling the engine to authorize a new main key. If you lost both keys, you will have to undergo a slow, manual identity verification process with the Lucid team.

**What if my Private Key is stolen or leaked? (Malware, accidental GitHub push)**
* **Consequence:** A malicious actor can now write a virus, pack it into an extension, sign it with your stolen key, and distribute it. Users' engines will see your trusted signature and install the virus without warning, ruining your reputation.
* **The Fix:** If your key is encrypted with a passphrase, you have time. The hacker must crack the AES encryption first. You must immediately use your `publisher_recovery.pem` to permanently **revoke** the stolen key. Any extensions signed by the stolen key will instantly become untrusted worldwide.

---

## Game Data Protection (VFS & Licenses)

While your **Publisher Key** protects your identity as a developer, the **Lucid Engine VFS** protects your actual game assets (3D models, textures, code) from being stolen.

### The Two-Step Security Model (Console-Style DRM)

To prevent people from simply copying your game and sharing it for free, Lucid uses a two-step "Title Key" system:

1.  **The Title Key:** When you "cook" your game for release, the Asset Baker generates a random key and encrypts your entire game into a `.lucid` bundle. This 50GB file is the same for everyone.
2.  **The License Ticket (`license.lge`):** When a player buys your game, they receive a tiny 1KB file. This file contains the Title Key, but it is **locked to their specific PC hardware** using their motherboard UUID.

### Why this is great for you:

*   **No Hardcoded Keys:** There is no "secret password" hidden in your game's code that a hacker can find. The key is derived mathematically from the player's own computer.
*   **Multi-Device Friendly:** If a player owns a Desktop and a Steam Deck, the store platform (like Steam) just gives them two different 1KB tickets. They don't have to download the 50GB game twice.
*   **Offline Support:** Once the player has their ticket, the engine can unlock the assets without an internet connection.

### Your Responsibility:

*   **Keep your AppID Secret:** Your AppID is used in the mathematical formula to generate the hardware keys. Do not share it publicly.
*   **Protect the Title Key:** When the Asset Baker finishes, it will output the Title Key to your private build logs. Treat this key like your private publisher key—if it leaks, anyone can decrypt your `.lucid` files.
