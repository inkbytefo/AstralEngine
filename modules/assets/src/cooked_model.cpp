#include <astral/assets/cook.hpp>
#include <astral/core/bytes.hpp>

#include <algorithm>

#include "model_hierarchy.hpp"

namespace astral {

namespace {

constexpr std::uint8_t magic[4] = {'A', 'M', 'D', 'L'};
constexpr std::uint32_t format_version = 3;
/// Counts read from a file are checked against this before allocating (a damaged count must not
/// make us allocate gigabytes); real models are far below it.
constexpr std::uint64_t max_count = 1u << 28;
/// Smallest encoding of one record of each section: a count claiming more records than the
/// remaining bytes can hold is damaged, and is refused before anything is allocated.
constexpr std::uint64_t min_primitive_bytes = 4 + 1 + 1;        // material, vertex count, index count
constexpr std::uint64_t min_material_bytes = 1 + 4 * 10 + 4 * 4; // name, 10 floats, 4 image indices (v1)
constexpr std::uint64_t min_image_bytes = 1 + 4 + 4 + 1 + 1 + 1 + 1;
constexpr std::uint64_t min_node_bytes = 1 + 4 * 10 + 1 + 1;

bool count_fits(const ByteReader& r, std::uint64_t n, std::uint64_t min_bytes) {
    return n <= max_count && n <= r.remaining() / min_bytes;
}

void vec(ByteWriter& w, const float* f, int n) {
    for (int i = 0; i < n; ++i) w.f32(f[i]);
}
void vec(ByteReader& r, float* f, int n) {
    for (int i = 0; i < n; ++i) f[i] = r.f32();
}
void u32s(ByteWriter& w, const std::vector<std::uint32_t>& v) {
    w.varint(v.size());
    for (std::uint32_t x : v) w.varint(x);
}
bool u32s(ByteReader& r, std::vector<std::uint32_t>& v) {
    const std::uint64_t n = r.varint();
    if (n > r.remaining()) return false;
    v.resize(static_cast<std::size_t>(n));
    for (auto& x : v) x = static_cast<std::uint32_t>(r.varint());
    return r.ok();
}
std::int32_t index(ByteReader& r) { return static_cast<std::int32_t>(r.u32()); }

const char* read_primitives(ByteReader& r, std::vector<ModelPrimitive>& primitives, std::uint32_t version) {
    const std::uint64_t n = r.varint();
    if (!count_fits(r, n, min_primitive_bytes)) return "damaged primitive count";
    primitives.resize(static_cast<std::size_t>(n));
    for (ModelPrimitive& p : primitives) {
        p.material = index(r);
        const std::uint64_t vertices = r.varint();
        if (vertices > r.remaining() / (version >= 3 ? 40 : 32)) return "truncated vertices";
        p.mesh.vertices.resize(static_cast<std::size_t>(vertices));
        for (Vertex& v : p.mesh.vertices) {
            vec(r, &v.position.x, 3);
            vec(r, &v.normal.x, 3);
            vec(r, &v.uv.x, 2);
            if (version >= 3) vec(r, &v.uv1.x, 2);
            else v.uv1 = v.uv;
        }
        const std::uint64_t indices = r.varint();
        if (indices > r.remaining() / 4) return "truncated indices";
        p.mesh.indices.resize(static_cast<std::size_t>(indices));
        for (std::uint32_t& i : p.mesh.indices) {
            i = r.u32();
            if (i >= vertices) return "index out of range";
        }
    }
    return nullptr;
}

const char* read_materials(ByteReader& r, std::vector<ModelMaterial>& materials, std::uint32_t version) {
    const std::uint64_t n = r.varint();
    if (!count_fits(r, n, min_material_bytes)) return "damaged material count";
    materials.resize(static_cast<std::size_t>(n));
    for (ModelMaterial& mat : materials) {
        mat.name = r.string();
        vec(r, &mat.base_color.x, 4);
        mat.roughness = r.f32();
        mat.metallic = r.f32();
        vec(r, &mat.emissive.x, 3);
        mat.normal_scale = r.f32();
        mat.base_color_image = index(r);
        mat.metallic_roughness_image = index(r);
        mat.normal_image = index(r);
        mat.emissive_image = index(r);
        if (version >= 2) {
            mat.occlusion_image = index(r);
            const auto mode = r.u8();
            if (mode > static_cast<std::uint8_t>(ModelAlphaMode::Blend)) return "invalid alpha mode";
            mat.alpha_mode = static_cast<ModelAlphaMode>(mode);
            mat.alpha_cutoff = r.f32();
            mat.occlusion_strength = r.f32();
            mat.double_sided = r.u8() != 0;
            for (auto& sampler : mat.samplers) {
                sampler = {r.u8(), r.u8(), r.u8(), r.u8() != 0};
                if (sampler.wrap_u > 2 || sampler.wrap_v > 2 || sampler.min_filter > 5) return "invalid material sampler";
            }
        }
        if (version >= 3) {
            for (auto& mapping : mat.texture_transforms) {
                vec(r, &mapping.scale.x, 2);
                vec(r, &mapping.offset.x, 2);
                mapping.rotation = r.f32();
                mapping.uv_set = r.u8();
                if (mapping.uv_set > 1) return "invalid texture coordinate set";
            }
        }
    }
    return nullptr;
}

void write_materials(ByteWriter& w, const ModelData& m) {
    w.varint(m.materials.size());
    for (const ModelMaterial& mat : m.materials) {
        w.string(mat.name);
        vec(w, &mat.base_color.x, 4);
        w.f32(mat.roughness);
        w.f32(mat.metallic);
        vec(w, &mat.emissive.x, 3);
        w.f32(mat.normal_scale);
        for (int i : {mat.base_color_image, mat.metallic_roughness_image, mat.normal_image, mat.emissive_image}) {
            w.u32(static_cast<std::uint32_t>(i));
        }
        w.u32(static_cast<std::uint32_t>(mat.occlusion_image));
        w.u8(static_cast<std::uint8_t>(mat.alpha_mode));
        w.f32(mat.alpha_cutoff);
        w.f32(mat.occlusion_strength);
        w.u8(mat.double_sided ? 1 : 0);
        for (const auto& sampler : mat.samplers) {
            w.u8(sampler.wrap_u);
            w.u8(sampler.wrap_v);
            w.u8(sampler.min_filter);
            w.u8(sampler.mag_linear ? 1 : 0);
        }
        for (const auto& mapping : mat.texture_transforms) {
            vec(w, &mapping.scale.x, 2);
            vec(w, &mapping.offset.x, 2);
            w.f32(mapping.rotation);
            w.u8(mapping.uv_set);
        }
    }
}

const char* read_images(ByteReader& r, std::vector<ModelImage>& images) {
    const std::uint64_t n = r.varint();
    if (!count_fits(r, n, min_image_bytes)) return "damaged image count";
    images.resize(static_cast<std::size_t>(n));
    for (ModelImage& img : images) {
        ImageData& d = img.image;
        img.source = r.string();
        d.width = r.u32();
        d.height = r.u32();
        d.srgb = r.u8() != 0;
        const std::uint8_t format = r.u8();
        if (format > static_cast<std::uint8_t>(ImageFormat::BC7)) return "unknown image format";
        d.format = static_cast<ImageFormat>(format);
        if (r.u8() != 0) {
            const std::uint64_t levels = r.varint();
            if (levels > 32) return "damaged level count";
            for (std::uint64_t l = 0; l < levels; ++l) {
                const auto bytes = r.bytes(r.varint());
                d.levels.emplace_back(bytes.begin(), bytes.end());
            }
        } else {
            const auto bytes = r.bytes(r.varint());
            d.rgba.assign(bytes.begin(), bytes.end());
        }
    }
    return nullptr;
}

const char* read_nodes(ByteReader& r, ModelData& m) {
    const std::uint64_t n = r.varint();
    if (!count_fits(r, n, min_node_bytes)) return "damaged node count";
    m.nodes.resize(static_cast<std::size_t>(n));
    for (ModelNode& node : m.nodes) {
        node.name = r.string();
        vec(r, &node.translation.x, 3);
        vec(r, &node.rotation.x, 4);
        vec(r, &node.scale.x, 3);
        if (!u32s(r, node.primitives) || !u32s(r, node.children)) return "truncated node";
        for (std::uint32_t p : node.primitives) {
            if (p >= m.primitives.size()) return "node refers to a missing primitive";
        }
        for (std::uint32_t c : node.children) {
            if (c >= m.nodes.size()) return "node refers to a missing child";
        }
    }
    if (!u32s(r, m.roots)) return "truncated roots";
    for (std::uint32_t root : m.roots) {
        if (root >= m.nodes.size()) return "missing root node";
    }
    std::string hierarchy;
    if (!validate_hierarchy(m.nodes, hierarchy)) return "invalid node hierarchy (cycle or two parents)";
    return nullptr;
}

} // namespace

bool is_cooked_model(std::span<const std::uint8_t> data) noexcept {
    return data.size() >= 4 && std::equal(data.begin(), data.begin() + 4, magic);
}

std::vector<std::uint8_t> write_cooked_model(const ModelData& m) {
    ByteWriter w;
    w.bytes(magic);
    w.u32(format_version);

    w.varint(m.primitives.size());
    for (const ModelPrimitive& p : m.primitives) {
        w.u32(static_cast<std::uint32_t>(p.material));
        w.varint(p.mesh.vertices.size());
        for (const Vertex& v : p.mesh.vertices) {
            vec(w, &v.position.x, 3);
            vec(w, &v.normal.x, 3);
            vec(w, &v.uv.x, 2);
            vec(w, &v.uv1.x, 2);
        }
        w.varint(p.mesh.indices.size());
        for (std::uint32_t i : p.mesh.indices) w.u32(i);
    }

    write_materials(w, m);

    w.varint(m.images.size());
    for (const ModelImage& img : m.images) {
        const ImageData& d = img.image;
        w.string(img.source);
        w.u32(d.width);
        w.u32(d.height);
        w.u8(d.srgb ? 1 : 0);
        w.u8(static_cast<std::uint8_t>(d.format));
        // An image never cooked keeps its RGBA8 pixels (mips are built on upload); a cooked one its levels.
        const bool cooked = !d.levels.empty();
        w.u8(cooked ? 1 : 0);
        if (cooked) {
            w.varint(d.levels.size());
            for (const auto& level : d.levels) {
                w.varint(level.size());
                w.bytes(level);
            }
        } else {
            w.varint(d.rgba.size());
            w.bytes(d.rgba);
        }
    }

    w.varint(m.nodes.size());
    for (const ModelNode& n : m.nodes) {
        w.string(n.name);
        vec(w, &n.translation.x, 3);
        vec(w, &n.rotation.x, 4);
        vec(w, &n.scale.x, 3);
        u32s(w, n.primitives);
        u32s(w, n.children);
    }
    u32s(w, m.roots);
    w.varint(m.warnings.size());
    for (const std::string& s : m.warnings) w.string(s);
    return w.take();
}

std::optional<ModelData> read_cooked_model(std::span<const std::uint8_t> data, std::string* error) {
    auto fail = [&](const char* why) -> std::optional<ModelData> {
        if (error) *error = std::string("cooked model: ") + why;
        return std::nullopt;
    };
    if (!is_cooked_model(data)) return fail("not a cooked model (no AMDL header)");
    ByteReader r(data.subspan(4));
    const auto version = r.u32();
    if (version < 1 || version > format_version) return fail("unsupported version (cook again)");
    ModelData m;

    if (const char* err = read_primitives(r, m.primitives, version)) return fail(err);
    if (const char* err = read_materials(r, m.materials, version)) return fail(err);
    if (const char* err = read_images(r, m.images)) return fail(err);
    if (const char* err = read_nodes(r, m)) return fail(err);

    const std::uint64_t n = r.varint();
    if (n > max_count) return fail("damaged warning count");
    for (std::uint64_t i = 0; i < n; ++i) m.warnings.push_back(r.string());
    if (!r.ok()) return fail("truncated");
    if (r.remaining() != 0) return fail("unexpected bytes at the end");
    for (const ModelPrimitive& p : m.primitives) {
        if (p.material >= static_cast<int>(m.materials.size())) return fail("primitive refers to a missing material");
    }
    return m;
}

} // namespace astral
