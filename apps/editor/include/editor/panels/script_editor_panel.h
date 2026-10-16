#pragma once

#include "editor/document.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace editor
{

    // The script editor: a tabbed text editor for .lucid files
    // (and other text files the editor handles).
    //
    // The panel owns its documents. It does not compile them; the
    // compile loop (step 9) is separate and reads from the
    // documents without changing them.
    //
    // Unsaved changes are handled by a modal prompt. Any action
    // that would close dirty documents (closing a tab, closing
    // the project, quitting the editor) routes through
    // `request_close_*`, which shows the prompt and only calls
    // its callback when the user has resolved it.
    class ScriptEditorPanel
    {
    public:
        ScriptEditorPanel();
        ~ScriptEditorPanel();

        // Open a file. If the file is already open, focuses the
        // existing tab. Returns the tab index, or -1 on failure.
        int open(const std::filesystem::path &path, std::string &error);

        // Save the active tab. Returns false and fills `error` on
        // failure (no active tab, or I/O error).
        bool save_active(std::string &error);

        // Save every dirty document. Returns false on the first
        // failure.
        bool save_all(std::string &error);

        // Request closing one tab. If the document is dirty, the
        // panel shows the prompt first. `on_done` is called when
        // the operation completes (whether saved, discarded, or
        // there was nothing to do).
        void request_close_tab(std::size_t index,
                               std::function<void()> on_done = nullptr);

        // Request closing all tabs. If any is dirty, the panel
        // shows the prompt first. `on_done` is called when the
        // operation completes.
        void request_close_all(std::function<void()> on_done = nullptr);

        // Close all tabs without prompting. Used internally after
        // the prompt is resolved.
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

        std::vector<std::unique_ptr<Document>> documents_;
        int active_tab_ = -1;
        bool visible_ = true;

        // Prompt state.
        PromptState prompt_ = PromptState::None;
        std::size_t prompt_tab_index_ = 0;
        std::function<void()> prompt_on_done_;

        void draw_tab_bar();
        void draw_active_document();
        void draw_prompt();

        void do_close_tab(std::size_t index);
        void perform_close_with_prompt();

        // Discard changes and perform the pending action.
        void prompt_discard();
        // Save all dirty documents, then perform the pending
        // action. On a save failure, aborts.
        void prompt_save();
        // Cancel the prompt.
        void prompt_cancel();
    };

} // namespace editor