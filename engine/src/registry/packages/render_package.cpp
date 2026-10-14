#include "engine/registry/packages/render_package.h"
#include "engine/registry/package_builder.h"

namespace engine
{

    void RenderPackage::register_into(PackageBuilder &b)
    {
        // No node types yet. When the render nodes arrive,
        // register them here. If a node takes a Color argument,
        // declare the Color struct type in the same register_into:
        //
        //   static const lucid::sema::StructFieldInfo colorFields[] = {
        //       {"r", lucid::sema::TypeId::primitive("float32")},
        //       {"g", lucid::sema::TypeId::primitive("float32")},
        //       {"b", lucid::sema::TypeId::primitive("float32")},
        //       {"a", lucid::sema::TypeId::primitive("float32")},
        //   };
        //   b.add_struct(lucid::sema::StructTypeInfo{
        //       "Color",
        //       ArenaSpan<lucid::sema::StructFieldInfo>(colorFields, 4)});
        //
        // The C++ side is engine/render/color.h. The field order
        // there (r, g, b, a) matches the order here.

        (void)b;
    }

} // namespace engine