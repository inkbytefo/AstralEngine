#include "stores/texture_store.hpp"

#include <astral/core/jobs.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace astral {

namespace {

float srgb_to_linear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float linear_to_srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f; }

/// What a stored byte means as a filter input: byte / 255, decoded to linear light for colour channels.
struct ChannelTables {
    std::array<float, 256> plain{};
    std::array<float, 256> srgb{};
};
const ChannelTables& channel_tables() {
    static const ChannelTables tables = [] {
        ChannelTables t;
        for (std::size_t i = 0; i < 256; ++i) {
            const float v = static_cast<float>(i) / 255.0f;
            t.plain[i] = v;
            t.srgb[i] = srgb_to_linear(v);
        }
        return t;
    }();
    return tables;
}

std::uint8_t encode_channel(double average, bool srgb) {
    float avg = static_cast<float>(average);
    if (srgb) avg = linear_to_srgb(avg);
    return static_cast<std::uint8_t>(std::clamp(avg, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/// One mip level of an exact 2:1 reduction (both sides even): every output texel is the plain average of a 2x2 block.
void halve_even(const std::vector<std::uint8_t>& src, std::uint32_t w, std::vector<std::uint8_t>& dst, std::uint32_t nw,
                std::uint32_t row_begin, std::uint32_t row_end, bool srgb_image) {
    const ChannelTables& tables = channel_tables();
    for (std::uint32_t y = row_begin; y < row_end; ++y) {
        const std::uint8_t* row0 = &src[std::size_t{2 * y} * w * 4];
        const std::uint8_t* row1 = row0 + std::size_t{w} * 4;
        for (std::uint32_t x = 0; x < nw; ++x) {
            for (std::uint32_t c = 0; c < 4; ++c) {
                const bool colour = srgb_image && c < 3;
                const std::array<float, 256>& table = colour ? tables.srgb : tables.plain;
                const std::size_t i = std::size_t{2 * x} * 4 + c;
                double sum = 0;
                sum += table[row0[i]];
                sum += table[row0[i + 4]];
                sum += table[row1[i]];
                sum += table[row1[i + 4]];
                dst[(std::size_t{y} * nw + x) * 4 + c] = encode_channel(sum / 4.0, colour);
            }
        }
    }
}

/// Any other size: each destination texel averages the source area it covers, weighted by overlap, so odd sizes
/// (3 -> 1) keep their last row/column instead of dropping it.
void reduce_weighted(const std::vector<std::uint8_t>& src, std::uint32_t w, std::uint32_t h, std::vector<std::uint8_t>& dst,
                     std::uint32_t nw, std::uint32_t nh, std::uint32_t row_begin, std::uint32_t row_end, bool srgb_image) {
    const ChannelTables& tables = channel_tables();
    const double fx = static_cast<double>(w) / nw, fy = static_cast<double>(h) / nh;
    for (std::uint32_t y = row_begin; y < row_end; ++y) {
        const double y0 = y * fy, y1 = (y + 1) * fy;
        for (std::uint32_t x = 0; x < nw; ++x) {
            const double x0 = x * fx, x1 = (x + 1) * fx;
            for (std::uint32_t c = 0; c < 4; ++c) {
                const bool colour = srgb_image && c < 3;
                const std::array<float, 256>& table = colour ? tables.srgb : tables.plain;
                double sum = 0, area = 0;
                for (auto sy = static_cast<std::uint32_t>(y0); sy < h && sy < y1; ++sy) {
                    const double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
                    for (auto sx = static_cast<std::uint32_t>(x0); sx < w && sx < x1; ++sx) {
                        const double wx = std::min<double>(sx + 1, x1) - std::max<double>(sx, x0);
                        sum += wx * wy * table[src[(std::size_t{sy} * w + sx) * 4 + c]];
                        area += wx * wy;
                    }
                }
                dst[(std::size_t{y} * nw + x) * 4 + c] = encode_channel(sum / area, colour);
            }
        }
    }
}

} // namespace

/// Box-filtered mip chain (colour images are averaged in linear space, then re-encoded).
std::vector<std::vector<std::uint8_t>> build_mips(const ImageData& image, JobSystem* jobs) {
    std::vector<std::vector<std::uint8_t>> mips{image.rgba};
    std::uint32_t w = image.width, h = image.height;
    while (w > 1 || h > 1) {
        const std::uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
        std::vector<std::uint8_t> dst(std::size_t{nw} * nh * 4);
        const std::vector<std::uint8_t>& src = mips.back();
        const bool even = w == 2 * nw && h == 2 * nh;
        // Output rows are independent: big levels are split over the job system, with the same bytes as a serial pass.
        const auto rows = [&](std::size_t begin, std::size_t end) {
            const auto b = static_cast<std::uint32_t>(begin), e = static_cast<std::uint32_t>(end);
            if (even) halve_even(src, w, dst, nw, b, e, image.srgb);
            else reduce_weighted(src, w, h, dst, nw, nh, b, e, image.srgb);
        };
        constexpr std::size_t kParallelTexels = 128 * 128; // below this the job overhead is not worth it
        if (jobs && std::size_t{nw} * nh >= kParallelTexels) {
            JobCounter counter;
            jobs->parallel_for(counter, nh, 8, rows);
            jobs->wait(counter);
        } else {
            rows(0, nh);
        }
        mips.push_back(std::move(dst));
        w = nw;
        h = nh;
    }
    return mips;
}

TextureStore::TextureStore(gpu::Device& device) : device_(device) {
    white_ = create({.width = 1, .height = 1, .rgba = {255, 255, 255, 255}, .srgb = true});
    flat_normal_ = create({.width = 1, .height = 1, .rgba = {128, 128, 255, 255}, .srgb = false});
}

TextureStore::~TextureStore() {
    textures_.for_each([&](TextureHandle, GpuTexture& t) { device_.destroy(t.texture); });
}

bool TextureStore::supports(ImageFormat format) const noexcept {
    return format == ImageFormat::RGBA8 || device_.supports(gpu::Feature::TextureCompressionBC);
}

gpu::TextureHandle TextureStore::upload(const ImageData& image) {
    if (image.width == 0 || image.height == 0) return {};
    gpu::Format format = image.srgb ? gpu::Format::RGBA8Srgb : gpu::Format::RGBA8Unorm;
    if (!image.levels.empty()) {
        if (image.format == ImageFormat::BC7) format = image.srgb ? gpu::Format::BC7Srgb : gpu::Format::BC7Unorm;
        if (image.format == ImageFormat::BC5) format = gpu::Format::BC5Unorm;
        for (std::size_t l = 0; l < image.levels.size(); ++l) {
            const std::uint32_t w = std::max(1u, image.width >> l), h = std::max(1u, image.height >> l);
            if (image.levels[l].size() != gpu::texture_data_size(format, w, h)) return {};
        }
    } else if (image.rgba.size() != std::size_t{image.width} * image.height * 4) {
        return {};
    }
    const auto mips = image.levels.empty() ? build_mips(image, jobs_) : image.levels;
    const gpu::TextureHandle t = device_.create_texture({.width = image.width, .height = image.height,
                                                         .format = format,
                                                         .usage = gpu::TextureUsage::Sampled,
                                                         .mip_levels = static_cast<std::uint32_t>(mips.size()),
                                                         .debug_name = "astral.material_texture"});
    for (std::uint32_t m = 0; m < mips.size(); ++m) {
        if (!device_.write_texture(t, mips[m].data(), mips[m].size(), m)) {
            device_.destroy(t);
            return {};
        }
    }
    return t;
}

TextureHandle TextureStore::create(const ImageData& image) {
    const gpu::TextureHandle t = upload(image);
    if (!t.valid()) return {};
    return textures_.create({t, image.width, image.height});
}

bool TextureStore::update(TextureHandle handle, const ImageData& image) {
    GpuTexture* tex = textures_.get(handle);
    if (!tex) return false;
    const gpu::TextureHandle t = upload(image);
    if (!t.valid()) return false;
    device_.destroy(tex->texture); // deferred until the GPU is done with it
    *tex = {t, image.width, image.height};
    return true;
}

void TextureStore::destroy(TextureHandle handle) {
    if (const GpuTexture* tex = textures_.get(handle)) {
        device_.destroy(tex->texture);
        textures_.destroy(handle);
    }
}

TextureHandle TextureStore::create_render_target(std::uint32_t width, std::uint32_t height, gpu::Format format) {
    if (width == 0 || height == 0) return {};
    const gpu::TextureHandle t = device_.create_texture({
        .width = width,
        .height = height,
        .format = format,
        .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled | gpu::TextureUsage::TransferSrc,
        .mip_levels = 1,
        .debug_name = "astral.render_target",
    });
    if (!t.valid()) return {};
    return textures_.create({t, width, height});
}

bool TextureStore::resize_render_target(TextureHandle handle, std::uint32_t width, std::uint32_t height,
                                        gpu::Format format) {
    GpuTexture* tex = textures_.get(handle);
    if (!tex || width == 0 || height == 0) return false;
    if (tex->width == width && tex->height == height) return true;
    const gpu::TextureHandle t = device_.create_texture({
        .width = width,
        .height = height,
        .format = format,
        .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled | gpu::TextureUsage::TransferSrc,
        .mip_levels = 1,
        .debug_name = "astral.render_target",
    });
    if (!t.valid()) return false;
    device_.destroy(tex->texture); // deferred until in-flight GPU submissions finish
    *tex = {t, width, height};
    return true;
}

gpu::TextureHandle TextureStore::native_handle(TextureHandle handle) const noexcept {
    const GpuTexture* tex = textures_.get(handle);
    return tex ? tex->texture : gpu::TextureHandle{};
}

bool TextureStore::is_linear_sampled(TextureHandle handle) const noexcept {
    const GpuTexture* tex = textures_.get(handle);
    if (!tex) return false;
    const gpu::Format fmt = device_.texture_desc(tex->texture).format;
    return fmt == gpu::Format::RGBA8Srgb || fmt == gpu::Format::BGRA8Srgb || fmt == gpu::Format::BC7Srgb;
}

std::uint32_t TextureStore::bindless_or(TextureHandle handle, std::uint32_t fallback) const {
    const GpuTexture* tex = textures_.get(handle);
    return tex ? device_.bindless_index(tex->texture) : fallback;
}

std::uint32_t TextureStore::stand_in_bindless(bool normal) const {
    const GpuTexture* stand_in = textures_.get(normal ? flat_normal_ : white_);
    return stand_in ? device_.bindless_index(stand_in->texture) : 0u;
}

} // namespace astral
