#pragma once

namespace engine
{

    namespace physics
    {
        class Backend;
    }

    // Engine-wide services. A service is a C++ object that lives
    // across the frame and is reachable from systems and from node
    // executors.
    //
    // A Services instance is constructed by the game (in main.cpp),
    // populated with whatever services the game has, and passed to
    // GraphRuntime and to the SystemContext.
    //
    // The struct is deliberately typed: each field is a specific
    // type, checked at compile time. Adding a service means adding a
    // field. A system that needs a service reads it; a system that
    // does not, ignores it.
    //
    // A null service pointer means "the service is not available."
    // Systems and executors that need a service check for null and
    // return early. A game that does not load a physics backend has
    // Services::physics == nullptr; the physics systems do nothing.
    //
    // This is not a service locator. There is no string-keyed
    // lookup; the fields are fixed at compile time. A future service
    // is a new field, and every consumer updates in the same commit.
    struct Services
    {
        // The physics backend. Null if the game does not use
        // physics, or if the backend failed to initialize.
        physics::Backend *physics = nullptr;

        // Future services:
        //   audio::Backend*   audio   = nullptr;
        //   render::Renderer* debug_renderer = nullptr;
        //   ...
    };

} // namespace engine