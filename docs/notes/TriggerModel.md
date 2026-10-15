# The Lucid trigger model

This note explains how a Lucid graph expresses control flow. It is
written for a reader who knows how to write imperative code and is
about to read their first `.lucid` file. The short version: **control
flow in Lucid is trigger subscription, not statements.** The engine's
scheduler walks the graph; the graph is the control flow.

---

## 1. The three node kinds

Every node in a Lucid graph has one of three kinds. The kind is
declared by the engine, not by the script.

**Value nodes** produce a value. They have no side effects. Reading a
table cell is a Value node. Comparing two values is a Value node.
Doing arithmetic is a Value node. A Value node is evaluated *on
demand*: when something downstream needs its value, the engine
evaluates it.

**Action nodes** perform a side effect. Writing a table cell,
destroying an entity, playing a sound — these are Actions. An Action
runs when it is fired. An Action is not evaluated for a value; it
either runs or it doesn't.

**Trigger nodes** fire. They have no value and no direct side effect.
They are the connectors: a Trigger, when it fires, asks each of its
subscribers to fire. Subscription is written as `on`:

```
node regen = Game_Regen_Entity_Float_Node(player, 5.0) on if_below_max
```

Here `regen` subscribes to `if_below_max`. When `if_below_max` fires,
`regen` runs.

---

## 2. Two modes of trigger

A Trigger has one of two modes, chosen by the engine.

**Source trigger.** The engine fires it directly, each frame. The
canonical example is `Engine_EveryFrame_Node`:

```
node every_frame = Engine_EveryFrame_Node()
```

Every tick, the engine fires `every_frame`. It doesn't wait for
anything; it fires.

**Gating trigger.** The engine fires it *conditionally*. A gating
trigger subscribes to a parent trigger, and it has a condition. When
the parent fires, the engine evaluates the condition. If the
condition is true, the gating trigger fires. If false, it does not.
The canonical example is `Engine_If_Bool_Node`:

```
node if_below_max = Engine_If_Bool_Node(below_max) on every_frame
```

`if_below_max` subscribes to `every_frame`. When `every_frame` fires,
the engine evaluates `below_max`. If it's true, `if_below_max` fires.
If it's false, `if_below_max` does not fire, and its subscribers are
never asked to run.

The package that registers a trigger type declares its mode with
`PackageBuilder::add_trigger_mode`, and the engine's runtime
metadata table (`node_runtime_info`) records it. The modes are
`Source`, `Gating`, and `NotATrigger` (`engine::TriggerMode`).

---

## 3. A conditional, read two ways

Consider:

```
node every_frame  = Engine_EveryFrame_Node()
node hp           = Game_PlayerHp_Entity_Node(player)
node max_hp       = Game_PlayerMaxHp_Entity_Node(player)
node below_max    = Engine_LessThan_Float_Float_Node(hp, max_hp)
node if_below_max = Engine_If_Bool_Node(below_max) on every_frame
node regen        = Game_Regen_Entity_Float_Node(player, 5.0) on if_below_max
```

**As a sentence:** every frame, if hp is below max_hp, regenerate 5.

**As mechanics, for one tick:**

1. The engine fires `every_frame`. It's a source trigger; it fires
   unconditionally.
2. Firing `every_frame` asks each subscriber to fire. The subscriber
   is `if_below_max`.
3. `if_below_max` is a gating trigger. Before it fires, the engine
   evaluates its condition, `below_max`.
4. Evaluating `below_max` evaluates `hp`, which reads the `Player`
   table's `hp` cell for `player`. It evaluates `max_hp`, which reads
   `max_hp`. It compares them, producing a `bool`.
5. If the bool is true, `if_below_max` fires. Firing asks each
   subscriber to fire. The subscriber is `regen`.
6. `regen` is an Action. It runs: reads `player.hp`, adds 5, clamps
   to `max_hp`, writes back.
7. If the bool is false, `if_below_max` does not fire. `regen` is
   never asked to run.

---

## 4. The distinction that matters

**`Engine_EveryFrame_Node` runs every frame.** It's a source trigger. The engine
fires it each tick, unconditionally.

**`Engine_If_Bool_Node` is *asked to fire* every frame, because its parent fires
every frame.** But it *fires* only when its condition is true. Being
asked and firing are different events.

**`Game_Regen_Entity_Float_Node` runs only when `if_below_max` fires.** When the
condition is false, `Game_Regen_Entity_Float_Node` is not asked to run at all.

So in one tick:

- `every_frame` fires: always.
- `if_below_max` is evaluated: always.
- `if_below_max` fires: only if `below_max` is true.
- `regen` runs: only if `if_below_max` fired.

---

## 5. Why this is not `if` inside a loop

The graph *means* "if inside a loop." The mechanism is not that, and
the difference is what a reader needs to internalize.

In an imperative language:

```
loop {
    if condition {
        action
    }
}
```

The loop body runs top to bottom. The `if` selects whether `action`
runs. `condition` is evaluated once per iteration, at the point the
`if` appears in the body.

In Lucid:

```
node every_frame  = Engine_EveryFrame_Node()
node below_max    = Engine_LessThan_Float_Float_Node(hp, max_hp)
node if_below_max = Engine_If_Bool_Node(below_max) on every_frame
node regen        = Game_Regen_Entity_Float_Node(player, 5.0) on if_below_max
```

There is no body. There are nodes and subscriptions. `Engine_EveryFrame_Node` is
a source trigger that fires each tick. `if_below_max` subscribes to
it. When `every_frame` fires, the engine evaluates `below_max`; if
true, `if_below_max` fires; if `if_below_max` fires, `regen` runs.

