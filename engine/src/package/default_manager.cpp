#include "engine/package/default_manager.h"
#include "engine/package/core_package.h"
#include "engine/package/logic_package.h"

#include <memory>

namespace engine
{

    PackageManager &default_package_manager()
    {
        static auto mgr = []
        {
            auto m = std::make_unique<PackageManager>();
            m->load_package(std::make_unique<CorePackage>());
            m->load_package(std::make_unique<LogicPackage>());
            return m;
        }();
        return *mgr;
    }

} // namespace engine