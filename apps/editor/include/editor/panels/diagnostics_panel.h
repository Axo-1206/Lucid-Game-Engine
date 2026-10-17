#pragma once

#include "editor/runtime_diagnostics.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace editor
{

    // The Diagnostics panel: a flat list of structured runtime
    // diagnostics. Complements the Problems panel (compile
    // diagnostics) — the two stores are separate by design (EditorPlan
    // Decision 6).
    //
    // Rows are grouped by severity: errors first, then warnings,
    // then notes/info. Each row shows the code (e.g. "E6041" or
    // "E-Ser"), the file (basename), and the message. Clicking a
    // row with a known file invokes the jump callback.
    class DiagnosticsPanel
    {
    public:
        using JumpCallback = std::function<void(const std::filesystem::path &,
                                                std::uint32_t)>;

        DiagnosticsPanel();
        ~DiagnosticsPanel();

        // Register the diagnostic sink with the engine. Called once
        // by EditorApp at init. Idempotent.
        void attach_to_engine();
        void detach_from_engine();

        void set_store(RuntimeDiagnosticStore *store) { store_ = store; }
        void set_jump_callback(JumpCallback cb) { on_jump_ = std::move(cb); }

        void draw();
        bool &visible() { return visible_; }

        // For tests: the store the panel reads. Not for production
        // code.
        RuntimeDiagnosticStore *store() { return store_; }

    private:
        RuntimeDiagnosticStore *store_ = nullptr;
        engine::DiagnosticSinkId sink_id_ = 0;
        bool visible_ = true;

        // Filters.
        bool show_errors_ = true;
        bool show_warnings_ = true;
        bool show_notes_ = false;
        char text_filter_[128] = {};

        JumpCallback on_jump_;

        void draw_filter_bar();
        bool passes_filter(const RuntimeDiagnosticEntry &e) const;
    };

} // namespace editor