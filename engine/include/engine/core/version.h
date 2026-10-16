#pragma once

namespace engine
{

    // The engine's version. Informational. The editor's About
    // dialog and the project file's `engine_version` field read
    // it; nothing enforces it at runtime. The build enforces
    // compatibility: the editor, the engine, and the format
    // library are built together by the same CMake.
    inline constexpr const char *kEngineVersion = "0.1.0";

} // namespace engine