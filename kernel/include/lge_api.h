#pragma once

#include <stdint.h>

#ifdef WIN32
    #ifdef LGE_EXPORT_DLL
        #define LGE_API __declspec(dllexport)
    #else
        #define LGE_API __declspec(dllimport)
    #endif
#else
    #define LGE_API __attribute__((visibility("default")))
#endif

#define LGE_VERSION 0x00010000 // 0.1.0

typedef uint64_t EntityID;

// --- Subsystem Function Tables ---

struct LGE_RenderAPI {
    void (*set_clear_color)(float r, float g, float b, float a);
    void (*draw_mesh)(EntityID entity);
    // ... to be expanded in Phase 1
};

struct LGE_PhysicsAPI {
    void (*set_gravity)(float x, float y, float z);
    void (*apply_force)(EntityID entity, float x, float y, float z);
};

// --- The Master API Table (Decision 2) ---

struct LGE_ExtensionAPI {
    uint32_t version;
    
    LGE_RenderAPI*  render;
    LGE_PhysicsAPI* physics;
    
    // Lifecycle hooks for the extension
    void (*on_load)(LGE_ExtensionAPI* api);
    void (*on_unload)();
    void (*on_update)(float dt);
};

// The entry point for any .lmod / .dll extension
extern "C" LGE_API LGE_ExtensionAPI* LGE_GetAPI(uint32_t version);