The subscription structure *is* the loop and the branch. The engine's
scheduler walks it. There is no statement list, no program counter, no
body.

---

## 6. Composition

Conditionals compose by gating gates. Every condition is a Value node
that produces a `bool`. Comparison nodes such as
`Engine_LessThan_Float_Float_Node` and `Engine_LessThan_Int_Int_Node`,
and logic nodes that combine bools, are all Value nodes.
`Engine_If_Bool_Node` is the only gating trigger you need.

Nested conditionals:

```
node every_frame = Engine_EveryFrame_Node()
node a           = ...
node b           = ...
node both        = Engine_And_Bool_Bool_Node(a, b)
node gate1       = Engine_If_Bool_Node(both) on every_frame
node c           = ...
node gate2       = Engine_If_Bool_Node(c) on gate1
node action      = Game_DoThing_Node() on gate2
```

Every frame, if both a and b, then if c, do the thing.

One tick: `every_frame` fires. `gate1` evaluates `both`. If true,
`gate1` fires. `gate2` evaluates `c`. If true, `gate2` fires.
`action` runs.

Chained conditions:

```
node every_frame = Engine_EveryFrame_Node()
node hp_low      = Engine_LessThan_Float_Float_Node(hp, 10.0)
node hp_zero     = Engine_EqualTo_Float_Float_Node(hp, 0.0)
node dying       = Engine_If_Bool_Node(hp_low) on every_frame
node dead        = Engine_If_Bool_Node(hp_zero) on dying
```

Every frame, if hp is low, then if hp is zero, ... .

The mechanism is the same at every depth. The engine evaluates the
condition of a gating trigger before deciding whether to fire it.

---

## 7. Where the side effect happens

Side effects happen in Action nodes, and only in Action nodes. A
Value node never writes. A Trigger never writes. Only an Action
writes.

This is why "regenerate health" is an Action (`Game_Regen_Entity_Float_Node`) and not a
Value node. Reading `Player.hp` is a Value node
(`Game_PlayerHp_Entity_Node`). Writing it back, with the addition applied, is an
Action.

If you want a node that reads a value and writes it back, that node
is an Action. It can read a Value node's output as an argument, but
it runs as an Action, not as a Value.

---

## 8. Ordering within a tick

**Conditions are evaluated before the actions they gate run.**

In the example:

1. `below_max` is evaluated when `if_below_max` is asked to fire.
2. `if_below_max` fires if the condition was true.
3. `regen` runs, reading `hp` and writing `hp`.

So `regen`'s write is visible to nodes evaluated later in the same
tick, but not to `below_max`, which was evaluated in step 1.

If you need a condition to see the state after some action has run,
put the condition after the action in the graph: gate a second
`Engine_If_Bool_Node` on the action's trigger, and it will evaluate after the
action has fired.

If you need a condition to see the state as of the *start* of the
tick, snapshot the relevant columns before dispatch. This is not part
of the v1 design; conditions see current state.

---

## 9. Who sets the order

The graph's `on` clauses set the order of nodes within a tick. Other
orders are set elsewhere:

| Scope                                 | Set by                            | Documented in                         |
| ------------------------------------- | --------------------------------- | ------------------------------------- |
| Frame (input, graph, systems, render) | Engine loop                       | `Dataflow.md` §7                      |
| System order                          | Engine loop                       | `Dataflow.md` §7, `PhysicsPlan.md` §6 |
| Graph order (which node fires when)   | The graph's `on` clauses          | This note                             |
| Node order (argument evaluation)      | `GraphRuntime`                    | `Dataflow.md` §4                      |
| Package registration order            | `main.cpp`'s `load_package` calls | `PackageNaming.md`                    |

The runtime is a graph walker. The graph's `on` subscriptions are
the edges; the source triggers are the roots. The graph author
decides the order; the runtime walks it. A package declares the
trigger mode of its node types (`Source`, `Gating`, `NotATrigger`);
it does not declare an order between node types.

---

## 10. Summary

| Concept                  | What it is                                                                                   |
| ------------------------ | -------------------------------------------------------------------------------------------- |
| Value node               | Produces a value. No side effects. Evaluated on demand.                                      |
| Action node              | Performs a side effect. Runs when fired.                                                     |
| Trigger node             | Fires. Asks subscribers to fire. Source (engine fires) or gating (condition checked).        |
| `Engine_EveryFrame_Node` | A source trigger. Fires each tick.                                                           |
| `Engine_If_Bool_Node`    | A gating trigger. Evaluated each tick its parent fires; fires only if its condition is true. |
| `on`                     | Subscription. `A on B` means "when B fires, A is asked to fire."                             |
| A conditional            | `Engine_If_Bool_Node(cond) on parent`, with `cond` a bool-producing Value node.              |
| A loop                   | A source trigger fired each tick.                                                            |
| A loop with a condition  | A source trigger, a Value node producing bool, an `Engine_If_Bool_Node`, and an Action.      |

The mental model: **the graph is the control flow.** Nodes are not
statements inside a function body. The engine walks the graph, firing
source triggers, evaluating gating conditions, and running actions.
The subscription structure *is* the loop and the branch.

---

## Related

A graph is referenced from a scene by an `Engine_Script` component that holds a
`.lucgraph` path. For the scene format — how entities, components, and
cameras are described and serialized — see `docs/notes/SceneFormat.md`.