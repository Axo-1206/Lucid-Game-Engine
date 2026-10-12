#pragma once

#include "engine/physics/backend.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace engine::physics
{

    // A 2D physics backend implemented against Box2D 3.x.
    //
    // See engine/physics/backend.h for the interface contract. This
    // class is the Box2D-specific implementation. It owns a
    // b2WorldId, creates and destroys b2BodyId instances, and
    // translates between the engine's BodyDesc/BodyState and
    // Box2D's b2 types.
    //
    // Threading: single-threaded. All methods must be called on the
    // thread that called init().
    //
    // Lifetime: construct, call init(), step() each frame, call
    // shutdown() before destruction.
    class Box2DBackend : public Backend
    {
    public:
        Box2DBackend();
        ~Box2DBackend() override;

        Box2DBackend(const Box2DBackend &) = delete;
        Box2DBackend &operator=(const Box2DBackend &) = delete;

        // --- Backend interface ---

        bool init() override;
        void shutdown() override;

        void set_gravity(float x, float y) override;
        void step(float dt) override;

        BodyId create_body(const BodyDesc &desc) override;
        void destroy_body(BodyId id) override;

        BodyState get_body_state(BodyId id) const override;

        void set_body_transform(BodyId id, float x, float y, float rot_z) override;
        void set_body_velocity(BodyId id, float vx, float vy) override;

        void apply_force(BodyId id, float fx, float fy) override;
        void apply_impulse(BodyId id, float ix, float iy) override;

        const std::vector<Contact> &contacts() const override;

        // --- Box2D-specific configuration ---

        // Number of sub-steps per step(). Higher is more accurate
        // but slower. Default 4, Box2D's recommendation.
        void set_sub_step_count(int n);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        std::vector<Contact> contacts_;
    };

} // namespace engine::physics