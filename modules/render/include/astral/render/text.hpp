#pragma once
// Text and simple 2D drawing for HUDs, on the 2D overlay (UiLayer).
//
//     Font font = *Font::create(default_font_ttf());   // signed-distance glyphs: sharp at any size
//     font.upload(renderer);
//     UiCanvas hud;
//     hud.rect({20, 20}, {220, 64}, {0, 0, 0, 0.5f});
//     hud.text(font, "Skor: 3 : 1", {32, 30}, {.size = 28, .color = {1, 1, 1, 1}, .shadow = true});
//     renderer.render(..., {.ui = std::span(&layer = hud.layer(), 1)});
//
// Positions are pixels of the render target, origin top-left. Colours are sRGB, as picked in any
// paint tool (the renderer blends correctly on sRGB targets).

#include <astral/render/renderer.hpp>
#include <astral/render/ui.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astral {

/// The engine's built-in font (Roboto Medium, Apache License 2.0), as TrueType bytes. Always available:
/// the debug UI and HUD text use it when a game brings no font of its own.
std::span<const std::uint8_t> default_font_ttf() noexcept;

struct FontDesc {
    float glyph_size = 48.0f;    ///< Pixel size glyphs are rasterised at (the field scales up and down from it).
    int padding = 6;             ///< Distance field spread in pixels around each glyph.
    std::vector<std::pair<char32_t, char32_t>> ranges = {
        {0x20, 0x7E},   // ASCII
        {0xA0, 0xFF},   // Latin-1
        {0x100, 0x17F}, // Latin Extended-A (Turkish ğ ş ı İ ...)
        {0x2013, 0x2026}, // dashes, quotes, bullet, ellipsis
        {0x20AC, 0x20BA}, // € and ₺ (and the currencies between)
    };
};

struct Glyph {
    Vec2 size{};    ///< Quad size at glyph_size, pixels (padding included).
    Vec2 offset{};  ///< Quad top-left relative to the pen on the baseline.
    Vec4 uv{};      ///< u0, v0, u1, v1 in the atlas.
    float advance = 0.0f;
};

/// A signed-distance-field font: glyph metrics plus one atlas texture.
class Font {
public:
    /// Null if the bytes are not a TrueType/OpenType font.
    static std::unique_ptr<Font> create(std::span<const std::uint8_t> ttf, const FontDesc& desc = {}, std::string* error = nullptr);
    ~Font();

    /// Creates (or recreates) the atlas texture on `renderer`. Layout works without it (headless).
    void upload(Renderer& renderer);
    [[nodiscard]] TextureHandle texture() const noexcept { return texture_; }
    [[nodiscard]] const ImageData& atlas() const noexcept { return atlas_; }

    [[nodiscard]] const Glyph* glyph(char32_t codepoint) const noexcept;
    /// Width and height of a (single or multi-line) string at `size` pixels.
    [[nodiscard]] Vec2 measure(std::string_view utf8, float size) const;
    [[nodiscard]] float line_height(float size) const noexcept { return line_gap_ * size / glyph_size_; }
    [[nodiscard]] float ascent(float size) const noexcept { return ascent_ * size / glyph_size_; }
    [[nodiscard]] float kerning(char32_t a, char32_t b, float size) const;
    [[nodiscard]] float glyph_size() const noexcept { return glyph_size_; }
    [[nodiscard]] float padding() const noexcept { return padding_; }

private:
    Font() = default;
    struct Info;
    std::unique_ptr<Info> info_; ///< The parsed TrueType file (for kerning).
    std::vector<std::uint8_t> ttf_;
    std::unordered_map<char32_t, Glyph> glyphs_;
    ImageData atlas_;
    TextureHandle texture_{};
    Renderer* renderer_ = nullptr;
    float glyph_size_ = 48.0f;
    float padding_ = 6.0f;
    float ascent_ = 0.0f;
    float line_gap_ = 0.0f;
};

enum class TextAlign : std::uint8_t { Left, Center, Right };

struct TextStyle {
    float size = 24.0f;            ///< Pixels (cap-to-descender height of the font at this size).
    Vec4 color{1, 1, 1, 1};
    TextAlign align = TextAlign::Left;
    bool shadow = false;           ///< A soft dark copy behind (readable on any background).
    float softness = 0.0f;         ///< Edge blur in distance units (0 = crisp).
};

/// Collects 2D shapes and text for one frame; layer() hands them to the renderer.
class UiCanvas {
public:
    void rect(Vec2 min, Vec2 max, Vec4 color);
    /// Outline of a rectangle, `thickness` pixels inside it.
    void frame(Vec2 min, Vec2 max, Vec4 color, float thickness = 1.0f);
    void image(TextureHandle texture, Vec2 min, Vec2 max, Vec4 tint = {1, 1, 1, 1}, Vec4 uv = {0, 0, 1, 1});
    /// Draws `utf8` with its top-left (or top-centre / top-right, by `align`) at `position`; returns its size.
    /// '\n' starts a new line. Characters the font lacks are skipped.
    Vec2 text(const Font& font, std::string_view utf8, Vec2 position, const TextStyle& style = {});

    /// Everything drawn since the last clear().
    [[nodiscard]] UiLayer layer() const noexcept { return {.vertices = vertices_, .indices = indices_, .commands = commands_}; }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }
    void clear() noexcept;

private:
    void quad(Vec2 min, Vec2 max, Vec4 uv, std::uint32_t color, TextureHandle texture, float sdf);
    std::vector<UiVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<UiCommand> commands_;
};

/// Packs an sRGB colour (0..1 per channel) into UiVertex::color.
std::uint32_t pack_color(Vec4 color) noexcept;
/// Next code point of UTF-8 text (advances `at`); U+FFFD for invalid bytes.
char32_t next_codepoint(std::string_view utf8, std::size_t& at) noexcept;

} // namespace astral
