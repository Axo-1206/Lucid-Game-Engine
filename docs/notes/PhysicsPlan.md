# Physics plan

This note describes how physics is designed in Lucid: the two
layers (a C++ backend and a package of node types), the component
schema, the sync policy, and the decisions that have been made.
It is a design document; the code follows it.

See also:
- `docs/notes/Dataflow.md` — the table-mediated dataflow model
  that physics follows.
- `docs/notes/SceneFormat.md` — the `.lscene` format, which
  gains an `Engine_Body` component.
- `docs/notes/PackageNaming.md` — how a physics package names
  its content.

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
a physics vocabulary to display; Sema needs handle types and
node arguments to check. A default physics package gives both.
A user who wants a different backend loads a package with a
different prefix and its own components.

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

Physics uses two component tables.

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

Serialized in the scene. Populated by the scene loader for
entities with an `Engine_Body` component in the `.lscene` file,
and by the `Engine_BodyNode` at runtime for entities created by a
graph.

**`Engine_BodyRuntime` — the backend's state. Not serialized.**

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

---

## 5. The nodes

The `engine.physics_2d` package registers these node types.

**Value nodes:**

- `Engine_BodyNode` — creates a body at runtime. Arguments:
  type, shape, size, mass. Produces an `Engine_BodyRef`.
- `Engine_BodyPositionNode(body)` — reads the body's position.
  Produces a `float32`.
- `Engine_BodyVelocityNode(body)` — reads the body's velocity.
  Produces a `float32`.

**Action nodes:**

- `Engine_MoveBodyNode(body, dx, dy)` — teleports a body.
- `Engine_ApplyForceNode(body, fx, fy)` — applies a force.
- `Engine_ApplyImpulseNode(body, ix, iy)` — applies an impulse.

**Trigger nodes:**

- `Engine_OnCollision(body, tag)` — fires when the body
  collides with something tagged `tag`.

**The `Engine_BodyRef` handle.** A body reference is a handle. A
node's argument that is a body takes an `Engine_BodyRef`. The
runtime resolves the handle to the entity's `Engine_BodyRuntime`
row, which holds the `backend_id`.

**What a body reference is *not*: an entity reference.** A body
is a component of an entity. The `Engine_BodyRef` names the
body, not the entity. In practice, the handle stores the entity's
`RowRef`, and the runtime looks up the runtime row by entity. But
the two are conceptually different: an entity might have no
body, and the same body handle should not be usable as an
entity handle.

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
2. runtime.tick(dt)
3. physics_step_system(ctx, dt)
4. physics_sync_system(ctx, dt)
5. hierarchy_system(ctx, dt)
6. (movement_system if it still exists; physics replaces it for
   bodies)
7. hierarchy_system(ctx, dt)
8. render begin
9. render_system(ctx, dt)
10. render end, present
```

The two physics systems run in that order, in that position.

**Why two systems.** Stepping and syncing are separate concerns.
Stepping talks to the backend; syncing talks to the world. A
future optimization (say, a backend that writes directly to the
world) would change the sync, not the step. Keeping them separate
preserves the option.

---

## 7. The physics owns the position

**Physics owns the position of any entity with an `Engine_Body`
component.**

Concretely:

- `physics_sync_system` writes `pos_*`, `rot_z` to
  `Engine_Transform` each frame. Any value a graph wrote to
  `Engine_Transform` in the same tick is overwritten.

- To move a physics body, a graph calls `Engine_MoveBodyNode`.
  That node tells the backend to teleport the body. The next
  `physics_sync_system` writes the new position to
  `Engine_Transform`.

- Writing `Engine_Transform.x` directly (via a hypothetical
  `Engine_SetTransformNode`) does **not** move the body. The
  next sync overwrites it.

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

- **By a node.** `Engine_BodyNode`'s executor creates an
  `Engine_BodyRuntime` row with a zero `backend_id`. The next
  `physics_step_system` creates the backend body.

  Actually — for a node to create a body, the node needs the
  entity too. `Engine_BodyNode` takes the entity as an argument
  (a `BodyRef` or a `RowRef`). Its executor writes
  `Engine_BodyRuntime` keyed by that entity.

**Destruction.**

- **By the entity being destroyed.** `destroy_entity` removes
  the entity's component rows, including `Engine_Body` and
  `Engine_BodyRuntime`. The next `physics_step_system` sees that
  a runtime row it knew about is gone and destroys the backend
  body.

- **By a node.** If there is an `Engine_DestroyBodyNode` (not in
  the initial node set), it removes the `Engine_Body` and
  `Engine_BodyRuntime` rows. The next step destroys the backend
  body.

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
  with `Engine_OnCollision`.

The body's initial position comes from the entity's
`Engine_Transform`. Its initial velocity comes from the entity's
`Engine_Velocity`. The `Engine_Body` component does not duplicate
those.

The `Engine_BodyRuntime` component is not serialized.

**A note on serialization.** The scene loader hardcodes
`Engine_Body` the way it hardcodes `Engine_Transform`. A future
phase might make the loader generic (packages register their own
component loaders), but that is a bigger change and not needed
for Step 6.

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

---

## 11. Collision triggers (deferred)

`Engine_OnCollision(body, tag)` is a trigger that fires when a
body collides with something tagged `tag`.

The node type is registered, but its firing semantics are
**deferred to a later step**. For now, its executor is a no-op.

**Why deferred.** Firing collision triggers requires:

- Reading `Backend::contacts()` after each step.
- Mapping each contact's two `BodyId`s back to their entities.
- Filtering by tag.
- Firing the `OnCollision` node once per matching contact.
- Propagating to its subscribers, with the "fire at most once
  per firing event" rule (see `TriggerModel.md`).

The last point is the complication. The current runtime's
`fired` set is per-tick; a node fires at most once per tick. A
collision trigger can fire multiple times per tick (multiple
contacts). The rule needs to be "per firing event," not "per
tick."

This is a change to the graph runtime's firing model. It is
scoped and understandable, but it is a change. It is deferred so
that Step 6 can focus on the physics backend and the basic node
set.

**What "deferred" means concretely:**

- The node type is registered. It appears in the palette.
- Its executor is a no-op. It never fires.
- Attempting to use it in a `.lucid` file compiles, but the
  trigger never fires.
- Documentation says so.

When the firing semantics are implemented, the node type is
already in place. Only the runtime's firing dispatch changes.

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

Beyond collision triggers, these are deferred:

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
   serialized.**
4. **Physics owns the position of any entity with an
   `Engine_Body` component.** `Engine_MoveBodyNode` is how a
   graph moves a body.
5. **Two systems: `physics_step_system` and
   `physics_sync_system`.** The first steps the backend; the
   second copies runtime state to `Engine_Transform` and
   `Engine_Velocity`.
6. **The backend is reached through a `Services` struct.**
   `SystemContext` and `TickContext` gain access to it.
7. **Collision trigger firing is deferred.** The node type
   exists; the firing semantics are a later step.
8. **The scene loader hardcodes `Engine_Body` for now.** A
   future version makes the loader generic.
9. **Determinism is per-platform, not cross-platform.**
   Documented as a limitation.
10. **The interface is small and backend-neutral.** Raycasts,
    joints, sensors, and character controllers are future
    methods.

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

After Step 6c, a `.lscene` file with an `Engine_Body` component
loads, ticks, and the body falls under gravity. A graph can
apply forces, teleport bodies, and read positions. Collision
triggers are registered but do not fire.

---

## 16. Cross-references

- `docs/notes/Dataflow.md` — the model physics follows.
- `docs/notes/TriggerModel.md` — how collision triggers will
  eventually fire, once the "per firing event" rule is
  implemented.
- `docs/notes/SceneFormat.md` — the `.lscene` format, extended
  with `Engine_Body`.
- `docs/notes/PackageNaming.md` — how `engine.physics_2d` names
  its content.
- `docs/Decision.md` — the design decisions that led to this
  plan.
```