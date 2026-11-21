#include <astral/assets/cook.hpp>
#include <astral/core/bytes.hpp>

namespace astral {

namespace {

constexpr std::uint8_t magic[4] = {'A', 'M', 'D', 'L'};
constexpr std::uint32_t format_version = 1;
/// Counts read from a file are checked against this before allocating (a damaged count must not
/// make us allocate gigabytes); real models are far below it.
constexpr std::uint64_t max_count = 1u << 28;

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
        }
        w.varint(p.mesh.indices.size());
        for (std::uint32_t i : p.mesh.indices) w.u32(i);
    }

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
    }

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
    if (r.u32() != format_version) return fail("unsupported version (cook again)");
    ModelData m;

    std::uint64_t n = r.varint();
    if (n > max_count) return fail("damaged primitive count");
    m.primitives.resize(static_cast<std::size_t>(n));
    for (ModelPrimitive& p : m.primitives) {
        p.material = index(r);
        const std::uint64_t vertices = r.varint();
        if (vertices * 32 > r.remaining()) return fail("truncated vertices");
        p.mesh.vertices.resize(static_cast<std::size_t>(vertices));
        for (Vertex& v : p.mesh.vertices) {
            vec(r, &v.position.x, 3);
            vec(r, &v.normal.x, 3);
            vec(r, &v.uv.x, 2);
        }
        const std::uint64_t indices = r.varint();
        if (indices * 4 > r.remaining()) return fail("truncated indices");
        p.mesh.indices.resize(static_cast<std::size_t>(indices));
        for (std::uint32_t& i : p.mesh.indices) {
            i = r.u32();
            if (i >= vertices) return fail("index out of range");
        }
    }

    n = r.varint();
    if (n > max_count) return fail("damaged material count");
    m.materials.resize(static_cast<std::size_t>(n));
    for (ModelMaterial& mat : m.materials) {
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
    }

    n = r.varint();
    if (n > max_count) return fail("damaged image count");
    m.images.resize(static_cast<std::size_t>(n));
    for (ModelImage& img : m.images) {
        ImageData& d = img.image;
        img.source = r.string();
        d.width = r.u32();
        d.height = r.u32();
        d.srgb = r.u8() != 0;
        const std::uint8_t format = r.u8();
        if (format > static_cast<std::uint8_t>(ImageFormat::BC7)) return fail("unknown image format");
        d.format = static_cast<ImageFormat>(format);
        if (r.u8() != 0) {
            const std::uint64_t levels = r.varint();
            if (levels > 32) return fail("damaged level count");
            for (std::uint64_t l = 0; l < levels; ++l) {
                const auto bytes = r.bytes(r.varint());
                d.levels.emplace_back(bytes.begin(), bytes.end());
            }
        } else {
            const auto bytes = r.bytes(r.varint());
            d.rgba.assign(bytes.begin(), bytes.end());
        }
    }

    n = r.varint();
    if (n > max_count) return fail("damaged node count");
    m.nodes.resize(static_cast<std::size_t>(n));
    for (ModelNode& node : m.nodes) {
        node.name = r.string();
        vec(r, &node.translation.x, 3);
        vec(r, &node.rotation.x, 4);
        vec(r, &node.scale.x, 3);
        if (!u32s(r, node.primitives) || !u32s(r, node.children)) return fail("truncated node");
        for (std::uint32_t p : node.primitives) {
            if (p >= m.primitives.size()) return fail("node refers to a missing primitive");
        }
        for (std::uint32_t c : node.children) {
            if (c >= m.nodes.size()) return fail("node refers to a missing child");
        }
    }
    if (!u32s(r, m.roots)) return fail("truncated roots");
    for (std::uint32_t root : m.roots) {
        if (root >= m.nodes.size()) return fail("missing root node");
    }
    n = r.varint();
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
