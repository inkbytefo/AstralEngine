#include "stores/material_store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <cmath>

namespace astral {

MaterialStore::MaterialStore(gpu::Device& device, const TextureStore& textures, const ShaderStore& shaders)
    : device_(device), textures_(textures), shaders_(shaders) {
    default_material_ = materials_.create(MaterialDesc{});
    slots_ = default_material_.index + 1;
}

namespace {
/// Largest uniform-block word a texture binding may name (a uniform block is at most 64 KiB).
constexpr std::uint32_t max_param_words = 16384;

bool valid_params(const MaterialDesc& desc) noexcept {
    if (desc.params.size() > max_param_words) return false;
    for (const MaterialDesc::ParamTexture& t : desc.textures) {
        if (t.offset >= max_param_words) return false;
    }
    return true;
}
} // namespace

MaterialHandle MaterialStore::create(const MaterialDesc& desc) {
    if (!valid_params(desc)) return {};
    const MaterialHandle h = materials_.create(desc);
    slots_ = std::max(slots_, h.index + 1);
    return h;
}

bool MaterialStore::update(MaterialHandle handle, const MaterialDesc& desc) {
    MaterialDesc* d = materials_.get(handle);
    if (!d || !valid_params(desc)) return false;
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

GpuMaterial MaterialStore::to_gpu(const MaterialDesc& d, const std::array<double, 3>& origin) const {
    GpuMaterial gm{};
    gm.base_color = d.base_color;
    gm.emissive_roughness = {d.emissive.x, d.emissive.y, d.emissive.z, d.roughness};
    gm.params = {d.metallic, d.normal_scale, d.wetness, 0.0f};
    gm.textures[0] = textures_.bindless_or(d.base_color_texture, no_texture);
    gm.textures[1] = textures_.bindless_or(d.metallic_roughness_texture, no_texture);
    gm.textures[2] = textures_.bindless_or(d.normal_texture, no_texture);
    gm.textures[3] = textures_.bindless_or(d.emissive_texture, no_texture);
    gm.custom[2] = static_cast<std::uint32_t>(d.shading_model);
    gm.coat_aniso = {d.clearcoat.strength, d.clearcoat.roughness, d.anisotropy.strength, d.anisotropy.rotation};
    gm.sheen = {d.sheen.color.x, d.sheen.color.y, d.sheen.color.z, d.sheen.roughness};
    gm.transmission = {d.transmission.strength, d.transmission.ior, d.transmission.thickness,
                       d.transmission.attenuation_distance};
    gm.subsurface = {d.subsurface.color.x, d.subsurface.color.y, d.subsurface.color.z, d.subsurface.strength};
    gm.attenuation = {d.transmission.attenuation_color.x, d.transmission.attenuation_color.y,
                      d.transmission.attenuation_color.z, d.subsurface.radius};
    gm.detail_textures[0] = textures_.bindless_or(d.detail.albedo_texture, no_texture);
    gm.detail_textures[1] = textures_.bindless_or(d.detail.normal_texture, no_texture);
    gm.detail_textures[2] = textures_.bindless_or(d.detail.height_texture, no_texture);
    gm.detail_textures[3] = textures_.bindless_or(d.occlusion_texture, no_texture);
    gm.detail_params = {d.detail.uv_scale, d.detail.normal_scale, d.detail.parallax_scale, std::clamp(d.occlusion_strength, 0.0f, 1.0f)};
    const auto& m = d.mapping;
    const float size = std::isfinite(m.tile_size_m) ? std::max(m.tile_size_m, 0.001f) : 1.0f;
    gm.uv_transform = {m.uv_scale.x, m.uv_scale.y, m.uv_offset.x, m.uv_offset.y};
    gm.projection = {static_cast<float>(m.projection), 1.0f / size, std::clamp(m.blend_sharpness, 1.0f, 32.0f),
                     m.world_space ? 1.0f : 0.0f};
    gm.projection_phase.w = m.uv_rotation;
    if (m.world_space && m.projection != TextureProjection::MeshUV) {
        for (int i = 0; i < 3; ++i) gm.projection_phase[i] = static_cast<float>(std::fmod(origin[i], size) / size);
    }
    return gm;
}

void MaterialStore::upload(GrowBuffer& material_buffer, GrowBuffer& param_buffer, const std::array<double, 3>& origin) {
    // Material table indexed by handle slot; dead slots keep the default material's values.
    table_.assign(slots_, {});
    const MaterialDesc* fallback = materials_.get(default_material_);
    const GpuMaterial fallback_gpu = to_gpu(*fallback, origin);
    for (auto& entry : table_) entry = fallback_gpu;
    param_words_.clear();
    materials_.for_each([&](MaterialHandle h, MaterialDesc& d) {
        GpuMaterial gm = to_gpu(d, origin);
        if (const GpuShader* gs = shaders_.get(d.shader)) {
            const auto base = static_cast<std::uint32_t>(param_words_.size());
            gm.custom[0] = base;
            gm.custom[1] = d.shader.index + 1;
            std::size_t words = d.params.size();
            for (const MaterialDesc::ParamTexture& t : d.textures) words = std::max<std::size_t>(words, std::size_t{t.offset} + 1);
            for (const SurfaceShaderBuild::Sampler& sm : gs->samplers) words = std::max<std::size_t>(words, std::size_t{sm.offset} + 1);
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
