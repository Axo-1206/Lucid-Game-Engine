#include "lge_api.h"
#include <iostream>

#ifdef WIN32
#include <windows.h>
typedef LGE_ExtensionAPI* (*GetAPI_Func)(uint32_t);
#endif

int main(int argc, char** argv) {
    std::cout << "Lucid Editor Bootstrap Starting..." << std::endl;
    
    // In a real scenario, this would LoadLibrary("luc_kernel.dll")
    // For now, we link it directly for simplicity in Phase 0.
    
    LGE_ExtensionAPI* api = LGE_GetAPI(LGE_VERSION);
    if (!api) {
        std::cerr << "Failed to initialize Lucid Kernel API!" << std::endl;
        return 1;
    }

    std::cout << "API Version: " << std::hex << api->version << std::endl;
    
    return 0;
}
