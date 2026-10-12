#include "engine/physics/backend_box2d/backend_box2d.h"
#include "engine/core/log.h"

// Box2D headers use MSVC-incompatible constructs under /W4
// (unreferenced parameters, signed/unsigned conversions in macros).
// Push a warning-disable around the include.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244) // float to int conversion
#pragma warning(disable : 4267) // size_t to int conversion
#pragma warning(disable : 4459) // declaration hides global
#endif

#include <box2d/box2d.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace engine::physics
{

    // =============================================================================
    // Impl
    // =============================================================================

    struct Box2DBackend::Impl
    {
        b2WorldId world = b2_nullWorldId;

        // Engine BodyId (as a u64) -> Box2D b2BodyId.
        std::unordered_map<std::uint64_t, b2BodyId> bodies;

        std::uint64_t next_id = 1;

        int sub_step_count = 4;

        bool initialized = false;
    };

    // =============================================================================
    // Helpers
    // =============================================================================

    namespace
    {

        // Pack an engine handle into a void* for Box2D user data.
        // Handles are u64; a uintptr_t is at least 64 bits on every
        // platform we target.
        void *handle_to_user_data(std::uint64_t handle)
        {
            return reinterpret_cast<void *>(static_cast<std::uintptr_t>(handle));
        }

        std::uint64_t user_data_to_handle(void *ud)
        {
            return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ud));
        }

        // Area of a body's shape. Used to compute density from mass.
        float shape_area(const BodyDesc &desc)
        {
            switch (desc.shape)
            {
            case ShapeType::Box:
                return (2.0f * desc.size_x) * (2.0f * desc.size_y);
            case ShapeType::Circle:
                return B2_PI * desc.size_x * desc.size_x;
            }
            return 1.0f;
        }

    } // namespace

    // =============================================================================
    // Lifecycle
    // =============================================================================

    Box2DBackend::Box2DBackend()
        : impl_(std::make_unique<Impl>())
    {
    }

    Box2DBackend::~Box2DBackend()
    {
        if (impl_ && impl_->initialized)
        {
            shutdown();
        }
    }

    bool Box2DBackend::init()
    {
        if (impl_->initialized)
        {
            log_warn("Box2DBackend::init: already initialized");
            return true;
        }

        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = b2Vec2{0.0f, -10.0f};

        impl_->world = b2CreateWorld(&def);
        if (!b2World_IsValid(impl_->world))
        {
            log_error("Box2DBackend: failed to create Box2D world");
            impl_->world = b2_nullWorldId;
            return false;
        }

        impl_->initialized = true;
        log_info("Box2DBackend initialized");
        return true;
    }

    void Box2DBackend::shutdown()
    {
        if (!impl_->initialized)
            return;

        for (auto &[handle, body] : impl_->bodies)
        {
            if (b2Body_IsValid(body))
            {
                b2DestroyBody(body);
            }
        }
        impl_->bodies.clear();

        if (b2World_IsValid(impl_->world))
        {
            b2DestroyWorld(impl_->world);
        }
        impl_->world = b2_nullWorldId;

        impl_->initialized = false;
        log_info("Box2DBackend shut down");
    }

    // =============================================================================
    // Simulation
    // =============================================================================

    void Box2DBackend::set_gravity(float x, float y)
    {
        if (!impl_->initialized)
            return;
        b2World_SetGravity(impl_->world, b2Vec2{x, y});
    }

    void Box2DBackend::set_sub_step_count(int n)
    {
        impl_->sub_step_count = n > 0 ? n : 1;
    }

    void Box2DBackend::step(float dt)
    {
        if (!impl_->initialized)
            return;

        b2World_Step(impl_->world, dt, impl_->sub_step_count);

        // Read the contact events. Box2D 3.x produces a begin event
        // when two shapes start touching. The manifold on the event
        // carries the normal and the initial contact points.
        contacts_.clear();

        b2ContactEvents events = b2World_GetContactEvents(impl_->world);
        for (int i = 0; i < events.beginCount; ++i)
        {
            const b2ContactBeginTouchEvent &ev = events.beginEvents[i];

            const b2BodyId body_a = b2Shape_GetBody(ev.shapeIdA);
            const b2BodyId body_b = b2Shape_GetBody(ev.shapeIdB);

            const std::uint64_t handle_a =
                user_data_to_handle(b2Body_GetUserData(body_a));
            const std::uint64_t handle_b =
                user_data_to_handle(b2Body_GetUserData(body_b));

            // If either handle is zero, the body was created outside
            // this backend. Skip; the engine does not know about it.
            if (handle_a == 0 || handle_b == 0)
                continue;

            Contact c;
            c.a.value = handle_a;
            c.b.value = handle_b;
            c.normal_x = ev.manifold.normal.x;
            c.normal_y = ev.manifold.normal.y;
            // The manifold's first point carries the separation;
            // negative separation means penetration.
            if (ev.manifold.pointCount > 0)
            {
                c.depth = -ev.manifold.points[0].separation;
            }
            else
            {
                c.depth = 0.0f;
            }

            contacts_.push_back(c);
        }
    }

    // =============================================================================
    // Bodies
    // =============================================================================

    BodyId Box2DBackend::create_body(const BodyDesc &desc)
    {
        if (!impl_->initialized)
        {
            log_error("Box2DBackend::create_body: not initialized");
            return BodyId{};
        }

        b2BodyDef body_def = b2DefaultBodyDef();

        switch (desc.type)
        {
        case BodyType::Static:
            body_def.type = b2_staticBody;
            break;
        case BodyType::Dynamic:
            body_def.type = b2_dynamicBody;
            break;
        case BodyType::Kinematic:
            body_def.type = b2_kinematicBody;
            break;
        }

        body_def.position = b2Vec2{desc.pos_x, desc.pos_y};
        body_def.rotation = b2MakeRot(desc.rot_z);
        body_def.linearVelocity = b2Vec2{desc.vel_x, desc.vel_y};
        body_def.angularVelocity = desc.angular_vel;
        body_def.enableSleep = true;
        body_def.isAwake = true;

        const b2BodyId body = b2CreateBody(impl_->world, &body_def);
        if (!b2Body_IsValid(body))
        {
            log_error("Box2DBackend: failed to create body");
            return BodyId{};
        }

        // Assign the engine handle before creating shapes, so the
        // body's user data is set by the time any contact event
        // references it.
        BodyId id;
        id.value = impl_->next_id++;
        b2Body_SetUserData(body, handle_to_user_data(id.value));

        // Shape.
        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.material.friction = desc.friction;
        shape_def.material.restitution = desc.restitution;
        shape_def.enableContactEvents = true;

        // Density from mass. Static bodies have infinite mass; leave
        // density zero. Dynamic and kinematic bodies use mass divided
        // by area.
        const float area = shape_area(desc);
        if (desc.type == BodyType::Static || desc.mass <= 0.0f || area <= 0.0f)
        {
            shape_def.density = 0.0f;
        }
        else
        {
            shape_def.density = desc.mass / area;
        }

        switch (desc.shape)
        {
        case ShapeType::Box:
        {
            const b2Polygon box = b2MakeBox(desc.size_x, desc.size_y);
            b2CreatePolygonShape(body, &shape_def, &box);
            break;
        }
        case ShapeType::Circle:
        {
            b2Circle circle;
            circle.center = b2Vec2{0.0f, 0.0f};
            circle.radius = desc.size_x;
            b2CreateCircleShape(body, &shape_def, &circle);
            break;
        }
        }

        impl_->bodies[id.value] = body;
        return id;
    }

    void Box2DBackend::destroy_body(BodyId id)
    {
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return;

        if (b2Body_IsValid(it->second))
        {
            b2DestroyBody(it->second);
        }
        impl_->bodies.erase(it);
    }

    BodyState Box2DBackend::get_body_state(BodyId id) const
    {
        BodyState state;
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return state;

        const b2BodyId body = it->second;
        if (!b2Body_IsValid(body))
            return state;

        const b2Vec2 pos = b2Body_GetPosition(body);
        const b2Rot rot = b2Body_GetRotation(body);
        const b2Vec2 vel = b2Body_GetLinearVelocity(body);

        state.pos_x = pos.x;
        state.pos_y = pos.y;
        state.rot_z = b2Rot_GetAngle(rot);
        state.vel_x = vel.x;
        state.vel_y = vel.y;
        state.angular_vel = b2Body_GetAngularVelocity(body);
        state.awake = b2Body_IsAwake(body);
        return state;
    }

    // =============================================================================
    // Direct control
    // =============================================================================

    void Box2DBackend::set_body_transform(BodyId id, float x, float y, float rot_z)
    {
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return;
        if (!b2Body_IsValid(it->second))
            return;

        b2Body_SetTransform(it->second, b2Vec2{x, y}, b2MakeRot(rot_z));
        b2Body_SetLinearVelocity(it->second, b2Vec2{0.0f, 0.0f});
        b2Body_SetAngularVelocity(it->second, 0.0f);
        b2Body_SetAwake(it->second, true);
    }

    void Box2DBackend::set_body_velocity(BodyId id, float vx, float vy)
    {
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return;
        if (!b2Body_IsValid(it->second))
            return;

        b2Body_SetLinearVelocity(it->second, b2Vec2{vx, vy});
        b2Body_SetAwake(it->second, true);
    }

    void Box2DBackend::apply_force(BodyId id, float fx, float fy)
    {
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return;
        if (!b2Body_IsValid(it->second))
            return;

        b2Body_ApplyForceToCenter(it->second, b2Vec2{fx, fy}, true);
    }

    void Box2DBackend::apply_impulse(BodyId id, float ix, float iy)
    {
        auto it = impl_->bodies.find(id.value);
        if (it == impl_->bodies.end())
            return;
        if (!b2Body_IsValid(it->second))
            return;

        b2Body_ApplyLinearImpulseToCenter(it->second, b2Vec2{ix, iy}, true);
    }

    // =============================================================================
    // Contacts
    // =============================================================================

    const std::vector<Contact> &Box2DBackend::contacts() const
    {
        return contacts_;
    }

} // namespace engine::physics