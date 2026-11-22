#include "stores/material_store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace astral {

MaterialStore::MaterialStore(gpu::Device& device, const TextureStore& textures, const ShaderStore& shaders)
    : device_(device), textures_(textures), shaders_(shaders) {
    default_material_ = materials_.create(MaterialDesc{});
    slots_ = default_material_.index + 1;
}

MaterialHandle MaterialStore::create(const MaterialDesc& desc) {
    const MaterialHandle h = materials_.create(desc);
    slots_ = std::max(slots_, h.index + 1);
    return h;
}

bool MaterialStore::update(MaterialHandle handle, const MaterialDesc& desc) {
    MaterialDesc* d = materials_.get(handle);
    if (!d) return false;
    *d = desc;
    return true;
}

void MaterialStore::destroy(MaterialHandle handle) {
    if (handle == default_material_) return; // always valid
    materials_.destroy(handle);
}

std::uint32_t MaterialStore::resolve_index(MaterialHandle handle) const noexcept {
    return materials_.get(handle) ? handle.index : default_material_.index;
}

const GpuShader* MaterialStore::shader_of(MaterialHandle handle) const noexcept {
    const MaterialDesc* d = materials_.get(handle);
    return d ? shaders_.get(d->shader) : nullptr;
}

std::uint32_t MaterialStore::surface_shader_key(MaterialHandle handle) const noexcept {
    const MaterialDesc* d = materials_.get(handle);
    const GpuShader* gs = d ? shaders_.get(d->shader) : nullptr;
    return gs && gs->kind == GpuShader::Kind::Surface ? d->shader.index + 1 : 0u; // post / sky shaders draw no meshes
}

bool MaterialStore::is_post(MaterialHandle handle) const noexcept {
    const GpuShader* gs = shader_of(handle);
    return gs && gs->kind == GpuShader::Kind::Post;
}

GpuMaterial MaterialStore::to_gpu(const MaterialDesc& d) const {
    return {d.base_color,
            {d.emissive.x, d.emissive.y, d.emissive.z, d.roughness},
            {d.metallic, d.normal_scale, 0, 0},
            {textures_.bindless_or(d.base_color_texture, no_texture),
             textures_.bindless_or(d.metallic_roughness_texture, no_texture),
             textures_.bindless_or(d.normal_texture, no_texture),
             textures_.bindless_or(d.emissive_texture, no_texture)}};
}

void MaterialStore::upload(GrowBuffer& material_buffer, GrowBuffer& param_buffer) {
    // Material table indexed by handle slot; dead slots keep the default material's values.
    table_.assign(slots_, {});
    const MaterialDesc* fallback = materials_.get(default_material_);
    const GpuMaterial fallback_gpu = to_gpu(*fallback);
    for (auto& entry : table_) entry = fallback_gpu;
    param_words_.clear();
    materials_.for_each([&](MaterialHandle h, MaterialDesc& d) {
        GpuMaterial gm = to_gpu(d);
        if (const GpuShader* gs = shaders_.get(d.shader)) {
            const auto base = static_cast<std::uint32_t>(param_words_.size());
            gm.custom[0] = base;
            gm.custom[1] = d.shader.index + 1;
            std::size_t words = d.params.size();
            for (const MaterialDesc::ParamTexture& t : d.textures) words = std::max<std::size_t>(words, t.offset + 1);
            for (const SurfaceShaderBuild::Sampler& sm : gs->samplers) words = std::max<std::size_t>(words, sm.offset + 1);
            param_words_.resize(base + words, 0u);
            for (std::size_t i = 0; i < d.params.size(); ++i) std::memcpy(&param_words_[base + i], &d.params[i], 4);
            for (const SurfaceShaderBuild::Sampler& sm : gs->samplers) { // unset textures read as white / a flat normal
                param_words_[base + sm.offset] = textures_.stand_in_bindless(sm.normal);
            }
            for (const MaterialDesc::ParamTexture& t : d.textures) {
                const GpuTexture* tex = textures_.get(t.texture);
                param_words_[base + t.offset] = tex ? device_.bindless_index(tex->texture) : textures_.stand_in_bindless(t.normal);
            }
        }
        table_[h.index] = gm;
    });
    material_buffer.write(device_, table_.data(), table_.size() * sizeof(GpuMaterial));
    if (param_words_.empty()) param_words_.push_back(0);
    param_buffer.write(device_, param_words_.data(), param_words_.size() * sizeof(std::uint32_t));
}

} // namespace astral
