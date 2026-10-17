#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's core package: the fundamental nodes that every
    // game relies on. The registered names are:
    //   Engine_Float_Node, Engine_Int_Node, Engine_EveryFrame_Node,
    //   Engine_Print_Float_Node, Engine_Print_Bool_Node.
    // (The "Engine_<Type>Node" spelling in earlier docs is stale;
    //  the source of truth is core_package.cpp's kNodeTypes.)
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