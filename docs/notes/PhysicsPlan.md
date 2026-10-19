# Physics plan

This note describes how physics is designed in Lucid: the two
layers (a C++ backend and a package of node types), the component
schema, the sync policy, and the decisions that have been made.
It is a design document; the code follows it.

See also:
- `docs/notes/Dataflow.md` — the table-mediated dataflow model
  that physics follows.
- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  sub-graphs, the scratchpad.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention, and why `Engine_Body` and `Engine_BodyRuntime` are
  physics-owned.
- `docs/notes/SceneFormat.md` — the `.lscene` format, which
  gains an `Engine_Body` component.
- `docs/notes/PackageNaming.md` — how a physics package names
  its content.
- `docs/notes/PackageComposition.md` — how a package registers
  tables, nodes, and services.

---

## 1. The two layers

Physics has two layers, matching the engine's overall split.

**Layer 1 — the C++ backend.** An implementation of the
`engine::physics::Backend` interface. It owns the simulation:
bodies, shapes, forces, contacts, stepping. The default backend
is `engine::physics::Box2DBackend`, wrapping Box2D 3.x. A future
backend wraps Jolt for 3D.

**Layer 2 — the physics package.** `engine.physics_2d`, a
package that registers the `Engine_Body` component, the
`Engine_BodyRuntime` component, the `Engine_BodyRef` handle, the
physics enums, and the physics nodes. The package's node
executors call into the backend.

The split matches the renderer's: `Renderer` is the interface;
`RendererGL` is the OpenGL implementation. `physics::Backend` is
the interface; `physics::Box2DBackend` is the Box2D
implementation.

> [!NOTE]
> The old §1 described the package's content in the format
> library's terms ("registers the `Engine_Body` component, the
> `Engine_BodyRuntime` component, the `Engine_BodyRef` handle,
> the physics enums, and the physics nodes"). Under the pivot,
> the package's primary content is **tables** (the two component
> tables and any plain tables the package needs), and its nodes
> are **Actions** whose executors call the backend and write the
> package's tables and the sub-graph's scratchpad. The "two
> layers" split is unchanged; the package's content list is
> updated.

---

## 2. Which backend, for which dimension

**2D physics uses Box2D.** Box2D is a mature, well-documented
2D physics engine, MIT-licensed, C++17, in version 3.x. It is
the standard 2D physics library.

**3D physics will use Jolt.** When 3D rendering and 3D scenes
exist, a second backend wraps Jolt. The 3D physics package
(`engine.physics_3d`) registers 3D-specific components and
nodes.

**This matches Unity's model.** Unity ships two built-in physics
engines: PhysX for 3D and Box2D for 2D, with separate components
(`Rigidbody` vs `Rigidbody2D`). Lucid does the same, with
separate packages.

**Why not one physics library for both dimensions:** 2D and 3D
physics have different shapes (circles and boxes vs spheres,
boxes, capsules, meshes), different state (2D rotation is a
scalar; 3D rotation is a quaternion), and different performance
characteristics. Using a 3D engine for 2D work is possible but
wasteful; using a 2D engine for 3D is impossible.

**Why not let users bring their own physics:** the editor needs
a physics vocabulary to display; the project validator needs
handle types and node argument types to check. A default physics
package gives both. A user who wants a different backend loads a
package with a different prefix and its own components.

---

## 3. The interface

`engine::physics::Backend` is the interface. It is small and
backend-neutral.

