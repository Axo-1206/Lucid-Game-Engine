#include "lge_api.h"
#include <iostream>

int main(int argc, char** argv) {
    // Silence unused parameter warnings
    (void)argc;
    (void)argv;

    std::cout << "Lucid Editor Bootstrap Starting..." << std::endl;
    
    // Attempt to initialize the Kernel API
    LGE_ExtensionAPI* api = LGE_GetAPI(LGE_VERSION);
    
    if (!api) {
        std::cerr << "Failed to initialize Lucid Kernel API! (Version Mismatch?)" << std::endl;
        return 1;
    }

    std::cout << "Lucid Kernel Initialized Successfully." << std::endl;
    std::cout << "API Version: 0x" << std::hex << api->version << std::dec << std::endl;
    
    return 0;
}
