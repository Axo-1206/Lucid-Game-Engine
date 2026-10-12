#pragma once

#include "engine/physics/backend.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace test
{

    // A minimal in-memory physics::Backend for plumbing tests.
    // Bodies are records; step() integrates velocity into position.
    class StubBackend : public engine::physics::Backend
    {
    public:
        bool init() override
        {
            initialized_ = true;
            return true;
        }
        void shutdown() override
        {
            initialized_ = false;
            bodies_.clear();
        }

        void set_gravity(float x, float y) override
        {
            gravity_x_ = x;
            gravity_y_ = y;
        }

        void step(float dt) override
        {
            ++step_count_;
            for (auto &[id, b] : bodies_)
            {
                b.vel_x += gravity_x_ * dt;
                b.vel_y += gravity_y_ * dt;
                b.pos_x += b.vel_x * dt;
                b.pos_y += b.vel_y * dt;
            }
        }

        engine::physics::BodyId create_body(const engine::physics::BodyDesc &desc) override
        {
            engine::physics::BodyId id{next_id_++};
            engine::physics::BodyState s;
            s.pos_x = desc.pos_x;
            s.pos_y = desc.pos_y;
            s.rot_z = desc.rot_z;
            s.vel_x = desc.vel_x;
            s.vel_y = desc.vel_y;
            s.angular_vel = desc.angular_vel;
            s.awake = true;
            bodies_[id.value] = s;
            return id;
        }

        void destroy_body(engine::physics::BodyId id) override
        {
            bodies_.erase(id.value);
        }

        engine::physics::BodyState get_body_state(engine::physics::BodyId id) const override
        {
            auto it = bodies_.find(id.value);
            return it == bodies_.end() ? engine::physics::BodyState{} : it->second;
        }

        void set_body_transform(engine::physics::BodyId id,
                                float x, float y, float rot_z) override
        {
            auto it = bodies_.find(id.value);
            if (it == bodies_.end())
                return;
            it->second.pos_x = x;
            it->second.pos_y = y;
            it->second.rot_z = rot_z;
            it->second.vel_x = 0.0f;
            it->second.vel_y = 0.0f;
            it->second.angular_vel = 0.0f;
        }

        void set_body_velocity(engine::physics::BodyId id, float vx, float vy) override
        {
            auto it = bodies_.find(id.value);
            if (it == bodies_.end())
                return;
            it->second.vel_x = vx;
            it->second.vel_y = vy;
        }

        void apply_force(engine::physics::BodyId id, float fx, float fy) override
        {
            auto it = bodies_.find(id.value);
            if (it == bodies_.end())
                return;
            // Treat force as instantaneous acceleration for the
            // stub's purposes; real backends divide by mass.
            it->second.vel_x += fx * 0.01f;
            it->second.vel_y += fy * 0.01f;
        }

        void apply_impulse(engine::physics::BodyId id, float ix, float iy) override
        {
            auto it = bodies_.find(id.value);
            if (it == bodies_.end())
                return;
            it->second.vel_x += ix;
            it->second.vel_y += iy;
        }

        const std::vector<engine::physics::Contact> &contacts() const override
        {
            return contacts_;
        }

        // Test accessors.
        int step_count() const { return step_count_; }
        std::size_t body_count() const { return bodies_.size(); }

    private:
        bool initialized_ = false;
        float gravity_x_ = 0.0f;
        float gravity_y_ = 0.0f;
        std::uint64_t next_id_ = 1;
        std::unordered_map<std::uint64_t, engine::physics::BodyState> bodies_;
        std::vector<engine::physics::Contact> contacts_;
        int step_count_ = 0;
    };

} // namespace test