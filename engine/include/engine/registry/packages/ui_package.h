#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's UI package: the screen-space UI element table and
    // (later) the nodes that write rows in it. A game that does not
    // use UI does not load this package; the engine's ui_system
    // returns early when the table is absent.
    class UIPackage : public Package
    {
    public:
        const char *name() const override { return "engine.ui"; }
        const char *prefix() const override { return "Engine"; }
        const char *version() const override { return "0.1.0"; }
        const char *description() const override { return "Screen-space UI elements."; }
        const char *author() const override { return "Axo"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine