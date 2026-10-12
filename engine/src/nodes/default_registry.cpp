#include "engine/nodes/default_registry.h"
#include "engine/package/default_manager.h"
#include "engine/core/log.h"

namespace engine
{

    namespace
    {

        // The default manager needs a World to apply() against. Core and
        // logic packages in Phase 10 do not register tables, so a
        // throwaway World is fine. When packages register tables, callers
        // will move to their own PackageManager.
        bool ensure_default_applied()
        {
            static const bool ok = []
            {
                static engine::ecs::World scratch;
                if (!default_package_manager().apply(scratch))
                {
                    for (const auto &c : default_package_manager().conflicts())
                    {
                        engine::log_error("default_registry: %s", c.c_str());
                    }
                    return false;
                }
                return true;
            }();
            return ok;
        }

    } // namespace

    const lucid::sema::Registry &default_registry()
    {
        ensure_default_applied();
        return default_package_manager().registry();
    }

} // namespace engine