```cpp
namespace engine::physics {

enum class BodyType : std::uint8_t {
    Static,       // does not move
    Dynamic,      // moves under forces
    Kinematic,    // moves under script control, unaffected by forces
};

enum class ShapeType : std::uint8_t {
    Box,
    Circle,
};

struct BodyDesc {
    BodyType type = BodyType::Dynamic;
    ShapeType shape = ShapeType::Box;

    // For Box: half extents. For Circle: size_x is the radius.
    float size_x = 0.5f;
    float size_y = 0.5f;

    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.1f;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float rot_z = 0.0f;

    float vel_x = 0.0f;
    float vel_y = 0.0f;
    float angular_vel = 0.0f;
};

struct BodyId {
    std::uint64_t value = 0;
    bool is_valid() const { return value != 0; }
};

struct BodyState {
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float rot_z = 0.0f;
    float vel_x = 0.0f;
    float vel_y = 0.0f;
    float angular_vel = 0.0f;
    bool awake = true;
};

struct Contact {
    BodyId a;
    BodyId b;
    float normal_x = 0.0f;
    float normal_y = 0.0f;
    float depth = 0.0f;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual void set_gravity(float x, float y) = 0;
    virtual void step(float dt) = 0;

    virtual BodyId create_body(const BodyDesc& desc) = 0;
    virtual void destroy_body(BodyId id) = 0;

    virtual BodyState get_body_state(BodyId id) const = 0;
    virtual void set_body_transform(BodyId id, float x, float y, float rot_z) = 0;
    virtual void set_body_velocity(BodyId id, float vx, float vy) = 0;

    virtual void apply_force(BodyId id, float fx, float fy) = 0;
    virtual void apply_impulse(BodyId id, float ix, float iy) = 0;

    virtual const std::vector<Contact>& contacts() const = 0;
};

} // namespace engine::physics
```

**What the interface is not:**

- It does not know about entities.
- It does not know about tables.
- It does not know about the graph.
- It does not render.

It knows bodies, forces, and contacts. Nothing else.

**What is not in the interface:**

- Joints and constraints.
- Raycasts.
- Character controllers.
- Sensors.
- Sleep thresholds.

Each of these can be added as a method when it is needed. The
interface is small today because small interfaces are easier to
implement, test, and evolve.

---

## 4. The components

Physics uses two component tables, and the physics package owns
both.

**`Engine_Body` — the serializable description.**

```
Engine_Body
├── entity: RowRef      (primary; references Engine_Entity)
├── type: int32         (0 = static, 1 = dynamic, 2 = kinematic)
├── shape: int32        (0 = box, 1 = circle)
├── size_x: float32     (half-extent for box; radius for circle)
├── size_y: float32     (half-extent for box; unused for circle)
├── mass: float32
├── friction: float32
├── restitution: float32
└── tag: string         (for collision filtering)
```

Serialized in the scene. Registered with `add_component`, so the
scene loader and saver handle it through the `ComponentRegistry`.
Populated for entities with an `Engine_Body` component in the
`.lscene` file, and by the graph at runtime for entities it
creates.

**`Engine_BodyRuntime` — the backend's state. Not serialized.**
Registered with `add_table`, so it has no codec.

```
Engine_BodyRuntime
├── entity: RowRef      (primary; references Engine_Entity)
├── backend_id: uint64  (the backend's body id)
├── pos_x: float32
├── pos_y: float32
├── rot_z: float32
├── vel_x: float32
├── vel_y: float32
└── awake: bool
```

Populated by `physics_step_system` after each backend step. Not
serialized; on scene load, the runtime rows are created by the
package's setup, and populated by the first `physics_step_system`
after each body is created.

**The two-table split.** `Engine_Body` describes the body
(engine-agnostic, serializable). `Engine_BodyRuntime` holds the
backend's state (backend-specific, not serialized). If the
backend changes, only `Engine_BodyRuntime` changes; the
description is stable.

**The two tables are physics-owned.** Under the
package-owned-table convention (`PackageTables.md`), the physics
package's nodes are the only sanctioned way to read or write
them. `bob.game` does not call `world.get_table("Engine_BodyRuntime")`
directly; it uses `Engine_BodyPosition_BodyRef_Node` and reads
the scratch cell the node wrote. This is a policy; the engine
cannot enforce it. See `PackageComposition.md` §8.

