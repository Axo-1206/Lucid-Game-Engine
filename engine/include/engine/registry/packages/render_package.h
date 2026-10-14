#pragma once

#include "engine/registry/package.h"

namespace engine
{

    // The engine's render package: the render-side nodes that write
    // to the render tables (Engine_Sprite and any future render
    // component).
    //
    // Not yet implemented. This header declares the package's
    // identity and its registration entry point. register_into()
    // registers nothing today; when the render nodes arrive, they
    // register here. If a node takes a Color argument, this is also
    // where the Color struct type is declared in the format
    // library's registry.
    //
    // A game that does not use render nodes does not load this
    // package. The engine's render_system reads the world's tables
    // regardless; the package only adds graph-visible node types.
    //
    // The package's public C++ type — Color — lives in
    // engine/render/color.h, not here. The package's register_into
    // and the C++ header are kept in sync by the package's author:
    // the field order in a registry struct declaration matches the
    // member order in the C++ struct.
    class RenderPackage : public Package
    {
    public:
        const char *name() const override { return "engine.render"; }
        const char *prefix() const override { return "Engine"; }
        const char *version() const override { return "0.1.0"; }
        const char *description() const override { return "Render-side nodes."; }
        const char *author() const override { return "Axo"; }
        void register_into(PackageBuilder &b) override;
    };

} // namespace engine