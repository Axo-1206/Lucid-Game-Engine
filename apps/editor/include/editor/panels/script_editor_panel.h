#pragma once

#include "editor/diagnostics.h"
#include "editor/document.h"

#include "engine/registry/package_manager.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace editor
{

    class ScriptEditorPanel
    {
    public:
        ScriptEditorPanel();
        ~ScriptEditorPanel();

        // Wire the compile loop.
        void set_package_manager(const engine::PackageManager *pm);
        void set_diagnostic_store(DiagnosticStore *store);

        // The jump callback: invoked when the user clicks a
        // location in the Problems panel and the panel wants to
        // move the cursor. `line` is 1-based.
        void request_jump(const std::filesystem::path &path,
                          std::uint32_t line);

        int open(const std::filesystem::path &path, std::string &error);
        bool save_active(std::string &error);
        bool save_all(std::string &error);
        void request_close_tab(std::size_t index,
                               std::function<void()> on_done = nullptr);
        void request_close_all(std::function<void()> on_done = nullptr);
        void close_all();

        void draw();
        bool &visible() { return visible_; }

        std::size_t tab_count() const { return documents_.size(); }
        bool has_dirty() const;

    private:
        enum class PromptState
        {
            None,
            CloseTab,
            CloseAll,
        };

        struct CompileState
        {
            std::chrono::steady_clock::time_point last_edit;
            bool pending = false;
        };

        struct JumpRequest
        {
            std::filesystem::path path;
            std::uint32_t line = 0;
            bool active = false;
        };

        std::vector<std::unique_ptr<Document>> documents_;
        std::vector<CompileState> compile_state_;
        int active_tab_ = -1;
        bool visible_ = true;

        const engine::PackageManager *packages_ = nullptr;
        DiagnosticStore *diagnostics_ = nullptr;

        JumpRequest jump_;

        PromptState prompt_ = PromptState::None;
        std::size_t prompt_tab_index_ = 0;
        std::function<void()> prompt_on_done_;

        void draw_tab_bar();
        void draw_active_document();
        void draw_prompt();
        void draw_diagnostic_summary(const Document &doc);

        void compile_document(std::size_t index);
        void maybe_compile();

        void do_close_tab(std::size_t index);
        void perform_close_with_prompt();

        void prompt_discard();
        void prompt_save();
        void prompt_cancel();
    };

} // namespace editor