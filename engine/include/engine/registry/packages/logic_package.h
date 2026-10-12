#pragma once

#include "engine/registry/package.h"

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
        const char *version() const override { return "0.1.0"; }
        const char *description() const override { return "Conditional nodes: Bool, LessThan, If."; }
        const char *author() const override { return "Axo"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine