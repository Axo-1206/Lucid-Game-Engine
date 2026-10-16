#pragma once

#include "engine/registry/package_manager.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace editor
{

    // The Package panel: a read-only view of the engine's loaded
    // packages and their registered content.
    //
    // Layout:
    //   - Left column: the package list.
    //   - Right top: metadata for the selected package.
    //   - Right middle: a tab bar with one tab per collection
    //     (Node Types, Tables, Enums, Handles, Structs). A tab is
    //     shown only if the selected package registered at least
    //     one item of that kind.
    //   - Right main: two columns. Filterable list on the left,
    //     detail of the selected item on the right.
    //
    // The list in each tab is the *selected package's* items, not
    // the engine's global list. Every item name comes from
    // PackageInfo; the full detail comes from the registry.
    //
    // The panel does not modify anything. Engine packages are
    // immutable from the editor.
    class PackagePanel
    {
    public:
        void set_manager(const engine::PackageManager *manager);
        void clear();
        void draw();
        bool &visible() { return visible_; }

    private:
        const engine::PackageManager *manager_ = nullptr;
        int selected_package_ = 0;
        bool visible_ = true;

        // Per-collection selection and filter state.
        int selected_node_ = -1;
        int selected_table_ = -1;
        int selected_enum_ = -1;
        int selected_handle_ = -1;
        int selected_struct_ = -1;

        char node_filter_[128] = {};
        char table_filter_[128] = {};
        char enum_filter_[128] = {};
        char handle_filter_[128] = {};
        char struct_filter_[128] = {};

        // The selected package, or nullptr if none / out of range.
        const engine::PackageInfo *current_package() const;

        void draw_package_list();
        void draw_package_metadata();
        void draw_tabs();

        void draw_node_types_tab();
        void draw_tables_tab();
        void draw_enums_tab();
        void draw_handles_tab();
        void draw_structs_tab();

        // Draw one collection: filter box + list on the left,
        // detail on the right. The `draw_detail` callback receives
        // the index of the selected row.
        //
        // `names` must outlive the call (the callback captures a
        // reference to it). Pass a reference, never a temporary.
        void draw_collection(
            const char *id,
            const std::vector<std::string> &names,
            int &selected,
            char *filter,
            std::size_t filter_size,
            const std::function<void(int)> &draw_detail);
    };

} // namespace editor