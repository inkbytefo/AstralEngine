#include <astral/render/text.hpp>

#include <algorithm>
#include <cmath>

#define STBTT_STATIC // private copy: no clash with other stb_truetype users (e.g. Dear ImGui)
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace astral {

struct Font::Info {
    stbtt_fontinfo font{};
    float scale = 1.0f;
};

namespace {
std::uint8_t to_byte(float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }
} // namespace

std::uint32_t pack_color(Vec4 c) noexcept {
    return std::uint32_t{to_byte(c.x)} | (std::uint32_t{to_byte(c.y)} << 8) | (std::uint32_t{to_byte(c.z)} << 16) |
           (std::uint32_t{to_byte(c.w)} << 24);
}

char32_t next_codepoint(std::string_view s, std::size_t& at) noexcept {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(s[i]); };
    const unsigned char b0 = byte(at);
    int extra = b0 < 0x80 ? 0 : (b0 >> 5) == 0x6 ? 1 : (b0 >> 4) == 0xE ? 2 : (b0 >> 3) == 0x1E ? 3 : -1;
    if (extra < 0 || at + static_cast<std::size_t>(extra) >= s.size() + (extra == 0 ? 1 : 0)) {
        ++at;
        return 0xFFFD;
    }
    char32_t cp = extra == 0 ? b0 : extra == 1 ? (b0 & 0x1Fu) : extra == 2 ? (b0 & 0x0Fu) : (b0 & 0x07u);
    for (int i = 1; i <= extra; ++i) {
        const unsigned char b = byte(at + static_cast<std::size_t>(i));
        if ((b >> 6) != 0x2) {
            ++at;
            return 0xFFFD;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    at += static_cast<std::size_t>(extra) + 1;
    return cp;
}

std::unique_ptr<Font> Font::create(std::span<const std::uint8_t> ttf, const FontDesc& desc, std::string* error) {
    auto font = std::unique_ptr<Font>(new Font());
    font->ttf_.assign(ttf.begin(), ttf.end());
    font->info_ = std::make_unique<Info>();
    const int offset = font->ttf_.empty() ? -1 : stbtt_GetFontOffsetForIndex(font->ttf_.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&font->info_->font, font->ttf_.data(), offset)) {
        if (error) *error = "Font: not a TrueType/OpenType font";
        return nullptr;
    }
    stbtt_fontinfo& info = font->info_->font;
    font->glyph_size_ = desc.glyph_size;
    font->padding_ = static_cast<float>(desc.padding);
    const float scale = stbtt_ScaleForPixelHeight(&info, desc.glyph_size);
    font->info_->scale = scale;
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    font->ascent_ = static_cast<float>(ascent) * scale;
    font->line_gap_ = static_cast<float>(ascent - descent + gap) * scale;

    // Rasterise every glyph's distance field, then shelf-pack them into one atlas.
    struct Bitmap {
        char32_t cp;
        int w, h;
        std::vector<std::uint8_t> pixels;
    };
    std::vector<Bitmap> bitmaps;
    for (const auto& [first, last] : desc.ranges) {
        for (char32_t cp = first; cp <= last; ++cp) {
            if (stbtt_FindGlyphIndex(&info, static_cast<int>(cp)) == 0 && cp != U' ') continue;
            int advance = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&info, static_cast<int>(cp), &advance, &lsb);
            Glyph g;
            g.advance = static_cast<float>(advance) * scale;
            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* sdf = stbtt_GetCodepointSDF(&info, scale, static_cast<int>(cp), desc.padding, 128,
                                                       128.0f / static_cast<float>(desc.padding), &w, &h, &xoff, &yoff);
            if (sdf) {
                g.size = {static_cast<float>(w), static_cast<float>(h)};
                g.offset = {static_cast<float>(xoff), static_cast<float>(yoff)};
                bitmaps.push_back({cp, w, h, std::vector<std::uint8_t>(sdf, sdf + w * h)});
                stbtt_FreeSDF(sdf, nullptr);
            }
            font->glyphs_[cp] = g;
        }
    }
    std::sort(bitmaps.begin(), bitmaps.end(), [](const Bitmap& a, const Bitmap& b) { return a.h > b.h; });
    const int width = 1024;
    int x = 1, y = 1, shelf = 0;
    struct Placement {
        int x, y;
    };
    std::vector<Placement> places;
    for (const Bitmap& b : bitmaps) {
        if (x + b.w + 1 > width) {
            x = 1;
            y += shelf + 1;
            shelf = 0;
        }
        places.push_back({x, y});
        x += b.w + 1;
        shelf = std::max(shelf, b.h);
    }
    int height = 1;
    while (height < y + shelf + 1) height *= 2;
    ImageData& atlas = font->atlas_;
    atlas = {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .rgba = {}, .srgb = false,
             .format = ImageFormat::RGBA8, .levels = {}};
    atlas.rgba.assign(std::size_t{atlas.width} * atlas.height * 4, 0);
    for (std::size_t i = 0; i < atlas.rgba.size(); i += 4) atlas.rgba[i] = atlas.rgba[i + 1] = atlas.rgba[i + 2] = 255;
    for (std::size_t i = 0; i < bitmaps.size(); ++i) {
        const Bitmap& b = bitmaps[i];
        for (int row = 0; row < b.h; ++row) {
            for (int col = 0; col < b.w; ++col) {
                atlas.rgba[(static_cast<std::size_t>(places[i].y + row) * atlas.width + static_cast<std::size_t>(places[i].x + col)) * 4 + 3] =
                    b.pixels[static_cast<std::size_t>(row * b.w + col)];
            }
        }
        Glyph& g = font->glyphs_[b.cp];
        g.uv = {static_cast<float>(places[i].x) / static_cast<float>(width), static_cast<float>(places[i].y) / static_cast<float>(height),
                static_cast<float>(places[i].x + b.w) / static_cast<float>(width),
                static_cast<float>(places[i].y + b.h) / static_cast<float>(height)};
    }
    return font;
}

