#include "editor/panels/script_editor_panel.h"

#include "engine/core/log.h"

#include "imgui.h"
#include "imgui_stdlib.h" // std::string overloads for InputText*

#include <algorithm>
#include <cctype>

namespace editor
{

    namespace
    {
        constexpr int kCompileDebounceMs = 300;

        // Extensions the script editor will open. A small allowlist.
        bool is_text_extension(const std::string &ext)
        {
            if (ext == ".lucid")
                return true;
            if (ext == ".lscene")
                return true;
            if (ext == ".json")
                return true;
            if (ext == ".txt")
                return true;
            if (ext == ".md")
                return true;
            if (ext == ".cfg")
                return true;
            return false;
        }

        // Case-insensitive extension comparison. Lowercase both
        // sides before comparing.
        std::string lower_ext(const std::filesystem::path &path)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c)
                           {
                               return static_cast<char>(std::tolower(c));
                           });
            return ext;
        }

        // Find the byte offset of the start of line N (1-based) in
        // `text`. If line is 0 or past the end, returns 0 (start of
        // file).
        int line_start_offset(const std::string &text, std::uint32_t line)
        {
            if (line <= 1)
                return 0;
            std::uint32_t current = 1;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '\n')
                {
                    ++current;
                    if (current == line)
                        return static_cast<int>(i + 1);
                }
            }
            return 0;
        }

        // ImGuiInputTextCallback: on the first call, jump the cursor
        // to the requested line.
        struct JumpCallbackData
        {
            bool done = false;
            int target_offset = 0;
        };

        int jump_callback(ImGuiInputTextCallbackData *data)
        {
            auto *jd = static_cast<JumpCallbackData *>(data->UserData);
            if (jd && !jd->done && data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
            {
                data->CursorPos = jd->target_offset;
                data->SelectionStart = jd->target_offset;
                data->SelectionEnd = jd->target_offset;
                jd->done = true;
            }
            return 0;
        }

    } // namespace

    ScriptEditorPanel::ScriptEditorPanel() = default;

    ScriptEditorPanel::~ScriptEditorPanel() = default;

    void ScriptEditorPanel::set_package_manager(const engine::PackageManager *pm)
    {
        packages_ = pm;
    }

    void ScriptEditorPanel::set_diagnostic_store(DiagnosticStore *store)
    {
        diagnostics_ = store;
    }

    bool ScriptEditorPanel::has_dirty() const
    {
        for (const auto &doc : documents_)
            if (doc->dirty())
                return true;
        return false;
    }

    int ScriptEditorPanel::open(const std::filesystem::path &path, std::string &error)
    {
        if (!is_text_extension(lower_ext(path)))
        {
            error = "Not a text file: " + path.string();
            return -1;
        }

        for (std::size_t i = 0; i < documents_.size(); ++i)
        {
            if (documents_[i]->path() == path)
            {
                active_tab_ = static_cast<int>(i);
                return active_tab_;
            }
        }

        auto doc = Document::open(path, error);
        if (!doc)
            return -1;

        documents_.push_back(std::move(doc));
        compile_state_.push_back(CompileState{});
        active_tab_ = static_cast<int>(documents_.size() - 1);

        // Compile immediately so the user sees errors on open.
        compile_document(active_tab_);
        return active_tab_;
    }

    bool ScriptEditorPanel::save_active(std::string &error)
    {
        if (active_tab_ < 0 || active_tab_ >= (int)documents_.size())
        {
            error = "No active document.";
            return false;
        }
        if (!documents_[active_tab_]->save(error))
            return false;

        // Compile after save so the Problems panel is up to date.
        compile_document(active_tab_);

        return true;
    }

    bool ScriptEditorPanel::save_all(std::string &error)
    {
        for (std::size_t i = 0; i < documents_.size(); ++i)
        {
            if (!documents_[i]->dirty())
                continue;
            if (!documents_[i]->save(error))
                return false;
            compile_document(i);
        }
        return true;
    }

    void ScriptEditorPanel::recompile_all()
    {
        if (!packages_ || !diagnostics_)
            return;

        for (std::size_t i = 0; i < documents_.size(); ++i)
            compile_document(i);
    }

    void ScriptEditorPanel::close_all()
    {
        if (diagnostics_)
        {
            for (auto &doc : documents_)
                diagnostics_->clear_for_file(doc->path());
        }
        documents_.clear();
        compile_state_.clear();
        active_tab_ = -1;
    }

    void ScriptEditorPanel::do_close_tab(std::size_t index)
    {
        if (index >= documents_.size())
            return;

        const auto path = documents_[index]->path();
        if (diagnostics_)
            diagnostics_->clear_for_file(path);

        documents_.erase(documents_.begin() + index);
        compile_state_.erase(compile_state_.begin() + index);

        if (documents_.empty())
            active_tab_ = -1;
        else if (active_tab_ >= static_cast<int>(documents_.size()))
            active_tab_ = static_cast<int>(documents_.size()) - 1;
    }

    void ScriptEditorPanel::request_close_tab(std::size_t index,
                                              std::function<void()> on_done)
    {
        if (index >= documents_.size())
        {
            if (on_done)
                on_done();
            return;
        }

        if (!documents_[index]->dirty())
        {
            do_close_tab(index);
            if (on_done)
                on_done();
            return;
        }

        prompt_ = PromptState::CloseTab;
        prompt_tab_index_ = index;
        prompt_on_done_ = std::move(on_done);
    }

    void ScriptEditorPanel::request_close_all(
        std::function<void()> on_done)
    {
        if (!has_dirty())
        {
            close_all();
            if (on_done)
                on_done();
            return;
        }

        prompt_ = PromptState::CloseAll;
        prompt_tab_index_ = 0;
        prompt_on_done_ = std::move(on_done);
    }

    void ScriptEditorPanel::perform_close_with_prompt()
    {
        const PromptState state = prompt_;
        const std::size_t tab = prompt_tab_index_;
        auto on_done = std::move(prompt_on_done_);

        prompt_ = PromptState::None;
        prompt_tab_index_ = 0;
        prompt_on_done_ = nullptr;

        if (state == PromptState::CloseTab)
        {
            do_close_tab(tab);
        }
        else if (state == PromptState::CloseAll)
        {
            close_all();
        }

        if (on_done)
            on_done();
    }

    void ScriptEditorPanel::prompt_discard()
    {
        perform_close_with_prompt();
    }

    void ScriptEditorPanel::prompt_save()
    {
        std::string error;
        if (!save_all(error))
        {
            engine::log_error("Save failed: %s", error.c_str());
            prompt_ = PromptState::None;
            prompt_tab_index_ = 0;
            prompt_on_done_ = nullptr;
            return;
        }
        perform_close_with_prompt();
    }

    void ScriptEditorPanel::prompt_cancel()
    {
        prompt_ = PromptState::None;
        prompt_tab_index_ = 0;
        prompt_on_done_ = nullptr;
    }

    void ScriptEditorPanel::draw_prompt()
    {
        if (prompt_ == PromptState::None)
            return;

        if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("There are unsaved changes.");
            ImGui::Text("Do you want to save them?");
            ImGui::Separator();

            if (ImGui::Button("Save", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
                prompt_save();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
                prompt_discard();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
                prompt_cancel();
            }

            ImGui::EndPopup();
        }
        else
        {
            // The popup isn't open yet; open it.
            ImGui::OpenPopup("Unsaved Changes");
        }
    }

    void ScriptEditorPanel::draw_tab_bar()
    {
        if (!ImGui::BeginTabBar("##script_tabs",
                                ImGuiTabBarFlags_Reorderable |
                                    ImGuiTabBarFlags_AutoSelectNewTabs |
                                    ImGuiTabBarFlags_FittingPolicyScroll))
            return;

        for (std::size_t i = 0; i < documents_.size(); ++i)
        {
            Document &doc = *documents_[i];

            const std::string label =
                doc.dirty() ? doc.name() + " *" : doc.name();

            ImGui::PushID(static_cast<int>(i));

            bool open = true;
            ImGuiTabItemFlags flags = 0;
            if (doc.dirty())
                flags |= ImGuiTabItemFlags_UnsavedDocument;

            if (ImGui::BeginTabItem(label.c_str(), &open, flags))
            {
                active_tab_ = static_cast<int>(i);
                ImGui::EndTabItem();
            }

            ImGui::PopID();

            if (!open)
            {
                // Defer the close to next frame: we're mid-iteration.
                // The panel stores the request and processes it after
                // EndTabBar.
                request_close_tab(i, nullptr);
                break;
            }
        }

        ImGui::EndTabBar();
    }

    void ScriptEditorPanel::draw_active_document()
    {
        if (active_tab_ < 0 || active_tab_ >= (int)documents_.size())
        {
            ImGui::TextDisabled("(no file open)");
            return;
        }

        Document &doc = *documents_[active_tab_];

        draw_diagnostic_summary(doc);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

        JumpCallbackData jd;
        if (jump_.active && jump_.path == doc.path())
        {
            jd.target_offset = line_start_offset(doc.text(), jump_.line);
            flags |= ImGuiInputTextFlags_CallbackAlways;
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputTextMultiline("##content",
                                      &doc.mutable_text(),
                                      avail,
                                      flags,
                                      jump_.active && jump_.path == doc.path()
                                          ? &jump_callback
                                          : nullptr,
                                      &jd))
        {
            doc.mark_dirty();
            if (active_tab_ < (int)compile_state_.size())
            {
                compile_state_[active_tab_].last_edit =
                    std::chrono::steady_clock::now();
                compile_state_[active_tab_].pending = true;
            }
        }

        // The jump is consumed after one frame.
        if (jump_.active && jump_.path == doc.path())
        {
            jump_.active = false;
        }
    }

    void ScriptEditorPanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Script Editor", &visible_))
        {
            ImGui::End();
            return;
        }

        if (documents_.empty())
        {
            ImGui::TextDisabled("(no file open)");
            ImGui::TextDisabled("Double-click a file in the Explorer.");
        }
        else
        {
            draw_tab_bar();
            draw_active_document();
        }

        draw_prompt();
        ImGui::End();

        // Compile after drawing the UI. The panel is drawn, the input
        // has had its chance to mark edits, and the debounce timer
        // runs here.
        maybe_compile();
    }

    void ScriptEditorPanel::draw_diagnostic_summary(const Document &doc)
    {
        if (!diagnostics_)
            return;

        const auto entries = diagnostics_->for_file(doc.path());
        if (entries.empty())
            return;

        int errors = 0;
        int warnings = 0;
        for (const auto &e : entries)
        {
            using S = engine::CompileDiagnostic::Severity;
            if (e.severity == S::Error || e.severity == S::Fatal)
                ++errors;
            else if (e.severity == S::Warning)
                ++warnings;
        }

        if (errors > 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "%d error%s", errors, errors == 1 ? "" : "s");
            ImGui::SameLine();
        }
        if (warnings > 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                               "%d warning%s", warnings, warnings == 1 ? "" : "s");
            ImGui::SameLine();
        }
        ImGui::TextDisabled("(see Problems)");
        ImGui::Separator();
    }

    void ScriptEditorPanel::compile_document(std::size_t index)
    {
        if (index >= documents_.size())
            return;
        if (!packages_ || !diagnostics_)
            return;

        Document &doc = *documents_[index];

        const auto result = engine::check_source(doc.text(),
                                                 doc.path().string(),
                                                 *packages_);

        std::vector<DiagnosticEntry> entries;
        entries.reserve(result.diagnostics.size());
        for (const auto &d : result.diagnostics)
        {
            DiagnosticEntry e;
            e.file = doc.path();
            e.line = d.line;
            e.column = d.column;
            e.severity = d.severity;
            e.message = d.message;
            e.source = DiagnosticSource::Compiler;
            entries.push_back(std::move(e));
        }

        diagnostics_->replace_for_file(doc.path(), std::move(entries));
    }

    void ScriptEditorPanel::maybe_compile()
    {
        if (active_tab_ < 0 || active_tab_ >= (int)documents_.size())
            return;
        if (active_tab_ >= (int)compile_state_.size())
            return;

        CompileState &cs = compile_state_[active_tab_];
        if (!cs.pending)
            return;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now - cs.last_edit)
                                 .count();
        if (elapsed < kCompileDebounceMs)
            return;

        compile_document(active_tab_);
        cs.pending = false;
    }

    void ScriptEditorPanel::request_jump(const std::filesystem::path &path,
                                         std::uint32_t line)
    {
        // If the file isn't open, open it.
        bool found = false;
        for (auto &doc : documents_)
        {
            if (doc->path() == path)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::string error;
            if (open(path, error) < 0)
            {
                engine::log_error("%s", error.c_str());
                return;
            }
        }
        else
        {
            // Focus the existing tab.
            for (std::size_t i = 0; i < documents_.size(); ++i)
            {
                if (documents_[i]->path() == path)
                {
                    active_tab_ = static_cast<int>(i);
                    break;
                }
            }
        }

        jump_.path = path;
        jump_.line = line;
        jump_.active = true;
        visible_ = true;
    }

} // namespace editor