**`Engine_BodyRef` is a handle.** A handle is an opaque id; the
value lives in `Engine_BodyRuntime`. A node's argument of type
`Engine_BodyRef` is a handle, and the node's executor resolves it
to the `Engine_BodyRuntime` row. The handle is not an entity
reference; it names a body, not the entity the body belongs to.
In practice the handle stores the entity's `RowRef`, and the
executor looks up the runtime row by entity. The two are
conceptually different: an entity might have no body, and a body
handle should not be usable as an entity handle.

> [!NOTE]
> The old §4 described `Engine_BodyRef` as a handle the runtime
> resolves; that's unchanged. The changes are (a) the package
> *owns* the two tables, and (b) `Engine_BodyRef` is produced by
> an `Action` node that writes the handle to a scratch cell, not
> by a `Value` node that returns the handle via
> `ctx.results[node_index]`. The package-ownership note is new;
> see `PackageTables.md`.

---

## 5. The nodes

The `engine.physics_2d` package registers these node types. All
of them are `Action`s; under the pivot, the user surface has no
`Value` nodes.

The node names follow the engine's naming convention
(`Provider_Operation[_ArgType]*_Node`); see `PackageNaming.md`
§2.2. Each node that "reads" a body writes its result to a
scratchpad cell (or a World cell); the user's `Branch` reads the
cell.

**Lifecycle nodes:**

- `Engine_CreateBody_Entity_Node(entity, out_bodyref)` — creates a
  body at runtime for the entity, from its `Engine_Body` row, and
  writes the new `Engine_BodyRef` to `out_bodyref`. `out_bodyref`
  is a scratch cell path.
- `Engine_DestroyBody_BodyRef_Node(body)` — destroys the backend
  body, removes the `Engine_BodyRuntime` row.

**Read nodes (Actions that write their result to a scratch cell):**

- `Engine_BodyPosition_BodyRef_Node(body, out_x, out_y)` — reads
  the body's position; writes `out_x` and `out_y`.
- `Engine_BodyVelocity_BodyRef_Node(body, out_dx, out_dy)` —
  reads the body's velocity; writes `out_dx` and `out_dy`.

**Action nodes (side effects):**

- `Engine_MoveBody_BodyRef_Float_Float_Node(body, dx, dy)` —
  teleports a body.
- `Engine_ApplyForce_BodyRef_Float_Float_Node(body, fx, fy)` —
  applies a force.
- `Engine_ApplyImpulse_BodyRef_Float_Float_Node(body, ix, iy)` —
  applies an impulse.

**Collision node (deferred):**

- `Engine_OnCollision_BodyRef_String_Node(body, tag, on_collision, on_no_collision)`
  — reads the backend's current contacts, checks whether `body`
  collides with anything tagged `tag`, and returns `on_collision`
  or `on_no_collision`. The firing rule is described in §11.

The `Engine_BodyRef` handle is a `Value`-kind argument: it comes
from a scratchpad cell (written by `Engine_CreateBody_Entity_Node`)
or a World cell. The body is a component of an entity; the handle
is the body's reference.

**Why every read is an Action that writes a cell.** The pivot has
no `Value` nodes on the user surface. `Engine_BodyPosition_BodyRef_Node`
must produce a value that a `Branch` can read; the value goes to
a scratchpad cell, and the `Branch` reads the cell. The user's
composition is:

```
sub-graph MyGameplay {
    scratch { body: Engine_BodyRef, x: float32, y: float32, below: bool }
    nodes:
        create   : Engine_CreateBody_Entity_Node(player, "scratch.body")  successor: read
        read     : Engine_BodyPosition_BodyRef_Node("scratch.body", "scratch.x", "scratch.y") successor: cmp
        cmp      : Game_LessThan_Float_Float_Node("scratch.y", 0.0, "scratch.below") successor: gate
        gate     : Engine_Branch_Bool_Node("scratch.below", on_true -> fell, on_false -> done)
        fell     : Game_Log_Node("player fell below 0")  successor: done
        done     : Engine_Yield_Node
}
```

This is the pivot's composition idiom. The old model's "read a
value and pass it as an argument" is replaced by "read a value
into a scratch cell; the next node's path argument names the
cell."

