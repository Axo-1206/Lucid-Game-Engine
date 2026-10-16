#pragma once

#include "editor/diagnostics.h"

#include <cstdint>
#include <filesystem>
#include <functional>

namespace editor
{

    // The Problems panel: a flat list of diagnostics, one row per
    // entry. Clicking a row invokes the jump callback with the
    // file and line.
    class ProblemsPanel
    {
    public:
        using JumpCallback = std::function<void(const std::filesystem::path &,
                                                std::uint32_t)>;

        void set_store(DiagnosticStore *store) { store_ = store; }
        void set_jump_callback(JumpCallback cb) { on_jump_ = std::move(cb); }

        void draw();
        bool &visible() { return visible_; }

    private:
        DiagnosticStore *store_ = nullptr;
        JumpCallback on_jump_;
        bool visible_ = true;
        bool show_notes_ = false;
    };

} // namespace editor