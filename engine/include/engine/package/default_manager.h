#pragma once

#include "engine/package/package_manager.h"

namespace engine
{

    // The process-lifetime PackageManager that holds the engine's
    // built-in packages (CorePackage, LogicPackage). Used by
    // default_registry() and node_runtime_info() to answer queries from
    // code that predates PackageManager.
    //
    // New code should construct its own PackageManager and pass it to
    // GraphRuntime, rather than relying on this singleton. The
    // singleton exists so that existing call sites keep working.
    PackageManager &default_package_manager();

} // namespace engine