> [!NOTE]
> The old §5 listed "Value nodes" (`Engine_CreateBody_Entity_Node`,
> `Engine_BodyPosition_BodyRef_Node`, `Engine_BodyVelocity_BodyRef_Node`)
> and "Action nodes" and "Trigger nodes" separately. Under the
> pivot, every node is an `Action`; the "read" nodes write their
> result to a scratch cell; the "trigger" node
> (`Engine_OnCollision_*`) takes two successors and returns one
> based on the collision check. The composition example at the
> end of the section is new and shows the pivot's idiom.

---

## 6. The systems

Two engine systems, provided by the engine itself, not the
package.

**`physics_step_system(ctx, dt)`** — runs after `runtime.tick(dt)`.

1. Reads `ctx.services.physics`.
2. If null, returns early (physics disabled).
3. Diffs the world's `Engine_BodyRuntime` against the backend's
   known bodies:
   - If a runtime row exists but has no `backend_id`, this is a
     new body: create it in the backend, store the id.
   - If a runtime row was destroyed, destroy the backend body.
4. Calls `backend.step(dt)`.
5. For each runtime row, reads `backend.get_body_state()` and
   updates the row's `pos_*`, `rot_z`, `vel_*`, `awake`.

**`physics_sync_system(ctx, dt)`** — runs after `physics_step_system`.

1. For each `Engine_BodyRuntime` row:
   - Find the entity's `Engine_Transform` row. If it exists,
     copy `pos_x`, `pos_y`, `rot_z` to `x`, `y`, `rot_z`.
   - Find the entity's `Engine_Velocity` row. If it exists,
     copy `vel_x`, `vel_y` to `dx`, `dy`.

**The engine loop, with physics:**

```
1. input poll
2. runtime.tick(dt)          -- runs all phases; the physics
                             -- phase (if any) is a phase the
                             -- project registered
3. physics_step_system(ctx, dt)
4. physics_sync_system(ctx, dt)
5. hierarchy_system(ctx, dt)
6. (movement_system if it still exists; physics replaces it for
   bodies)
7. hierarchy_system(ctx, dt)
8. renderer.begin_frame
9. for each View: render_system / ui_system
10. renderer.end_frame, present
```

The two physics systems run in that order, in that position.

**Why two systems.** Stepping and syncing are separate concerns.
Stepping talks to the backend; syncing talks to the world. A
future optimization (say, a backend that writes directly to the
world) would change the sync, not the step. Keeping them separate
preserves the option.

**The physics phase.** The engine defines a `physics` phase. The
project registers a sub-graph on it — either the package's
`DefaultPhysics` template or a user-composed one. That sub-graph
runs during `runtime.tick(dt)` (step 2 of the loop), before
`physics_step_system` runs. A node in the phase can apply forces;
`physics_step_system` steps the backend after the phase; the
effects are visible to the next tick.

This is the phase model from `ExecutionModel.md` §3: phases are
engine-owned scheduling boundaries; the project populates them.
The physics phase is not special; it is a phase the engine
declares and the package ships a template for.

---

## 7. The physics owns the position

**Physics owns the position of any entity with an `Engine_Body`
component.**

Concretely:

- `physics_sync_system` writes `pos_*`, `rot_z` to
  `Engine_Transform` each frame. Any value a graph wrote to
  `Engine_Transform` in the same tick is overwritten.

- To move a physics body, a graph calls
  `Engine_MoveBody_BodyRef_Float_Float_Node`. That node tells the
  backend to teleport the body. The next `physics_sync_system`
  writes the new position to `Engine_Transform`.

- Writing `Engine_Transform.x` directly (via a hypothetical
  `Engine_SetTransform_Entity_Float_Float_Node`) does **not**
  move the body. The next sync overwrites it.

**This is a deliberate policy.** A physics body is authoritative
about its own position. The transform is a copy for rendering
and for game logic to read. If a game needs to move a body, it
goes through the physics layer, not through the transform.

**Document this clearly.** It will surprise a contributor who
expects `Engine_Transform` to be authoritative.

