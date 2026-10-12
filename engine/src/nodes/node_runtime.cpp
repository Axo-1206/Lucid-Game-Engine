#include "engine/nodes/node_runtime.h"
#include "engine/nodes/default_registry.h" // ensure_default_applied
#include "engine/package/default_manager.h"

namespace engine
{

    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id)
    {
        // Ensure the default manager is applied.
        (void)default_registry();

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