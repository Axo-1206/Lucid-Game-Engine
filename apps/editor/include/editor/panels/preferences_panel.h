// editor/include/editor/panels/preferences_panel.h

#pragma once

#include "editor/settings.h"

#include <functional>
#include <string>
#include <vector>

namespace editor
{

    // The Preferences modal. Edits a copy of Settings; Save (via
    // apply()) dispatches the copy to the callback; Cancel
    // discards. The panel does not own settings_ or the project
    // roots — those live in EditorApp. The panel is a transient
    // editor of Settings.
    //
    // Fields:
    //   - Theme: shown disabled (themes deferred, Decision 42).
    //   - Font size: shown disabled (fonts deferred, Decision 42).
    //   - Show Console on start: honored at next launch.
    //   - Default new-project parent: the folder New Project opens
    //     in.
    //   - Project roots: extra folders the launch panel scans.
    //
    // apply() is public because it is the Save action. A test
    // calls the same method the button does; there is no
    // test-only hook that could drift from the real path.
    class PreferencesPanel
    {
    public:
        using SaveCallback =
            std::function<void(const Settings &new_settings)>;

        void set_save_callback(SaveCallback cb) { on_save_ = std::move(cb); }

        // Open the modal with `current` as the starting state. The
        // panel copies it into its working state; edits go to the
        // copy until apply().
        void open(const Settings &current);

        // True while the modal is visible.
        bool is_open() const { return open_; }

        // Draw the modal. Called once per frame by EditorApp.
        void draw();

        // Apply the current edits: fold the roots list into the
        // working settings, dispatch it to the save callback, and
        // close. The Save button calls this; a test calls it
        // directly. One code path.
        void apply();

        // Close without saving. The X button and Escape call this.
        void close();

    private:
        SaveCallback on_save_;

        bool open_ = false;
        bool pending_open_ = false;
        Settings working_;
        std::vector<std::string> roots_;
        char new_root_buf_[512] = {};

        void draw_theme_section();
        void draw_default_parent_section();
        void draw_console_section();
        void draw_project_roots_section();
    };

} // namespace editor