---

## 8. Body lifecycle

**Creation.**

- **By the scene loader.** An entity with an `Engine_Body`
  component in the `.lscene` file gets a row in `Engine_Body`.
  When the package loads and the scene is applied, the loader
  also creates an `Engine_BodyRuntime` row with a zero
  `backend_id`. The first `physics_step_system` sees the row,
  creates the backend body, and stores the id.

- **By a node.** `Engine_CreateBody_Entity_Node` takes the entity
  as an argument. Its executor writes an `Engine_BodyRuntime`
  row keyed by that entity, with a zero `backend_id`, and writes
  the new `Engine_BodyRef` to the caller's scratch cell. The
  next `physics_step_system` creates the backend body.

**Destruction.**

- **By the entity being destroyed.** `destroy_entity` removes
  the entity's component rows, including `Engine_Body` and
  `Engine_BodyRuntime`. The next `physics_step_system` sees that
  a runtime row it knew about is gone and destroys the backend
  body.

- **By a node.** `Engine_DestroyBody_BodyRef_Node` removes the
  `Engine_Body` and `Engine_BodyRuntime` rows. The next step
  destroys the backend body.

**The diff.** `physics_step_system` maintains a mapping from
entity `RowRef` to `BodyId`. Each step:

- For each runtime row: if it has no `BodyId`, create a body and
  store the id; if it has one, keep it.
- For each `BodyId` in the mapping: if its runtime row no longer
  exists, destroy the backend body and remove the mapping entry.

The mapping lives in the physics backend wrapper (the
`Box2DBackend` holds it) or in a small engine-side struct. It is
not a table; it is the sync between the tables and the backend's
internal state.

> [!NOTE]
> The old §8 said `Engine_CreateBody_Entity_Node` was a `Value`
> node that "produces an `Engine_BodyRef`". Under the pivot, it
> is an `Action` that writes the handle to a scratch cell. The
> lifecycle (scene loader creates a runtime row with a zero
> `backend_id`; the first `physics_step_system` creates the
> backend body; destruction is by row removal) is unchanged.

---

## 9. The scene schema

The `.lscene` format gains an `Engine_Body` component. Its
fields:

```json
{
  "Engine_Body": {
    "type": "Dynamic",
    "shape": "Box",
    "size": [0.5, 0.5],
    "mass": 1.0,
    "friction": 0.5,
    "restitution": 0.1,
    "tag": "player"
  }
}
```

- `type`: `"Static"`, `"Dynamic"`, or `"Kinematic"`. Default
  `"Dynamic"`.
- `shape`: `"Box"` or `"Circle"`. Default `"Box"`.
- `size`: `[x, y]`. For `Box`, half-extents. For `Circle`, `x`
  is the radius; `y` is ignored. Default `[0.5, 0.5]`.
- `mass`: number. Default `1.0`.
- `friction`: number. Default `0.5`.
- `restitution`: number. Default `0.1`.
- `tag`: string. Default `""`. Used for collision filtering
  with `Engine_OnCollision_BodyRef_String_Node`.

The body's initial position comes from the entity's
`Engine_Transform`. Its initial velocity comes from the entity's
`Engine_Velocity`. The `Engine_Body` component does not duplicate
those.

The `Engine_BodyRuntime` component is not serialized.

**A note on serialization.** `Engine_Body` is registered through
`PackageBuilder::add_component` with a codec that reads and writes
the JSON above. The scene loader and saver dispatch to it through
the `ComponentRegistry`; neither names `Engine_Body` directly.

---

## 10. The services

The physics backend is an engine-wide service. It is passed to
`GraphRuntime` and to `SystemContext`.

```cpp
namespace engine {

struct Services {
    physics::Backend* physics = nullptr;
    // future: audio::Backend* audio; renderer-specific services; etc.
};

} // namespace engine
```

