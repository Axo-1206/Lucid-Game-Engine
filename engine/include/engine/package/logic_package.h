#pragma once

#include "engine/package/package.h"

namespace engine
{

    // The engine's logic package: Engine_BoolNode, Engine_LessThanNode,
    // Engine_IfNode. The three nodes a graph needs to express a
    // conditional.
    class LogicPackage : public Package
    {
    public:
        const char *name() const override { return "engine.logic"; }
        const char *prefix() const override { return "Engine"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine