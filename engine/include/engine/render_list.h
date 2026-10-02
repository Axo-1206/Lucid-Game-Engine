#pragma once

#include "engine/render_types.h"
#include "engine/texture.h"

#include <cstdint>
#include <vector>

namespace engine
{

    class Renderer;

    // A single item to draw in the current frame.
    struct RenderItem
    {
        enum class Type : std::uint8_t
        {
            Rect,
            Sprite,
            Circle,
            Line,
        };

        Type type = Type::Rect;
        TextureHandle texture{};
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float rotation = 0.0f;
        float thickness = 1.0f;
        float radius = 0.0f;
        Color color = Color::white();
        int layer = 0;
        float anchor_x = 0.5f;
        float anchor_y = 0.5f;
    };

    // A per-frame list of draw items.
    class RenderList
    {
    public:
        void clear();

        void add_rect(float x, float y, float w, float h,
                      Color color, int layer);

        void add_sprite(TextureHandle tex,
                        float x, float y, float w, float h,
                        float rotation, Color tint,
                        float anchor_x, float anchor_y,
                        int layer);

        void add_circle(float cx, float cy, float radius,
                        Color color, int layer);

        void add_line(float x1, float y1, float x2, float y2,
                      float thickness, Color color, int layer);

        void sort();
        void submit(Renderer &renderer) const;

        std::size_t size() const { return items_.size(); }
        const std::vector<RenderItem> &items() const { return items_; }

    private:
        std::vector<RenderItem> items_;
    };

} // namespace engine