Font::~Font() {
    if (renderer_) renderer_->destroy(texture_);
}

void Font::upload(Renderer& renderer) {
    if (renderer_ == &renderer && texture_.valid() && renderer.update_texture(texture_, atlas_)) return;
    if (renderer_) renderer_->destroy(texture_);
    renderer_ = &renderer;
    texture_ = renderer.create_texture(atlas_);
}

const Glyph* Font::glyph(char32_t cp) const noexcept {
    auto it = glyphs_.find(cp);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float Font::kerning(char32_t a, char32_t b, float size) const {
    return static_cast<float>(stbtt_GetCodepointKernAdvance(&info_->font, static_cast<int>(a), static_cast<int>(b))) *
           info_->scale * size / glyph_size_;
}

Vec2 Font::measure(std::string_view utf8, float size) const {
    const float s = size / glyph_size_;
    float width = 0, line = 0;
    int lines = 1;
    char32_t prev = 0;
    for (std::size_t at = 0; at < utf8.size();) {
        const char32_t cp = next_codepoint(utf8, at);
        if (cp == U'\n') {
            width = std::max(width, line);
            line = 0;
            ++lines;
            prev = 0;
            continue;
        }
        const Glyph* g = glyph(cp);
        if (!g) continue;
        if (prev) line += kerning(prev, cp, size);
        line += g->advance * s;
        prev = cp;
    }
    return {std::max(width, line), line_height(size) * static_cast<float>(lines)};
}

void UiCanvas::quad(Vec2 min, Vec2 max, Vec4 uv, std::uint32_t color, TextureHandle texture, float sdf) {
    const auto base = static_cast<std::uint32_t>(vertices_.size());
    vertices_.push_back({{min.x, min.y}, {uv.x, uv.y}, color});
    vertices_.push_back({{max.x, min.y}, {uv.z, uv.y}, color});
    vertices_.push_back({{max.x, max.y}, {uv.z, uv.w}, color});
    vertices_.push_back({{min.x, max.y}, {uv.x, uv.w}, color});
    const auto first = static_cast<std::uint32_t>(indices_.size());
    for (std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) indices_.push_back(base + i);
    if (!commands_.empty()) {
        UiCommand& last = commands_.back();
        if (last.texture == texture && last.sdf_softness == sdf && last.first_index + last.index_count == first) {
            last.index_count += 6; // same state: one draw
            return;
        }
    }
    commands_.push_back({.first_index = first, .index_count = 6, .vertex_offset = 0, .texture = texture, .sdf_softness = sdf});
}

void UiCanvas::rect(Vec2 min, Vec2 max, Vec4 color) { quad(min, max, {0, 0, 1, 1}, pack_color(color), {}, 0.0f); }

void UiCanvas::frame(Vec2 min, Vec2 max, Vec4 color, float t) {
    rect(min, {max.x, min.y + t}, color);
    rect({min.x, max.y - t}, max, color);
    rect({min.x, min.y + t}, {min.x + t, max.y - t}, color);
    rect({max.x - t, min.y + t}, {max.x, max.y - t}, color);
}

void UiCanvas::image(TextureHandle texture, Vec2 min, Vec2 max, Vec4 tint, Vec4 uv) {
    quad(min, max, uv, pack_color(tint), texture, 0.0f);
}

Vec2 UiCanvas::text(const Font& font, std::string_view utf8, Vec2 position, const TextStyle& style) {
    const Vec2 size = font.measure(utf8, style.size);
    if (style.shadow) {
        TextStyle shadow = style;
        shadow.shadow = false;
        shadow.color = {0, 0, 0, 0.65f * style.color.w};
        shadow.softness = 0.12f;
        const float d = std::max(1.0f, style.size * 0.06f);
        text(font, utf8, {position.x + d, position.y + d}, shadow);
    }
    const float s = style.size / font.glyph_size();
    const std::uint32_t color = pack_color(style.color);
    const float softness = std::max(style.softness, 0.001f); // > 0 marks the command as distance-field text
    float y = position.y;
    for (std::size_t line_start = 0; line_start <= utf8.size();) {
        std::size_t line_end = utf8.find('\n', line_start);
        if (line_end == std::string_view::npos) line_end = utf8.size();
        const std::string_view line = utf8.substr(line_start, line_end - line_start);
        const float width = font.measure(line, style.size).x;
        float x = style.align == TextAlign::Left ? position.x : style.align == TextAlign::Center ? position.x - width * 0.5f : position.x - width;
        const float baseline = y + font.ascent(style.size);
        char32_t prev = 0;
        for (std::size_t at = 0; at < line.size();) {
            const char32_t cp = next_codepoint(line, at);
            const Glyph* g = font.glyph(cp);
            if (!g) continue;
            if (prev) x += font.kerning(prev, cp, style.size);
            if (g->size.x > 0) {
                const Vec2 min{x + g->offset.x * s, baseline + g->offset.y * s};
                quad(min, {min.x + g->size.x * s, min.y + g->size.y * s}, g->uv, color, font.texture(), softness);
            }
            x += g->advance * s;
            prev = cp;
        }
        y += font.line_height(style.size);
        if (line_end == utf8.size()) break;
        line_start = line_end + 1;
    }
    return size;
}

void UiCanvas::clear() noexcept {
    vertices_.clear();
    indices_.clear();
    commands_.clear();
}

} // namespace astral
