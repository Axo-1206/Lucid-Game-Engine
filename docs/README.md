# Engine documentation

Start at the project root if you haven't already:
[`../README.md`](../README.md) — project overview, Vocabulary section
(Registry vs World vs Graph vs Scene), build instructions, and the
top-level folder map.

---

## Vocabulary

The engine's `Registry` is `lucid::sema::Registry`, the format
library's node vocabulary. The engine's `ComponentRegistry`
(`engine::ComponentRegistry`) is the scene layer's component codec
table. They are different things with similar names. See
[`notes/WorldLifecycle.md`](notes/WorldLifecycle.md) and
[`notes/PackageComposition.md`](notes/PackageComposition.md).

---

## Current documents

- [`Decision.md`](Decision.md) — the design decisions (1–18), the
  format library freeze statement, the vocabulary contract, and the
  gap table.

- [`FileStructure.md`](FileStructure.md) — the repository layout in
  full: every folder, every source file, and a note on what each
  subdirectory is responsible for.

### Notes

- [`notes/Dataflow.md`](notes/Dataflow.md) — the model the engine runs
  on: entities, components, nodes, systems, tables as the interface,
  the engine loop (including views), and struct values.

- [`notes/TriggerModel.md`](notes/TriggerModel.md) — how a Lucid
  graph expresses control flow: value nodes, action nodes, trigger
  nodes, source triggers, gating triggers (`Engine_If_Bool_Node`),
  and who sets the order at each scope.

- [`notes/SceneFormat.md`](notes/SceneFormat.md) — the `.lscene`
  format: top-level shape, camera and entity schemas, component
  definitions, asset path rules, evolution rules (compatible vs.
  breaking changes), and notes on future 3D and backend-swap support.

- [`notes/WorldLifecycle.md`](notes/WorldLifecycle.md) — how a
  `World` is emptied and what survives: `clear`, `clear_rows`, and
  `Scene::apply_to`, and which one to use when.

- [`notes/PackageNaming.md`](notes/PackageNaming.md) — how the engine
  names its content: node types, enum types, handle types, struct
  types, and table names. It covers the prefix convention, the node
  type name convention, how the engine enforces the prefix, and what
  a package author and a script author need to know.

- [`notes/PackageComposition.md`](notes/PackageComposition.md) — what
  a package contributes (node types, executors, types, tables and
  codecs), how it registers them, and how packages use each other's
  content.

- [`notes/EditorMetadata.md`](notes/EditorMetadata.md) — the metadata
  the engine exposes so an editor can build a package list, a node
  palette, and a table inspector, including the operation table.

- [`notes/RenderPasses.md`](notes/RenderPasses.md) — the renderer's
  pass model (world 2D and UI), the viewport, and views.

- [`notes/PhysicsPlan.md`](notes/PhysicsPlan.md) — how physics is
  designed: the backend interface, the `engine.physics_2d` package,
  the components, the systems, and what is deferred.

- [`implementationPlans/kernelImplementationPlan.md`](implementationPlans/kernelImplementationPlan.md)
  — the implementation plan for the kernel.

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