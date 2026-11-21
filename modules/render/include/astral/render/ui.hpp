#pragma once
// 2D overlay geometry in pixels: what the debug UI, HUD text and sprites are drawn with.
// Triangles indexed per command; each command has a texture (or none = white) and a clip rect.
// Colours are 8-bit sRGB (like UI tools produce); the renderer blends them correctly on sRGB targets.

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>

#include <cstdint>
#include <span>

namespace astral {

struct RenderTextureTag;
using TextureHandle = Handle<RenderTextureTag>;

struct UiVertex {
    Vec2 position{};            ///< Pixels, origin top-left.
    Vec2 uv{};
    std::uint32_t color = 0xFFFFFFFFu; ///< RGBA8, R in the lowest byte.
};

struct UiCommand {
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    std::int32_t vertex_offset = 0;   ///< Added to every index.
    TextureHandle texture{};          ///< Invalid: plain colour.
    Vec4 clip{0, 0, 1e9f, 1e9f};      ///< x0, y0, x1, y1 in pixels.
    /// > 0: the texture's alpha is a signed distance (0.5 = edge) and this is the edge softness in
    /// distance units — sharp text at any size (HUD fonts). 0: ordinary texture.
    float sdf_softness = 0.0f;
};

/// One batch of 2D geometry. Layers draw in order, after the 3D image and debug lines.
struct UiLayer {
    std::span<const UiVertex> vertices = {};
    std::span<const std::uint32_t> indices = {};
    std::span<const UiCommand> commands = {};
};

} // namespace astral
