# Engine documentation

Start at the project root if you haven't already:
[`../README.md`](../README.md) — project overview, Vocabulary section
(Registry vs World vs Graph vs Scene), build instructions, and the
top-level folder map.

---

## Current documents

- [`Decision.md`](Decision.md) — the design decisions, one per phase
  of the engine rework (Phases 0–8). Includes the format library
  freeze statement, the vocabulary contract, and the resolved gap
  table.

- [`FileStructure.md`](FileStructure.md) — the repository layout in
  full: every folder, every source file, and a note on what each
  subdirectory is responsible for.

- [`notes/TriggerModel.md`](notes/PackageNaming.md.md) — how the 
engine names its content: node types, enum types, handle types, 
and table names. It explains the prefix convention, why it exists, 
how the engine enforces it, and what a package author and a 
script author need to know.

- [`notes/TriggerModel.md`](notes/TriggerModel.md) — how a Lucid
  graph expresses control flow: value nodes, action nodes, trigger
  nodes, source triggers, gating triggers (`Engine_IfNode`), and the
  ordering contract within a tick.

- [`notes/SceneFormat.md`](notes/SceneFormat.md) — the `.lscene`
  format: top-level shape, camera and entity schemas, component
  definitions, asset path rules, evolution rules (compatible vs.
  breaking changes), and notes on future 3D and backend-swap support.

---

## Archive

[`archive/`](archive/) contains older design documents kept for
history. They describe earlier iterations of the engine and are not
current. Do not treat them as authoritative; use the documents above.

---

## Format library docs

The Lucid format library has its own documentation. After the first
`cmake -S . -B build`, its docs land under:

```
build/_deps/lucid_format-src/docs/
```

Relevant files there: `Architecture.md`, `Grammar.md`,
`notes/RegistryContractNote.md`. Those docs describe the library
itself; this folder describes the engine that uses it.