- `SystemContext` gains `Services& services`.
- `TickContext` gains `Services* services`.
- `GraphRuntime` takes `Services&` in its constructor.
- `apps/game/main.cpp` constructs a `Services`, populates it
  with a `Box2DBackend`, and passes it to the runtime and the
  context.

**Why a struct and not a service locator.** A typed struct is
compile-time checked. `ctx.services.physics` fails to compile if
the field doesn't exist. A service locator
(`world.get_service("physics")`) returns `void*` and defers the
error to runtime.

**Why not a field on `World`.** `World` is the storage layer. It
should not know about physics, rendering, or audio. The
`Services` struct is a separate, engine-level concept.

**Why not the package.** The backend is a runtime object. The
package is a registration bundle. They are different things.
Putting the backend on the package would couple the package
system to a specific service.

**When does a package check `Services::physics`?** The
`Physics2DPackage::register_into` does not have access to
`Services`. A separate step, after package loading, checks
whether the services are populated and loads the package only
if the backend is available. Or: the game's `main.cpp` decides
whether to load the physics package based on whether it wants
physics. **The latter is simpler.** A game that wants physics
constructs a `Box2DBackend`, puts it in `Services`, and loads
`Physics2DPackage`. A game that doesn't, doesn't.

**The physics package declares a dependency on `engine.core`.**
This ensures the engine's core tables and the `Services`
struct's `physics` field are available before the physics
package's `register_into` runs. See `PackageComposition.md` §8.1.

> [!NOTE]
> The old §10 said `TickContext` gains `Services* services`; that
> is unchanged. The dependency declaration is new: it ensures
> the physics package is loaded after `engine.core`, which is
> the package that provides `Engine_Entity` and the rest of the
> core tables the physics package's executors assume exist.

---

## 11. Collision handling (deferred)

`Engine_OnCollision_BodyRef_String_Node(body, tag, on_collision, on_no_collision)`
is a control-flow node: it reads the backend's current contacts,
checks whether `body` collides with anything tagged `tag`, and
returns `on_collision` or `on_no_collision`.

The node type is registered, but its firing semantics are
**deferred to a later step**. For now, its executor returns
`on_no_collision` unconditionally (i.e., it behaves as if no
collision occurred).

**Why deferred.** Implementing collision handling requires:

- Reading `Backend::contacts()` after each step.
- Mapping each contact's two `BodyId`s back to their entities.
- Filtering by tag.
- Choosing the successor for each contact.
- Handling the "one node, many contacts" case: a body may
  collide with multiple tagged bodies in one tick. The node
  cannot run once per contact within a single tick; it must
  either (a) return `on_collision` if *any* matching contact
  exists, or (b) run once per matching contact via a loop. The
  choice is a design decision, deferred.

The deferral is scoped: the node type is registered, its
semantics are `on_no_collision` for now, and its behavior is a
later addition. It does not block the physics backend or the
basic node set.

**What "deferred" means concretely:**

- The node type is registered. It appears in the palette.
- Its executor returns `on_no_collision` unconditionally.
- The editor shows the node with a "not yet implemented" badge.
- Documentation says so.

When the collision semantics land, the executor is updated; the
node type's name, arguments, and successors are already in place.
Only the executor's behavior changes.

> [!NOTE]
> The old §11 was "Collision triggers (deferred)" and described
> `Engine_OnCollision_BodyRef_String_Node` as a trigger whose
> "firing semantics are deferred." Triggers don't exist under the
> pivot. The node is now a **control-flow node** (an Action with
> two successors) whose collision check is deferred. The
> deferral reasoning and the "what deferred means" list are
> otherwise unchanged.

---

## 12. Determinism

Box2D is deterministic for the same input sequence on the same
platform. It is not guaranteed to be cross-platform
deterministic.

**Consequences:**

- Single-player games: no issue.
- Replays on the same machine: deterministic.
- Cross-platform replays, lockstep multiplayer, or
  deterministic simulation: not guaranteed. Additional work
  (fixed-point math, careful ordering) is required.

**Document this limitation.** It is a property of Box2D, not a
design choice of the engine.

---

