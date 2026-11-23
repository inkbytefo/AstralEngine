#pragma once

#include "gpu_layouts.hpp"

#include <astral/core/handle.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>

namespace astral {

class TextureStore {
public:
    explicit TextureStore(gpu::Device& device);
    ~TextureStore();
    TextureStore(const TextureStore&) = delete;
    TextureStore& operator=(const TextureStore&) = delete;

    TextureHandle create(const ImageData& image);
    bool update(TextureHandle handle, const ImageData& image);
    TextureHandle create_render_target(std::uint32_t width, std::uint32_t height, gpu::Format format);
    bool resize_render_target(TextureHandle handle, std::uint32_t width, std::uint32_t height, gpu::Format format);
    void destroy(TextureHandle handle);

    [[nodiscard]] bool supports(ImageFormat format) const noexcept;
    [[nodiscard]] const GpuTexture* get(TextureHandle handle) const noexcept { return textures_.get(handle); }
    [[nodiscard]] gpu::TextureHandle native_handle(TextureHandle handle) const noexcept;
    [[nodiscard]] bool is_linear_sampled(TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint32_t bindless_or(TextureHandle handle, std::uint32_t fallback) const;
    [[nodiscard]] std::uint32_t stand_in_bindless(bool normal) const;
    [[nodiscard]] TextureHandle white() const noexcept { return white_; }
    [[nodiscard]] TextureHandle flat_normal() const noexcept { return flat_normal_; }

private:
    gpu::TextureHandle upload(const ImageData& image);

    gpu::Device& device_;
    HandlePool<GpuTexture, RenderTextureTag> textures_;
    TextureHandle white_{};       // stand-in for unset .ashader colour textures
    TextureHandle flat_normal_{}; // stand-in for unset .ashader normal maps
};

} // namespace astral
