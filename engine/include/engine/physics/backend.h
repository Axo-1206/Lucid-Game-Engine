#pragma once

#include <cstdint>
#include <vector>

namespace engine::physics
{

    // The kind of a rigid body.
    //
    // - Static:    does not move. Infinite mass. Collides with
    //              dynamic bodies. Used for walls, floors, terrain.
    // - Dynamic:   moves under forces and gravity. Collides with
    //              everything except other static bodies.
    // - Kinematic: moves under script control. Not affected by
    //              gravity or forces. Collides with dynamic bodies.
    //              Used for moving platforms, scripted hazards.
    enum class BodyType : std::uint8_t
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2,
    };

    // The shape of a body's collision geometry.
    //
    // - Box:    an axis-aligned box in the body's local space.
    //           size_x and size_y are half-extents.
    // - Circle: a circle. size_x is the radius; size_y is unused.
    //
    // A future version may add Capsule, Polygon, and Edge shapes.
    // Adding a value to this enum is a compatible change as long as
    // the existing values keep their meaning.
    enum class ShapeType : std::uint8_t
    {
        Box = 0,
        Circle = 1,
    };

    // The description of a body at creation time.
    //
    // Engine-agnostic: any backend interprets this the same way. A
    // backend may have additional fields (a density curve, a
    // material pointer); those are backend-specific and are added
    // through the backend's own configuration, not through this
    // struct.
    struct BodyDesc
    {
        BodyType type = BodyType::Dynamic;
        ShapeType shape = ShapeType::Box;

        // For Box: half-extents. For Circle: size_x is the radius.
        float size_x = 0.5f;
        float size_y = 0.5f;

        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.1f;

        // Initial transform in world space.
        float pos_x = 0.0f;
        float pos_y = 0.0f;
        float rot_z = 0.0f;

        // Initial velocity.
        float vel_x = 0.0f;
        float vel_y = 0.0f;
        float angular_vel = 0.0f;
    };

    // An opaque handle to a body inside a backend. Handle 0 is
    // invalid. The backend issues handles; the engine stores them but
    // never inspects them.
    struct BodyId
    {
        std::uint64_t value = 0;

        bool is_valid() const { return value != 0; }

        bool operator==(const BodyId &other) const { return value == other.value; }
        bool operator!=(const BodyId &other) const { return !(*this == other); }
    };

    // A body's state after a step.
    struct BodyState
    {
        float pos_x = 0.0f;
        float pos_y = 0.0f;
        float rot_z = 0.0f;
        float vel_x = 0.0f;
        float vel_y = 0.0f;
        float angular_vel = 0.0f;
        bool awake = true;
    };

    // A contact produced by the last step. Two bodies, a normal, and
    // a depth. A backend may produce multiple contacts per body pair
    // (one per contact manifold point); the engine treats each as
    // independent.
    struct Contact
    {
        BodyId a;
        BodyId b;
        float normal_x = 0.0f;
        float normal_y = 0.0f;
        float depth = 0.0f;
    };

    // The physics backend interface.
    //
    // A backend simulates bodies. It takes body descriptions at
    // creation, advances the simulation by dt, and reports body
    // state and contacts. It does not know about entities, tables,
    // the graph, or the renderer.
    //
    // Two implementations are planned: Box2DBackend for 2D (the
    // default), and a Jolt-backed implementation for 3D (future).
    // The engine never instantiates a backend directly; a game does,
    // puts it in Services, and loads the physics package.
    //
    // Lifetime:
    //   - Construct the backend.
    //   - Call init() before creating bodies.
    //   - Call step(dt) once per frame.
    //   - Call shutdown() before destroying the backend.
    //
    // Threading: a backend is single-threaded. All methods are called
    // on the main thread. A backend is not safe to use from multiple
    // threads.
    class Backend
    {
    public:
        virtual ~Backend() = default;

        // --- Lifecycle ---

        // Initialize the backend. Returns false on failure; the
        // backend is not usable after a failed init.
        virtual bool init() = 0;

        // Release backend resources. After shutdown, the backend is
        // not usable until init is called again.
        virtual void shutdown() = 0;

        // --- Simulation ---

        // Set the gravity vector. Default is (0, -9.81) for 2D
        // (negative Y is down if the world uses Y-down; the caller
        // chooses the sign based on the world's convention).
        virtual void set_gravity(float x, float y) = 0;

        // Advance the simulation by dt seconds. Updates every body's
        // state and refreshes contacts().
        virtual void step(float dt) = 0;

        // --- Bodies ---

        // Create a body. Returns an invalid BodyId on failure.
        virtual BodyId create_body(const BodyDesc &desc) = 0;

        // Destroy a body. No-op if id is invalid or unknown.
        virtual void destroy_body(BodyId id) = 0;

        // Read a body's current state. Returns a default-constructed
        // BodyState if id is invalid or unknown.
        virtual BodyState get_body_state(BodyId id) const = 0;

        // --- Direct control ---

        // Teleport a body. Sets position and rotation directly and
        // zeroes velocity. Used by a script that wants to move a
        // body without simulating the motion.
        virtual void set_body_transform(BodyId id,
                                        float x, float y, float rot_z) = 0;

        // Set a body's linear velocity directly. Used by a script
        // that wants to override physics for a moment.
        virtual void set_body_velocity(BodyId id, float vx, float vy) = 0;

        // Apply a continuous force for the next step. The force is
        // cleared after the step. Accumulates across multiple calls
        // in the same frame.
        virtual void apply_force(BodyId id, float fx, float fy) = 0;

        // Apply an instantaneous impulse. Takes effect immediately.
        virtual void apply_impulse(BodyId id, float ix, float iy) = 0;

        // --- Contacts ---

        // The contacts produced by the last step. Valid until the
        // next step. The returned reference is owned by the backend
        // and is stable across the frame.
        virtual const std::vector<Contact> &contacts() const = 0;
    };

} // namespace engine::physics