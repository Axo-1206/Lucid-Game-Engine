#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's core package: the fundamental nodes that every
    // game relies on. Engine_Float32Node, Engine_Int32Node,
    // Engine_EveryFrame, Engine_PrintNode.
    class CorePackage : public Package
    {
    public:
        const char *name() const override { return "engine.core"; }
        const char *prefix() const override { return "Engine"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine