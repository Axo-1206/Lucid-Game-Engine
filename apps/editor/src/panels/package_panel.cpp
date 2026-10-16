#include "editor/panels/package_panel.h"

#include "engine/registry/operation.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>

namespace editor
{

    namespace
    {

        bool icontains(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
                return true;
            if (needle.size() > haystack.size())
                return false;

            auto to_lower = [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            };

            const std::size_t last = haystack.size() - needle.size();
            for (std::size_t i = 0; i <= last; ++i)
            {
                bool match = true;
                for (std::size_t j = 0; j < needle.size(); ++j)
                {
                    if (to_lower(haystack[i + j]) != to_lower(needle[j]))
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        const char *kind_name(lucid::sema::NodeKind kind)
        {
            using K = lucid::sema::NodeKind;
            switch (kind)
            {
            case K::Value:
                return "Value";
            case K::Action:
                return "Action";
            case K::Trigger:
                return "Trigger";
            }
            return "?";
        }

        // Print a string_view safely. Use this for every registry
        // name that reaches the UI.
        void text_unformatted(std::string_view sv)
        {
            ImGui::TextUnformatted(sv.data(), sv.data() + sv.size());
        }

        // The registry has findEnum, findHandle, findStruct, but not
        // findNodeType. This does the linear search.
        const lucid::sema::NodeTypeInfo *find_node_type(
            const lucid::sema::Registry &reg,
            std::string_view name)
        {
            for (const auto &nt : reg.nodeTypes)
            {
                if (nt.name == name)
                    return &nt;
            }
            return nullptr;
        }

    } // namespace

    void PackagePanel::set_manager(const engine::PackageManager *manager)
    {
        manager_ = manager;
        selected_package_ = 0;
        selected_node_ = -1;
        selected_table_ = -1;
        selected_enum_ = -1;
        selected_handle_ = -1;
        selected_struct_ = -1;
    }

    void PackagePanel::clear()
    {
        selected_package_ = 0;
        selected_node_ = -1;
        selected_table_ = -1;
        selected_enum_ = -1;
        selected_handle_ = -1;
        selected_struct_ = -1;
        node_filter_[0] = '\0';
        table_filter_[0] = '\0';
        enum_filter_[0] = '\0';
        handle_filter_[0] = '\0';
        struct_filter_[0] = '\0';
    }

    const engine::PackageInfo *PackagePanel::current_package() const
    {
        if (!manager_)
            return nullptr;

        const auto &infos = manager_->package_infos();
        if (selected_package_ < 0 ||
            selected_package_ >= static_cast<int>(infos.size()))
            return nullptr;

        return &infos[selected_package_];
    }

    void PackagePanel::draw_package_list()
    {
        if (!manager_)
            return;

        const auto &infos = manager_->package_infos();

        ImGui::Text("Packages (%zu)", infos.size());
        ImGui::Separator();

        for (std::size_t i = 0; i < infos.size(); ++i)
        {
            const auto &info = infos[i];

            ImGui::PushID(static_cast<int>(i));

            // PackageInfo::name is a std::string, so %s is safe.
            const std::string label = info.name + "  v" + info.version;

            if (ImGui::Selectable(label.c_str(),
                                  selected_package_ == static_cast<int>(i)))
            {
                // Changing the package resets all selections, since
                // the lists are per-package.
                selected_package_ = static_cast<int>(i);
                selected_node_ = -1;
                selected_table_ = -1;
                selected_enum_ = -1;
                selected_handle_ = -1;
                selected_struct_ = -1;
            }

            ImGui::SameLine();
            ImGui::TextDisabled("[read-only]");

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Author: %s\n%s",
                                  info.author.c_str(),
                                  info.description.c_str());
            }

            ImGui::PopID();
        }
    }

    void PackagePanel::draw_package_metadata()
    {
        const auto *info = current_package();
        if (!info)
            return;

        ImGui::Text("%s  v%s  [read-only]",
                    info->name.c_str(), info->version.c_str());
        ImGui::TextDisabled("Prefix: %s  |  Author: %s",
                            info->prefix.c_str(), info->author.c_str());

        if (!info->description.empty())
        {
            ImGui::TextWrapped("%s", info->description.c_str());
        }

        ImGui::TextDisabled("%zu node types, %zu tables, %zu enums, "
                            "%zu handles, %zu structs",
                            info->node_type_count,
                            info->table_count,
                            info->enum_count,
                            info->handle_count,
                            info->struct_count);
    }

    void PackagePanel::draw_collection(
        const char *id,
        const std::vector<std::string> &names,
        int &selected,
        char *filter,
        std::size_t filter_size,
        const std::function<void(int)> &draw_detail)
    {
        ImGui::PushID(id);

        // Left column: the filter and the list.
        ImGui::BeginChild("##list",
                          ImVec2(260.0f, 0.0f),
                          ImGuiChildFlags_Borders);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##filter", "filter...",
                                 filter, filter_size);
        ImGui::Separator();

        ImGui::BeginChild("##scroll", ImVec2(0, 0));
        for (int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            if (!icontains(names[i], filter))
                continue;

            ImGui::PushID(i);
            if (ImGui::Selectable(names[i].c_str(), selected == i))
                selected = i;
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::EndChild();

        ImGui::SameLine();

        // Right column: the detail of the selected row.
        ImGui::BeginChild("##detail", ImVec2(0, 0), ImGuiChildFlags_Borders);

        if (selected >= 0 && selected < static_cast<int>(names.size()))
            draw_detail(selected);
        else
            ImGui::TextDisabled("(select an item)");

        ImGui::EndChild();

        ImGui::PopID();
    }

    void PackagePanel::draw_node_types_tab()
    {
        const auto *info = current_package();
        if (!info)
            return;

        const auto &reg = manager_->registry();
        const auto &names = info->node_type_names;

        draw_collection(
            "nodes",
            names,
            selected_node_,
            node_filter_, sizeof(node_filter_),
            [this, &reg, &names](int i)
            {
                const auto *nt = find_node_type(reg, names[i]);
                if (!nt)
                {
                    ImGui::TextDisabled("(not found in registry)");
                    return;
                }

                const auto *editor_info =
                    manager_->node_editor_info(names[i]);

                text_unformatted(nt->name);
                ImGui::Separator();

                if (editor_info && !editor_info->display_name.empty())
                {
                    ImGui::TextDisabled("Display: %s",
                                        editor_info->display_name.c_str());
                }
                ImGui::Text("Kind:     %s", kind_name(nt->kind));

                ImGui::TextUnformatted("Category: ");
                ImGui::SameLine();
                text_unformatted(nt->category);

                // Arguments: name and type. `arg.type.name` is a
                // std::string, so %s is safe.
                ImGui::TextUnformatted("Arguments:");
                if (nt->args.size() == 0)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(none)");
                }
                else
                {
                    for (std::size_t i = 0; i < nt->args.size(); ++i)
                    {
                        const auto &arg = nt->args[i];
                        if (i > 0)
                        {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(", ");
                        }
                        ImGui::SameLine();
                        ImGui::TextUnformatted(arg.name.data(),
                                               arg.name.data() + arg.name.size());
                        ImGui::SameLine();
                        ImGui::TextDisabled(": %s", arg.type.name.c_str());
                    }
                }

                // Result type. Only Value nodes have one; the registry
                // leaves resultType invalid for Action and Trigger.
                if (nt->resultType.isValid())
                {
                    ImGui::Text("Returns: %s", nt->resultType.name.c_str());
                }

                if (editor_info)
                {
                    if (!editor_info->doc_summary.empty())
                    {
                        ImGui::Spacing();
                        ImGui::TextWrapped("%s",
                                           editor_info->doc_summary.c_str());
                    }
                    if (!editor_info->doc_long.empty())
                    {
                        ImGui::Spacing();
                        ImGui::TextWrapped("%s",
                                           editor_info->doc_long.c_str());
                    }

                    if (editor_info->operation != engine::Operation::None)
                    {
                        ImGui::Spacing();
                        if (editor_info->operation == engine::Operation::Custom)
                        {
                            ImGui::Text("Operation: %s",
                                        editor_info->custom_operation.c_str());
                            ImGui::Text("Group:     %s",
                                        editor_info->operation_group_override.c_str());
                        }
                        else
                        {
                            ImGui::Text("Operation: %s",
                                        engine::operation_name(editor_info->operation));
                            ImGui::Text("Group:     %s",
                                        engine::operation_group(editor_info->operation));
                        }
                    }

                    if (!editor_info->example.empty())
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Example:");
                        ImGui::TextWrapped("%s",
                                           editor_info->example.c_str());
                    }
                }
            });
    }

    void PackagePanel::draw_tables_tab()
    {
        const auto *info = current_package();
        if (!info)
            return;

        const auto &names = info->table_names;

        draw_collection(
            "tables",
            names,
            selected_table_,
            table_filter_, sizeof(table_filter_),
            [this, &names](int i)
            {
                const std::string &name = names[i];
                text_unformatted(name);
                ImGui::Separator();

                const auto *editor_info =
                    manager_->table_editor_info(name);
                if (editor_info)
                {
                    if (!editor_info->display_name.empty())
                        ImGui::TextDisabled("Display: %s",
                                            editor_info->display_name.c_str());
                    if (!editor_info->doc_summary.empty())
                        ImGui::TextWrapped("%s",
                                           editor_info->doc_summary.c_str());
                }
                else
                {
                    ImGui::TextDisabled("(no editor info)");
                }
            });
    }

    void PackagePanel::draw_enums_tab()
    {
        const auto *info = current_package();
        if (!info)
            return;

        const auto &reg = manager_->registry();
        const auto &names = info->enum_names;

        draw_collection(
            "enums",
            names,
            selected_enum_,
            enum_filter_, sizeof(enum_filter_),
            [&reg, &names](int i)
            {
                const auto *e = reg.findEnum(names[i]);
                if (!e)
                {
                    ImGui::TextDisabled("(not found in registry)");
                    return;
                }

                text_unformatted(e->name);
                ImGui::Separator();
                ImGui::Text("Members:");

                for (const auto &m : e->members)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(m.name.data(),
                                           m.name.data() + m.name.size());
                    ImGui::SameLine();
                    ImGui::TextDisabled("= %lld",
                                        static_cast<long long>(m.value));
                }
            });
    }

    void PackagePanel::draw_handles_tab()
    {
        const auto *info = current_package();
        if (!info)
            return;

        const auto &reg = manager_->registry();
        const auto &names = info->handle_names;

        draw_collection(
            "handles",
            names,
            selected_handle_,
            handle_filter_, sizeof(handle_filter_),
            [&reg, &names](int i)
            {
                const auto *h = reg.findHandle(names[i]);
                if (!h)
                {
                    ImGui::TextDisabled("(not found in registry)");
                    return;
                }

                text_unformatted(h->name);
                ImGui::Separator();
                ImGui::TextDisabled("(no further detail)");
            });
    }

    void PackagePanel::draw_structs_tab()
    {
        const auto *info = current_package();
        if (!info)
            return;

        const auto &reg = manager_->registry();
        const auto &names = info->struct_names;

        draw_collection(
            "structs",
            names,
            selected_struct_,
            struct_filter_, sizeof(struct_filter_),
            [&reg, &names](int i)
            {
                const auto *s = reg.findStruct(names[i]);
                if (!s)
                {
                    ImGui::TextDisabled("(not found in registry)");
                    return;
                }

                text_unformatted(s->name);
                ImGui::Separator();
                ImGui::Text("Fields:");

                for (const auto &f : s->fields)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(f.name.data(),
                                           f.name.data() + f.name.size());
                }
            });
    }

    void PackagePanel::draw_tabs()
    {
        const auto *info = current_package();
        if (!info)
            return;

        if (!ImGui::BeginTabBar("##collections",
                                ImGuiTabBarFlags_Reorderable |
                                    ImGuiTabBarFlags_AutoSelectNewTabs))
            return;

        // A tab is shown only if this package registered at least
        // one item of that kind.
        if (info->node_type_count > 0)
        {
            if (ImGui::BeginTabItem("Node Types"))
            {
                draw_node_types_tab();
                ImGui::EndTabItem();
            }
        }

        if (info->table_count > 0)
        {
            if (ImGui::BeginTabItem("Tables"))
            {
                draw_tables_tab();
                ImGui::EndTabItem();
            }
        }

        if (info->enum_count > 0)
        {
            if (ImGui::BeginTabItem("Enums"))
            {
                draw_enums_tab();
                ImGui::EndTabItem();
            }
        }

        if (info->handle_count > 0)
        {
            if (ImGui::BeginTabItem("Handles"))
            {
                draw_handles_tab();
                ImGui::EndTabItem();
            }
        }

        if (info->struct_count > 0)
        {
            if (ImGui::BeginTabItem("Structs"))
            {
                draw_structs_tab();
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    void PackagePanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Package", &visible_))
        {
            ImGui::End();
            return;
        }

        if (!manager_)
        {
            ImGui::TextDisabled("(no package manager)");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("##left", ImVec2(280, 0), ImGuiChildFlags_Borders);
        draw_package_list();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##right", ImVec2(0, 0), ImGuiChildFlags_Borders);
        draw_package_metadata();
        ImGui::Separator();
        draw_tabs();
        ImGui::EndChild();

        ImGui::End();
    }

} // namespace editor