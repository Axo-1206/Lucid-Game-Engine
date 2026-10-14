#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's math package: scalar arithmetic, comparison, and
    // the boolean logic that pairs with them. No vector type in this
    // slice; Vector2 and the vector nodes arrive in a later slice
    // once the graph runtime carries struct values.
    //
    // The package ships no public C++ types. A game uses the nodes
    // from .lucid; the executors are internal to the engine.
    class MathPackage : public Package
    {
    public:
        const char *name() const override { return "engine.math"; }
        const char *prefix() const override { return "Engine"; }
        const char *version() const override { return "0.1.0"; }
        const char *description() const override { return "Scalar arithmetic, comparison, and boolean logic."; }
        const char *author() const override { return "Axo"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine