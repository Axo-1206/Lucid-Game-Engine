#include "engine/registry/default_registry.h"
#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/core/log.h"

namespace engine
{

    namespace
    {

        PackageManager &default_package_manager()
        {
            static auto mgr = []
            {
                auto m = std::make_unique<PackageManager>();
                m->load_package(std::make_unique<CorePackage>());
                m->load_package(std::make_unique<LogicPackage>());
                if (!m->apply_registration_only())
                {
                    for (const auto &c : m->conflicts())
                    {
                        engine::log_error("default_registry: %s", c.c_str());
                    }
                }
                return m;
            }();
            return *mgr;
        }

    } // namespace

    const lucid::sema::Registry &default_registry()
    {
        return default_package_manager().registry();
    }

    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id)
    {
        (void)default_registry(); // ensure applied
        const auto &modes = default_package_manager().trigger_modes();
        if (id >= modes.size())
        {
            static const NodeTypeRuntimeInfo kFallback{};
            return kFallback;
        }
        return modes[id];
    }

    std::uint32_t node_runtime_info_count()
    {
        (void)default_registry();
        return static_cast<std::uint32_t>(
            default_package_manager().trigger_modes().size());
    }

} // namespace engine