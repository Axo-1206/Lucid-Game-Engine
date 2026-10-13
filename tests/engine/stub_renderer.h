#pragma once

#include "engine/render/renderer.h"
#include "engine/render/render_list.h"
#include "engine/platform/input.h"

#include <array>
#include <string>
#include <vector>

namespace engine::tests
{

    // A Renderer that records calls. Used by view and UI tests to
    // assert on the sequence of renderer operations without a GL
    // context.
    struct StubRenderer : engine::Renderer
    {
        std::vector<Camera2D> cameras;
        std::vector<std::array<int, 4>> viewports;
        std::vector<std::string> passes;
        std::vector<RenderItem> submitted_items;

        int window_w = 1280;
        int window_h = 720;

        bool init(void *) override { return true; }
        void shutdown() override {}
        void begin_frame(Color) override { passes.push_back("frame_begin"); }
        void end_frame() override { passes.push_back("frame_end"); }
        void present() override {}

        void begin_world_2d() override { passes.push_back("world_begin"); }
        void end_world_2d() override { passes.push_back("world_end"); }
        void begin_ui() override { passes.push_back("ui_begin"); }
        void end_ui() override { passes.push_back("ui_end"); }

        void set_viewport(int x, int y, int w, int h) override
        {
            viewports.push_back({x, y, w, h});
        }
        void set_full_viewport() override
        {
            viewports.push_back({0, 0, window_w, window_h});
        }
        int viewport_x() const override { return 0; }
        int viewport_y() const override { return 0; }
        int viewport_width() const override { return 0; }
        int viewport_height() const override { return 0; }

        void set_camera_2d(const Camera2D &camera) override
        {
            cameras.push_back(camera);
        }
        Camera2D get_camera_2d() const override { return {}; }

        TextureHandle load_texture(const std::string &) override { return {}; }
        void free_texture(TextureHandle) override {}
        TextureHandle white_texture() const override { return {}; }

        void draw_rect(float x, float y, float w, float h, Color c, int layer) override
        {
            RenderItem item;
            item.type = RenderItem::Type::Rect;
            item.x = x;
            item.y = y;
            item.w = w;
            item.h = h;
            item.color = c;
            item.layer = layer;
            submitted_items.push_back(item);
        }
        void draw_rect_outline(float, float, float, float, float, Color, int) override {}
        void draw_circle(float, float, float, Color, int, int) override {}
        void draw_line(float, float, float, float, float, Color, int) override {}
        void draw_sprite(TextureHandle, float, float, float, float, float, Color, float, float, int) override {}

        RenderStats stats() const override { return {}; }
        void reset_stats() override {}
    };

    struct StubInput : engine::Input
    {
        void poll() override {}
    };

} // namespace engine::tests