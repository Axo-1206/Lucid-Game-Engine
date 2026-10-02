#include "engine/render_list.h"
#include "engine/renderer.h"

#include <algorithm>

namespace engine
{

    void RenderList::clear()
    {
        items_.clear();
    }

    void RenderList::add_rect(float x, float y, float w, float h,
                              Color color, int layer)
    {
        RenderItem item;
        item.type = RenderItem::Type::Rect;
        item.x = x;
        item.y = y;
        item.w = w;
        item.h = h;
        item.color = color;
        item.layer = layer;
        items_.push_back(item);
    }

    void RenderList::add_sprite(TextureHandle tex,
                                float x, float y, float w, float h,
                                float rotation, Color tint,
                                float anchor_x, float anchor_y,
                                int layer)
    {
        RenderItem item;
        item.type = RenderItem::Type::Sprite;
        item.texture = tex;
        item.x = x;
        item.y = y;
        item.w = w;
        item.h = h;
        item.rotation = rotation;
        item.color = tint;
        item.layer = layer;
        item.anchor_x = anchor_x;
        item.anchor_y = anchor_y;
        items_.push_back(item);
    }

    void RenderList::add_circle(float cx, float cy, float radius,
                                Color color, int layer)
    {
        RenderItem item;
        item.type = RenderItem::Type::Circle;
        item.x = cx;
        item.y = cy;
        item.radius = radius;
        item.color = color;
        item.layer = layer;
        items_.push_back(item);
    }

    void RenderList::add_line(float x1, float y1, float x2, float y2,
                              float thickness, Color color, int layer)
    {
        RenderItem item;
        item.type = RenderItem::Type::Line;
        item.x = x1;
        item.y = y1;
        item.w = x2;
        item.h = y2;
        item.thickness = thickness;
        item.color = color;
        item.layer = layer;
        items_.push_back(item);
    }

    void RenderList::sort()
    {
        std::stable_sort(items_.begin(), items_.end(),
                         [](const RenderItem &a, const RenderItem &b)
                         {
                             if (a.layer != b.layer)
                                 return a.layer < b.layer;
                             if (a.type != b.type)
                                 return static_cast<int>(a.type) < static_cast<int>(b.type);
                             return a.texture.id < b.texture.id;
                         });
    }

    void RenderList::submit(Renderer &renderer) const
    {
        for (const auto &item : items_)
        {
            switch (item.type)
            {
            case RenderItem::Type::Rect:
                renderer.draw_rect(item.x, item.y, item.w, item.h,
                                   item.color, item.layer);
                break;
            case RenderItem::Type::Sprite:
                renderer.draw_sprite(item.texture,
                                     item.x, item.y, item.w, item.h,
                                     item.rotation, item.color,
                                     item.anchor_x, item.anchor_y,
                                     item.layer);
                break;
            case RenderItem::Type::Circle:
                renderer.draw_circle(item.x, item.y, item.radius,
                                     item.color, item.layer);
                break;
            case RenderItem::Type::Line:
                renderer.draw_line(item.x, item.y, item.w, item.h,
                                   item.thickness, item.color, item.layer);
                break;
            }
        }
    }

} // namespace engine
