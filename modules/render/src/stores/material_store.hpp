#pragma once

#include "frame/frame_slot.hpp"
#include "gpu_layouts.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/core/handle.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>
#include <vector>

namespace astral {

class MaterialStore {
public:
    MaterialStore(gpu::Device& device, const TextureStore& textures, const ShaderStore& shaders);
    MaterialStore(const MaterialStore&) = delete;
    MaterialStore& operator=(const MaterialStore&) = delete;

    MaterialHandle create(const MaterialDesc& desc);
    bool update(MaterialHandle handle, const MaterialDesc& desc);
    void destroy(MaterialHandle handle);

    [[nodiscard]] const MaterialDesc* get(MaterialHandle handle) const noexcept { return materials_.get(handle); }
    [[nodiscard]] MaterialHandle default_material() const noexcept { return default_material_; }
    [[nodiscard]] std::uint32_t resolve_index(MaterialHandle handle) const noexcept;
    [[nodiscard]] const GpuShader* shader_of(MaterialHandle handle) const noexcept;
    /// 1-based shader draw key for surface shaders (0 = standard pipeline; post/sky draw no meshes).
    [[nodiscard]] std::uint32_t surface_shader_key(MaterialHandle handle) const noexcept;
    [[nodiscard]] bool is_post(MaterialHandle handle) const noexcept;

    /// Uploads the GPU material table and .ashader parameter block into the frame's buffers.
    void upload(GrowBuffer& material_buffer, GrowBuffer& param_buffer, const std::array<double, 3>& origin = {});

private:
    [[nodiscard]] GpuMaterial to_gpu(const MaterialDesc& desc, const std::array<double, 3>& origin) const;

    gpu::Device& device_;
    const TextureStore& textures_;
    const ShaderStore& shaders_;
    HandlePool<MaterialDesc, MaterialTag> materials_;
    MaterialHandle default_material_{};
    std::uint32_t slots_ = 0; // highest material index + 1 (size of the GPU table)
    std::vector<GpuMaterial> table_;
    std::vector<std::uint32_t> param_words_;
};

} // namespace astral
