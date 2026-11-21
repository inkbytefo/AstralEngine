#include "Astral/Engine/Assets/AssetManager.hpp"

#include "Astral/Core/Log.hpp"
#include "Astral/Render/MeshPrimitives.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>

#include <functional>

namespace Astral {

namespace fs = std::filesystem;

AssetManager::AssetManager(Render::Renderer& renderer, fs::path root) : m_Renderer(renderer), m_Root(std::move(root)) {}

AssetManager::~AssetManager() { Clear(); }

fs::path AssetManager::Resolve(const std::string& path) const {
    fs::path p(path);
    return p.is_absolute() ? p : m_Root / p;
}

void AssetManager::Clear() {
    for (auto& [path, model] : m_Models) {
        for (auto& prim : model.primitives) {
            m_Renderer.DestroyMesh(prim.mesh);
            m_Renderer.DestroyMaterial(prim.material);
        }
    }
    for (auto& [path, tex] : m_Textures) m_Renderer.DestroyTexture(tex);
    m_Models.clear();
    m_Textures.clear();
    m_Failed.clear();
}

Model AssetManager::MakeBuiltin(const std::string& name) {
    Render::MeshDesc desc;
    if (name == "builtin:sphere") desc = Render::Primitives::Sphere(0.5f);
    else if (name == "builtin:plane") desc = Render::Primitives::Plane(0.5f);
    else desc = Render::Primitives::Cube(0.5f);
    Model model;
    model.path = name;
    Render::MaterialDesc material;
    material.baseColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    material.roughness = 0.6f;
    model.primitives.push_back({m_Renderer.CreateMesh(desc), m_Renderer.CreateMaterial(material), glm::mat4(1.0f)});
    model.boundsMin = glm::vec3(-0.5f, name == "builtin:plane" ? 0.0f : -0.5f, -0.5f);
    model.boundsMax = glm::vec3(0.5f, name == "builtin:plane" ? 0.0f : 0.5f, 0.5f);
    return model;
}

const Model* AssetManager::GetModel(const std::string& path) {
    if (auto it = m_Models.find(path); it != m_Models.end()) return &it->second;
    if (m_Failed.count(path)) return nullptr;

    if (path.rfind("builtin:", 0) == 0) {
        if (path != "builtin:cube" && path != "builtin:sphere" && path != "builtin:plane") {
            Log::Warn("assets", "unknown builtin mesh '{}'", path);
            m_Failed[path] = true;
            return nullptr;
        }
        return &(m_Models[path] = MakeBuiltin(path));
    }

    Model model;
    model.path = path;
    if (!LoadGltf(Resolve(path), model)) {
        m_Failed[path] = true;
        return nullptr;
    }
    return &(m_Models[path] = std::move(model));
}

Render::TextureHandle AssetManager::GetTexture(const std::string& path, bool srgb) {
    const std::string key = path + (srgb ? "#srgb" : "#linear");
    if (auto it = m_Textures.find(key); it != m_Textures.end()) return it->second;
    int w, h, channels;
    stbi_uc* pixels = stbi_load(Resolve(path).string().c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        Log::Warn("assets", "cannot load texture '{}': {}", path, stbi_failure_reason());
        return {};
    }
    Render::TextureDesc desc;
    desc.width = uint32_t(w);
    desc.height = uint32_t(h);
    desc.format = srgb ? Render::TextureFormat::RGBA8_sRGB : Render::TextureFormat::RGBA8_Linear;
    desc.pixels = std::span<const uint8_t>(pixels, size_t(w) * h * 4);
    desc.debugName = path;
    Render::TextureHandle tex = m_Renderer.CreateTexture(desc);
    stbi_image_free(pixels);
    return m_Textures[key] = tex;
}

// ---------------------------------------------------------------------------- glTF

namespace {

struct GltfContext {
    Render::Renderer& renderer;
    fs::path directory;
    std::string key;
    std::unordered_map<const cgltf_image*, Render::TextureHandle> textures[2]; // [srgb]
    std::unordered_map<const cgltf_material*, Render::MaterialHandle> materials;
    std::vector<Render::TextureHandle>* ownedTextures;
};

Render::TextureHandle LoadImage(GltfContext& ctx, const cgltf_texture_view& view, bool srgb) {
    if (!view.texture || !view.texture->image) return {};
    const cgltf_image* image = view.texture->image;
    auto& cache = ctx.textures[srgb ? 1 : 0];
    if (auto it = cache.find(image); it != cache.end()) return it->second;

    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = nullptr;
    if (image->buffer_view) { // embedded (GLB)
        const auto* bv = image->buffer_view;
        const auto* data = static_cast<const stbi_uc*>(bv->buffer->data) + bv->offset;
        pixels = stbi_load_from_memory(data, int(bv->size), &w, &h, &channels, 4);
    } else if (image->uri && std::string_view(image->uri).rfind("data:", 0) != 0) {
        std::string uri = image->uri;
        cgltf_decode_uri(uri.data());
        pixels = stbi_load((ctx.directory / uri.c_str()).string().c_str(), &w, &h, &channels, 4);
    }
    if (!pixels) {
        Log::Warn("assets", "{}: could not decode an image", ctx.key);
        return cache[image] = {};
    }
    Render::TextureDesc desc;
    desc.width = uint32_t(w);
    desc.height = uint32_t(h);
    desc.format = srgb ? Render::TextureFormat::RGBA8_sRGB : Render::TextureFormat::RGBA8_Linear;
    desc.pixels = std::span<const uint8_t>(pixels, size_t(w) * h * 4);
    desc.debugName = image->name ? image->name : ctx.key;
    Render::TextureHandle tex = ctx.renderer.CreateTexture(desc);
    stbi_image_free(pixels);
    ctx.ownedTextures->push_back(tex);
    return cache[image] = tex;
}

Render::MaterialDesc ConvertMaterial(GltfContext& ctx, const cgltf_material* m) {
    Render::MaterialDesc d;
    if (!m) return d;
    if (m->has_pbr_metallic_roughness) {
        const auto& pbr = m->pbr_metallic_roughness;
        d.baseColor = glm::make_vec4(pbr.base_color_factor);
        d.metallic = pbr.metallic_factor;
        d.roughness = pbr.roughness_factor;
        d.baseColorTexture = LoadImage(ctx, pbr.base_color_texture, true);
        d.metallicRoughnessTexture = LoadImage(ctx, pbr.metallic_roughness_texture, false);
    }
    d.emissive = glm::make_vec3(m->emissive_factor);
    if (m->has_emissive_strength) d.emissive *= m->emissive_strength.emissive_strength;
    d.emissiveTexture = LoadImage(ctx, m->emissive_texture, true);
    d.normalTexture = LoadImage(ctx, m->normal_texture, false);
    d.normalScale = m->normal_texture.texture ? m->normal_texture.scale : 1.0f;
    return d;
}

bool ReadPrimitive(const cgltf_primitive& prim, Render::MeshDesc& out) {
    if (prim.type != cgltf_primitive_type_triangles) return false;
    const cgltf_accessor *pos = nullptr, *nrm = nullptr, *tan = nullptr, *uv = nullptr;
    for (size_t i = 0; i < prim.attributes_count; ++i) {
        const auto& a = prim.attributes[i];
        if (a.type == cgltf_attribute_type_position) pos = a.data;
        else if (a.type == cgltf_attribute_type_normal) nrm = a.data;
        else if (a.type == cgltf_attribute_type_tangent) tan = a.data;
        else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv = a.data;
    }
    if (!pos) return false;

    out.vertices.resize(pos->count);
    for (size_t i = 0; i < pos->count; ++i) {
        Render::Vertex& v = out.vertices[i];
        cgltf_accessor_read_float(pos, i, &v.position.x, 3);
        if (nrm) cgltf_accessor_read_float(nrm, i, &v.normal.x, 3);
        if (tan) cgltf_accessor_read_float(tan, i, &v.tangent.x, 4);
        if (uv) cgltf_accessor_read_float(uv, i, &v.uv.x, 2);
    }
    if (prim.indices) {
        out.indices.resize(prim.indices->count);
        for (size_t i = 0; i < prim.indices->count; ++i) out.indices[i] = uint32_t(cgltf_accessor_read_index(prim.indices, i));
    } else {
        out.indices.resize(pos->count);
        for (size_t i = 0; i < pos->count; ++i) out.indices[i] = uint32_t(i);
    }

    // Missing normals/tangents are derived from the triangles.
    if (!nrm || !tan) {
        std::vector<glm::vec3> normals(out.vertices.size(), glm::vec3(0.0f)), tangents(out.vertices.size(), glm::vec3(0.0f));
        for (size_t t = 0; t + 2 < out.indices.size(); t += 3) {
            uint32_t i0 = out.indices[t], i1 = out.indices[t + 1], i2 = out.indices[t + 2];
            const auto &a = out.vertices[i0], &b = out.vertices[i1], &c = out.vertices[i2];
            glm::vec3 e1 = b.position - a.position, e2 = c.position - a.position;
            glm::vec3 n = glm::cross(e1, e2);
            glm::vec2 d1 = b.uv - a.uv, d2 = c.uv - a.uv;
            float det = d1.x * d2.y - d2.x * d1.y;
            glm::vec3 tg = std::abs(det) > 1e-8f ? (e1 * d2.y - e2 * d1.y) / det : glm::normalize(e1);
            for (uint32_t i : {i0, i1, i2}) {
                normals[i] += n;
                tangents[i] += tg;
            }
        }
        for (size_t i = 0; i < out.vertices.size(); ++i) {
            auto& v = out.vertices[i];
            if (!nrm) v.normal = glm::length(normals[i]) > 0 ? glm::normalize(normals[i]) : glm::vec3(0, 1, 0);
            if (!tan) {
                glm::vec3 t = tangents[i] - v.normal * glm::dot(v.normal, tangents[i]);
                v.tangent = glm::vec4(glm::length(t) > 1e-6f ? glm::normalize(t) : glm::vec3(1, 0, 0), 1.0f);
            }
        }
    }
    return true;
}

} // namespace

bool AssetManager::LoadGltf(const fs::path& file, Model& model) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    const std::string pathStr = file.string();
    if (cgltf_parse_file(&options, pathStr.c_str(), &data) != cgltf_result_success) {
        Log::Warn("assets", "cannot parse glTF '{}'", pathStr);
        return false;
    }
    if (cgltf_load_buffers(&options, data, pathStr.c_str()) != cgltf_result_success) {
        Log::Warn("assets", "cannot load buffers of '{}'", pathStr);
        cgltf_free(data);
        return false;
    }

    std::vector<Render::TextureHandle> owned;
    GltfContext ctx{m_Renderer, file.parent_path(), model.path, {}, {}, &owned};
    glm::vec3 lo(1e30f), hi(-1e30f);

    std::function<void(const cgltf_node*)> visit = [&](const cgltf_node* node) {
        if (node->mesh) {
            float m[16];
            cgltf_node_transform_world(node, m);
            glm::mat4 world = glm::make_mat4(m);
            for (size_t p = 0; p < node->mesh->primitives_count; ++p) {
                const cgltf_primitive& prim = node->mesh->primitives[p];
                Render::MeshDesc desc;
                if (!ReadPrimitive(prim, desc)) continue;
                desc.debugName = model.path;
                for (const auto& v : desc.vertices) {
                    glm::vec3 wp = glm::vec3(world * glm::vec4(v.position, 1.0f));
                    lo = glm::min(lo, wp);
                    hi = glm::max(hi, wp);
                }
                auto& material = ctx.materials[prim.material];
                if (!material) material = m_Renderer.CreateMaterial(ConvertMaterial(ctx, prim.material));
                model.primitives.push_back({m_Renderer.CreateMesh(desc), material, world});
            }
        }
        for (size_t c = 0; c < node->children_count; ++c) visit(node->children[c]);
    };
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    if (scene) {
        for (size_t i = 0; i < scene->nodes_count; ++i) visit(scene->nodes[i]);
    } else {
        for (size_t i = 0; i < data->nodes_count; ++i)
            if (!data->nodes[i].parent) visit(&data->nodes[i]);
    }
    cgltf_free(data);

    for (size_t i = 0; i < owned.size(); ++i) m_Textures[model.path + "#tex" + std::to_string(i)] = owned[i];
    if (model.primitives.empty()) {
        Log::Warn("assets", "'{}' contains no triangle meshes", pathStr);
        return false;
    }
    model.boundsMin = lo;
    model.boundsMax = hi;
    Log::Info("assets", "loaded '{}' ({} primitives)", model.path, model.primitives.size());
    return true;
}

} // namespace Astral
