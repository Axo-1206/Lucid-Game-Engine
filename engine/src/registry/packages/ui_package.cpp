#include "engine/registry/packages/ui_package.h"
#include "engine/registry/package_builder.h"
#include "engine/ecs/world.h"

namespace engine
{

    void UIPackage::register_into(PackageBuilder &b)
    {
        b.add_table("Engine_UIElement", [](engine::ecs::World &w)
                    {
            auto *t = w.create_table("Engine_UIElement");
            t->add_column("x",            engine::ecs::ColumnType::Float32);
            t->add_column("y",            engine::ecs::ColumnType::Float32);
            t->add_column("w",            engine::ecs::ColumnType::Float32);
            t->add_column("h",            engine::ecs::ColumnType::Float32);
            t->add_column("anchor",       engine::ecs::ColumnType::Int32);
            t->add_column("layer",        engine::ecs::ColumnType::Int32);
            t->add_column("texture_path", engine::ecs::ColumnType::String);
            t->add_column("tint_r",       engine::ecs::ColumnType::Float32);
            t->add_column("tint_g",       engine::ecs::ColumnType::Float32);
            t->add_column("tint_b",       engine::ecs::ColumnType::Float32);
            t->add_column("tint_a",       engine::ecs::ColumnType::Float32);
            t->add_column("visible",      engine::ecs::ColumnType::Bool); });

        // No nodes in V2. A future Engine_UIRectNode, Engine_UISpriteNode,
        // Engine_UISetVisibleNode would register here.
    }

} // namespace engine