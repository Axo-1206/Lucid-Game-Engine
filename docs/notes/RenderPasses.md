# Render passes

The renderer has two passes today: world 2D and UI. A third
(world 3D) will be added when 3D exists. This note describes the
pass model, the viewport, and what the renderer does and does
not do.

See also `docs/notes/Dataflow.md` §7 for where rendering sits in the
engine loop, `docs/notes/EditorMetadata.md` for what the editor reads
from the engine, and `docs/Decision.md` for the design decisions.

---

## The pass model

A pass is a scope of draws that share a camera and a viewport. The
renderer is stateful, but its state is *scoped*: a caller enters a
pass, draws, and leaves the pass. Between passes, the state is
what it was before.

### World 2D pass

Draws the game's 2D world content. Uses the 2D world camera set
via `set_camera_2d()`. The projection is orthographic, centered on
the camera's position, scaled by the camera's zoom.

    renderer.begin_world_2d();
    render_system(ctx, dt);      // fills a RenderList and submits
    renderer.end_world_2d();

Draw calls between `begin_world_2d()` and `end_world_2d()` land in
this pass.

### UI pass

Draws screen-space overlays: HUD elements, menus, the editor's
panels, minimaps. Uses a fixed orthographic projection from
(0, 0) to (viewport_width, viewport_height), y down.

    renderer.begin_ui();
    ui_system(ctx, dt);
    renderer.end_ui();

The camera is not settable; the pass is always screen-space.

---

## The viewport

The viewport is a rectangle in window pixels: `(x, y, w, h)`,
origin top-left, +y down. The default is the full window.

    renderer.set_viewport(0, 0, 800, 600);   // top-left 800x600
    renderer.set_full_viewport();            // reset to the window

Setting the viewport affects subsequent passes, not the current
one. A caller sets the viewport before a pass; the pass renders
into it.

Uses:

- **Editor preview panel.** A `World2D` view with the viewport
  `(200, 100, 800, 600)`, followed by a `UI` view over the full
  window.
- **Split-screen.** Two `World2D` views: `(0, 0, w/2, h)` for the
  first player, `(w/2, 0, w/2, h)` for the second.
- **Minimap.** A `World2D` view with the viewport
  `(w-200, h-200, 200, 200)` and a different camera.

---

## Views

A `View` (`engine/render/view.h`) is the caller's arrangement of a
pass, a viewport, a camera, and a world. The engine loop iterates a
`std::vector<View>` between `begin_frame` and `end_frame`:

    renderer.begin_frame(clear_color);
    for (const View& view : views) {
        renderer.set_viewport(view.x, view.y, view.w, view.h);
        if (view.pass == View::Pass::World2D) {
            renderer.set_camera_2d(to_camera2d(view.camera, view.w, view.h));
            renderer.begin_world_2d();
            render_system(ctx, dt);
            renderer.end_world_2d();
        } else {
            renderer.begin_ui();
            ui_system(ctx, dt);
            renderer.end_ui();
        }
        renderer.set_full_viewport();
    }
    renderer.end_frame();
    renderer.present();

The renderer's `set_viewport`, `set_camera_2d`, and pass entry points
(`begin_world_2d`, `begin_ui`) are driven by the view loop. The
renderer does not choose a camera or a viewport on its own.

A `View::Pass::World2D` view draws with the view's camera; a
`View::Pass::UI` view draws in screen space. The game's `main.cpp`
builds one `World2D` view and one `UI` view. An editor builds more:
a scene view and a game view, each with its own viewport and camera.

The frame is cleared once, by `begin_frame`, not once per view.

---

## What the renderer does not do

The renderer draws primitives: quads, textures, shapes, lines. It
does not:

- Compute transforms.
- Choose materials.
- Decide what "billboard" or "HUD element" or "nameplate" means.
- Read the world.
- Handle input.

Those are content concerns. They live in components, nodes, and
systems, which produce the primitives the renderer draws.

### Billboards and 2D-in-3D

A billboard is a quad that always faces the camera. It is a
*transform* — a per-frame computation that produces a quad's
position and orientation. It belongs in a node or a system, which
writes the transform to a render component. The renderer draws the
resulting quad.

Case by case:

- **Billboard.** Compute a camera-facing quad's transform, then
  draw it. Content concern.
- **Nameplate.** Project a 3D point to 2D, then draw a UI element
  at that screen position. The renderer exposes the projection
  matrix; the projection and the UI draw are content and pass
  concerns.
- **Minimap.** A second viewport with a second camera. Pass
  orchestration, not a renderer feature.

The renderer draws what it is told. The game decides what to draw.

---

## Layer semantics

The immediate-mode `draw_*` functions draw in submission order.
The `layer` parameter is stored on the draw but does not change
the order in which the renderer issues draws.

To draw in layer order, use a `RenderList`:

    render_list.add_sprite(tex, x, y, w, h, rot, tint, ax, ay, layer);
    render_list.add_rect(x, y, w, h, color, layer);
    render_list.sort();
    render_list.submit(renderer);

`RenderList::sort()` orders by layer first, then by type, then by
texture. This is the canonical way to draw a frame.

---

## The white-texture fallback

If a `draw_sprite` call uses an unknown or invalid texture handle,
the renderer substitutes the white texture. The draw still
happens. This is deliberate: a broken texture reference produces a
visible white rectangle rather than a missing draw.

A future version may log a warning when this happens. For now, the
substitution is silent.