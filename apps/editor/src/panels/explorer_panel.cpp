#include "editor/panels/explorer_panel.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <cstring>

namespace editor
{

    namespace
    {

        // Directories whose contents are never shown in the
        // Explorer. Their presence is a build-system or tool
        // artifact, not project content.
        bool is_skipped_directory(const std::string &name)
        {
            if (name.empty())
                return true;
            if (name[0] == '.')
                return true; // .lucid, .git, .svn, ...
            if (name == "build")
                return true;
            if (name == "out")
                return true;
            if (name == "node_modules")
                return true;
            return false;
        }

        // Files whose names mark them as compiled output or
        // temporary. Never shown.
        bool is_skipped_file(const std::string &name)
        {
            if (name.empty())
                return true;
            if (name[0] == '.')
                return true; // editor swap files

            auto ends_with = [&](const char *suffix)
            {
                const std::size_t n = std::strlen(suffix);
                return name.size() >= n &&
                       name.compare(name.size() - n, n, suffix) == 0;
            };

            if (ends_with(".lucgraph"))
                return true;
            if (ends_with(".tmp"))
                return true;
            if (ends_with(".bak"))
                return true;
            if (ends_with("~"))
                return true;
            return false;
        }

        // Sort: directories first, then files. Within a group,
        // case-insensitive alphabetical.
        bool compare_entries(const ExplorerEntry &a, const ExplorerEntry &b)
        {
            const bool a_dir = (a.kind == ExplorerEntryKind::Directory);
            const bool b_dir = (b.kind == ExplorerEntryKind::Directory);
            if (a_dir != b_dir)
                return a_dir;

            return std::lexicographical_compare(
                a.name.begin(), a.name.end(),
                b.name.begin(), b.name.end(),
                [](unsigned char ca, unsigned char cb)
                {
                    return std::tolower(ca) < std::tolower(cb);
                });
        }

        void walk(const std::filesystem::path &dir, ExplorerEntry &out)
        {
            std::error_code ec;

            std::filesystem::directory_iterator it(
                dir,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            if (ec)
                return;

            for (const auto &dir_entry : it)
            {
                std::error_code entry_ec;

                // Skip symlinks entirely. Following them can produce
                // cycles; listing them as files is confusing.
                if (dir_entry.is_symlink(entry_ec))
                    continue;

                const std::string name = dir_entry.path().filename().string();

                if (dir_entry.is_directory(entry_ec) && !entry_ec)
                {
                    if (is_skipped_directory(name))
                        continue;

                    ExplorerEntry child;
                    child.kind = ExplorerEntryKind::Directory;
                    child.path = dir_entry.path();
                    child.name = name;

                    walk(dir_entry.path(), child);

                    out.children.push_back(std::move(child));
                }
                else if (dir_entry.is_regular_file(entry_ec) && !entry_ec)
                {
                    if (is_skipped_file(name))
                        continue;

                    ExplorerEntry child;
                    child.kind = ExplorerEntryKind::File;
                    child.path = dir_entry.path();
                    child.name = name;

                    out.children.push_back(std::move(child));
                }
            }

            std::sort(out.children.begin(), out.children.end(), compare_entries);
        }

    } // namespace

    ExplorerEntry build_explorer_tree(const std::filesystem::path &root)
    {
        ExplorerEntry tree;
        tree.kind = ExplorerEntryKind::Directory;
        tree.path = root;
        tree.name = root.filename().string();

        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec) || ec)
            return tree;

        walk(root, tree);
        return tree;
    }

    // ---- ExplorerPanel ----

    void ExplorerPanel::set_root(const std::filesystem::path &project_root)
    {
        root_ = project_root;
        refresh();
    }

    void ExplorerPanel::clear()
    {
        root_.clear();
        tree_ = ExplorerEntry{};
        has_tree_ = false;
        pending_open_.reset();
    }

    void ExplorerPanel::refresh()
    {
        if (root_.empty())
        {
            tree_ = ExplorerEntry{};
            has_tree_ = false;
            return;
        }
        tree_ = build_explorer_tree(root_);
        has_tree_ = true;
    }

    std::optional<std::filesystem::path> ExplorerPanel::take_open_request()
    {
        auto result = pending_open_;
        pending_open_.reset();
        return result;
    }

    void ExplorerPanel::draw_entry(const ExplorerEntry &entry)
    {
        // Directories get a tree node; files are leaves.
        if (entry.kind == ExplorerEntryKind::Directory)
        {
            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            // The root is open by default. Its children are shown
            // without a node of their own for the root itself.
            if (entry.path == root_)
                flags |= ImGuiTreeNodeFlags_DefaultOpen;

            // Empty directories: no expand arrow.
            if (entry.children.empty())
                flags |= ImGuiTreeNodeFlags_Leaf |
                         ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::PushID(entry.path.string().c_str());

            const bool open = ImGui::TreeNodeEx(entry.name.c_str(), flags);

            // Click on the directory selects it. Double-click on
            // an empty directory is a no-op; we do not open
            // directories, only expand them.
            (void)open;

            if (open && !entry.children.empty())
            {
                for (const auto &child : entry.children)
                    draw_entry(child);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
        else
        {
            // A file: a selectable. Double-click records an open
            // request.
            ImGui::PushID(entry.path.string().c_str());

            const bool clicked = ImGui::Selectable(
                entry.name.c_str(),
                false,
                ImGuiSelectableFlags_AllowDoubleClick);

            if (clicked && ImGui::IsMouseDoubleClicked(0))
            {
                pending_open_ = entry.path;
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", entry.path.string().c_str());
            }

            ImGui::PopID();
        }
    }

    void ExplorerPanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Explorer", &visible_))
        {
            ImGui::End();
            return;
        }

        // A refresh button and the current root path in a header.
        if (ImGui::Button("Refresh"))
        {
            refresh();
        }
        ImGui::SameLine();
        if (root_.empty())
        {
            ImGui::TextDisabled("(no project)");
        }
        else
        {
            ImGui::TextDisabled("%s", root_.string().c_str());
        }

        ImGui::Separator();

        if (!has_tree_)
        {
            ImGui::TextDisabled("(empty)");
            ImGui::End();
            return;
        }

        // Draw the root's children directly. The root's own name is
        // shown in the header, not as a tree node.
        for (const auto &child : tree_.children)
        {
            draw_entry(child);
        }

        ImGui::End();
    }

} // namespace editor