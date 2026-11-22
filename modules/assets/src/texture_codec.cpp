#include <astral/assets/cook.hpp>
#include <astral/render/renderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#include "bc7decomp.h"
#include "bc7enc.h"
#define RGBCX_IMPLEMENTATION
#include "rgbcx.h"

namespace astral {

namespace {

void init_codecs() {
    static std::once_flag once;
    std::call_once(once, [] {
        bc7enc_compress_block_init();
        rgbcx::init();
    });
}

std::size_t block_bytes(std::uint32_t w, std::uint32_t h) { return std::size_t{(w + 3) / 4} * ((h + 3) / 4) * 16; }

/// 4x4 texels at block (bx, by), edges repeated past the image.
void gather(const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h, std::uint32_t bx, std::uint32_t by,
            std::uint8_t out[64]) {
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const std::uint32_t sx = std::min(w - 1, bx * 4 + x), sy = std::min(h - 1, by * 4 + y);
            std::memcpy(out + (y * 4 + x) * 4, &rgba[(std::size_t{sy} * w + sx) * 4], 4);
        }
    }
}

/// Mips of a normal map are averaged vectors: renormalise them.
void renormalize(std::vector<std::uint8_t>& rgba) {
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        float v[3];
        for (int c = 0; c < 3; ++c) v[c] = rgba[i + static_cast<std::size_t>(c)] / 127.5f - 1.0f;
        const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (len < 1e-4f) continue;
        for (int c = 0; c < 3; ++c) {
            rgba[i + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(std::clamp((v[c] / len + 1.0f) * 127.5f + 0.5f, 0.0f, 255.0f));
        }
    }
}

} // namespace

ImageData cook_image(const ImageData& image, TextureUse use, const TextureCookOptions& options) {
    ImageData src = image;
    if (!image.levels.empty()) {
        if (image.format != ImageFormat::RGBA8) return image; // already cooked
        src.rgba = image.levels[0];
        src.levels.clear();
    }
    src.srgb = use == TextureUse::Color;
    std::vector<std::vector<std::uint8_t>> mips = build_mips(src);
    if (use == TextureUse::Normal) {
        for (std::size_t l = 1; l < mips.size(); ++l) renormalize(mips[l]);
    }
    ImageData out{.width = image.width, .height = image.height, .rgba = {}, .srgb = src.srgb,
                  .format = ImageFormat::RGBA8, .levels = {}};
    if (!options.compress) {
        out.levels = std::move(mips);
        return out;
    }

    init_codecs();
    out.format = use == TextureUse::Normal ? ImageFormat::BC5 : ImageFormat::BC7;
    bc7enc_compress_block_params params;
    bc7enc_compress_block_params_init(&params);
    params.m_uber_level = std::min<std::uint32_t>(options.bc7_quality, BC7ENC_MAX_UBER_LEVEL);
    params.m_max_partitions_mode = options.bc7_quality == 0 ? 16 : BC7ENC_MAX_PARTITIONS1;
    if (use != TextureUse::Color) bc7enc_compress_block_params_init_linear_weights(&params);
    for (std::size_t l = 0; l < mips.size(); ++l) {
        const std::uint32_t w = std::max(1u, image.width >> l), h = std::max(1u, image.height >> l);
        std::vector<std::uint8_t> level(block_bytes(w, h));
        std::uint8_t pixels[64];
        std::size_t at = 0;
        for (std::uint32_t by = 0; by < (h + 3) / 4; ++by) {
            for (std::uint32_t bx = 0; bx < (w + 3) / 4; ++bx, at += 16) {
                gather(mips[l], w, h, bx, by, pixels);
                if (out.format == ImageFormat::BC5) rgbcx::encode_bc5(&level[at], pixels, 0, 1, 4);
                else bc7enc_compress_block(&level[at], pixels, &params);
            }
        }
        out.levels.push_back(std::move(level));
    }
    return out;
}

ImageData decompress_image(const ImageData& image) {
    if (image.levels.empty() || image.format == ImageFormat::RGBA8) return image;
    ImageData out{.width = image.width, .height = image.height, .rgba = {}, .srgb = image.srgb,
                  .format = ImageFormat::RGBA8, .levels = {}};
    for (std::size_t l = 0; l < image.levels.size(); ++l) {
        const std::uint32_t w = std::max(1u, image.width >> l), h = std::max(1u, image.height >> l);
        const std::vector<std::uint8_t>& src = image.levels[l];
        std::vector<std::uint8_t> level(std::size_t{w} * h * 4);
        if (src.size() != block_bytes(w, h)) return {};
        std::size_t at = 0;
        for (std::uint32_t by = 0; by < (h + 3) / 4; ++by) {
            for (std::uint32_t bx = 0; bx < (w + 3) / 4; ++bx, at += 16) {
                std::uint8_t pixels[64];
                if (image.format == ImageFormat::BC5) {
                    std::memset(pixels, 255, sizeof pixels);
                    rgbcx::unpack_bc5(&src[at], pixels, 0, 1, 4);
                    for (int p = 0; p < 16; ++p) { // rebuild Z like the shader does
                        const float x = pixels[p * 4] / 127.5f - 1.0f, y = pixels[p * 4 + 1] / 127.5f - 1.0f;
                        const float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
                        pixels[p * 4 + 2] = static_cast<std::uint8_t>((z + 1.0f) * 127.5f + 0.5f);
                    }
                } else {
                    bc7decomp::unpack_bc7(&src[at], reinterpret_cast<bc7decomp::color_rgba*>(pixels));
                }
                for (std::uint32_t y = 0; y < 4 && by * 4 + y < h; ++y) {
                    for (std::uint32_t x = 0; x < 4 && bx * 4 + x < w; ++x) {
                        std::memcpy(&level[(std::size_t{by * 4 + y} * w + bx * 4 + x) * 4], pixels + (y * 4 + x) * 4, 4);
                    }
                }
            }
        }
        out.levels.push_back(std::move(level));
    }
    return out;
}

void cook_model_images(ModelData& model, const TextureCookOptions& options, JobSystem* jobs) {
    std::vector<std::optional<TextureUse>> uses(model.images.size());
    auto mark = [&](int index, TextureUse use) {
        if (index >= 0 && static_cast<std::size_t>(index) < uses.size() && !uses[static_cast<std::size_t>(index)]) {
            uses[static_cast<std::size_t>(index)] = use;
        }
    };
    for (const ModelMaterial& m : model.materials) {
        mark(m.normal_image, TextureUse::Normal);
        mark(m.base_color_image, TextureUse::Color);
        mark(m.emissive_image, TextureUse::Color);
        mark(m.metallic_roughness_image, TextureUse::Data);
    }
    auto cook_one = [&](std::size_t i) {
        ImageData& img = model.images[i].image;
        if (img.width == 0 || !uses[i]) return; // unused by every material
        img = cook_image(img, *uses[i], options);
    };
    if (jobs) {
        JobCounter counter;
        jobs->parallel_for(counter, model.images.size(), 1, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) cook_one(i);
        });
        jobs->wait(counter);
    } else {
        for (std::size_t i = 0; i < model.images.size(); ++i) cook_one(i);
    }
}

} // namespace astral