## 13. What is deferred

Beyond collision handling, these are deferred:

- **3D physics.** A future `engine.physics_3d` package with a
  Jolt backend.
- **Joints and constraints.** A future `Backend` method.
- **Raycasts.** A future `Backend` method.
- **Character controllers.** A future `Backend` method.
- **Sensors (non-colliding overlaps).** A future `Backend`
  method.
- **Collision filtering beyond a tag.** A future `Engine_Body`
  field pair (category, mask).
- **Serialization of backend state.** Not needed; the runtime is
  rebuilt from `Engine_Body` on load.
- **Multiple physics worlds.** One backend per game.
- **Threading.** Box2D is single-threaded; the step is on the
  main thread.

---

## 14. The decisions

Summarized:

1. **Physics is Layer 1 (a C++ backend) plus Layer 2 (a
   package).** The backend is `engine::physics::Backend`; the
   package is `engine.physics_2d`.
2. **2D uses Box2D; 3D will use Jolt.** Separate components,
   separate packages. Unity's model.
3. **`Engine_Body` is the serializable description.
   `Engine_BodyRuntime` is the backend's state, not
   serialized.** Both are physics-owned tables.
4. **Physics owns the position of any entity with an
   `Engine_Body` component.**
   `Engine_MoveBody_BodyRef_Float_Float_Node` is how a graph
   moves a body.
5. **Two systems: `physics_step_system` and
   `physics_sync_system`.** The first steps the backend; the
   second copies runtime state to `Engine_Transform` and
   `Engine_Velocity`.
6. **The backend is reached through a `Services` struct.**
   `SystemContext` and `TickContext` gain access to it.
7. **Collision handling is deferred.** The node type exists;
   its executor returns `on_no_collision` for now; its full
   semantics are a later step.
8. **`Engine_Body` is a component with a codec.** The package
   registers it with `add_component`; the scene loader and saver
   reach it through the `ComponentRegistry`.
9. **Determinism is per-platform, not cross-platform.**
   Documented as a limitation.
10. **The interface is small and backend-neutral.** Raycasts,
    joints, sensors, and character controllers are future
    methods.

> [!NOTE]
> Decision 7 changed from "Collision trigger firing is
> deferred" to "Collision handling is deferred," because there
> are no triggers. The other decisions are unchanged.

---

## 15. What this means for the code

The steps to implement physics:

**Step 6a — the interface and services plumbing.** No backend,
no package. `engine::physics::Backend`, `engine::Services`,
`SystemContext` and `TickContext` gain `services`. A stub
backend for tests.

**Step 6b — the Box2D backend.** `engine::physics::Box2DBackend`,
wrapping Box2D 3.x. Tests: a body falls, collides, comes to
rest. No package, no scene integration.

**Step 6c — the physics package and scene integration.** The
`engine.physics_2d` package, the `Engine_Body` and
`Engine_BodyRuntime` tables, the nodes, the two systems, the
scene schema, and the integration test.

**Step 6d — the physics phase template.** A `DefaultPhysics`
sub-graph template the package ships, so a user can register it
on the `physics` phase without composing it by hand.

After Step 6c, a `.lscene` file with an `Engine_Body` component
loads, ticks, and the body falls under gravity. A graph can
apply forces, teleport bodies, and read positions. Collision
handling is registered but returns `on_no_collision`.

> [!NOTE]
> Step 6d is new: the sub-graph template mechanism is new under
> the pivot (see `PackageComposition.md` §11), and the physics
> package should ship a default template for its phase.

---

## 16. Cross-references

- `docs/notes/Dataflow.md` — the model physics follows.
- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  sub-graphs, the scratchpad.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention, and why `Engine_Body` and `Engine_BodyRuntime` are
  physics-owned.
- `docs/notes/SceneFormat.md` — the `.lscene` format, extended
  with `Engine_Body`.
- `docs/notes/PackageNaming.md` — how `engine.physics_2d` names
  its content.
- `docs/Decision.md` — the design decisions that led to this
  plan.