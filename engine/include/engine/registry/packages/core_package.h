#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's core package: the fundamental nodes that every
    // game relies on. Engine_Float32_Node, Engine_Int32_Node,
    // Engine_EveryFrame, Engine_PrintNode.
    class CorePackage : public Package
    {
    public:
        const char *name() const override { return "engine.core"; }
        const char *prefix() const override { return "Engine"; }
        const char *version() const override { return "0.1.0"; }
        const char *description() const override { return "The engine's core nodes."; }
        const char *author() const override { return "Axo"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine