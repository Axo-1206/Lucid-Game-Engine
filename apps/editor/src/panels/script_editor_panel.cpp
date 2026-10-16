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

    } // namespace

    ScriptEditorPanel::ScriptEditorPanel() = default;

    ScriptEditorPanel::~ScriptEditorPanel() = default;

    bool ScriptEditorPanel::has_dirty() const
    {
        for (const auto &doc : documents_)
            if (doc->dirty())
                return true;
        return false;
    }

    int ScriptEditorPanel::open(const std::filesystem::path &path,
                                std::string &error)
    {
        if (!is_text_extension(lower_ext(path)))
        {
            error = "Not a text file: " + path.string();
            return -1;
        }

        // Existing tab? Focus it.
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
        active_tab_ = static_cast<int>(documents_.size() - 1);
        return active_tab_;
    }

    bool ScriptEditorPanel::save_active(std::string &error)
    {
        if (active_tab_ < 0 || active_tab_ >= static_cast<int>(documents_.size()))
        {
            error = "No active document.";
            return false;
        }
        return documents_[active_tab_]->save(error);
    }

    bool ScriptEditorPanel::save_all(std::string &error)
    {
        for (auto &doc : documents_)
        {
            if (!doc->dirty())
                continue;
            if (!doc->save(error))
                return false;
        }
        return true;
    }

    void ScriptEditorPanel::close_all()
    {
        documents_.clear();
        active_tab_ = -1;
    }

    void ScriptEditorPanel::do_close_tab(std::size_t index)
    {
        if (index >= documents_.size())
            return;

        documents_.erase(documents_.begin() + index);

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
        if (active_tab_ < 0 || active_tab_ >= static_cast<int>(documents_.size()))
        {
            ImGui::TextDisabled("(no file open)");
            return;
        }

        Document &doc = *documents_[active_tab_];

        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

        if (ImGui::InputTextMultiline("##content",
                                      &doc.mutable_text(),
                                      avail,
                                      flags))
        {
            doc.mark_dirty();
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
    }

} // namespace editor