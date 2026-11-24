// glTF 2.0 loader on top of astral::Json. Reads what the engine draws; skips the rest with warnings.
#include <astral/assets/model.hpp>
#include <astral/core/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace astral {

namespace {

constexpr int GL_UNSIGNED_BYTE = 5121, GL_UNSIGNED_SHORT = 5123, GL_UNSIGNED_INT = 5125, GL_FLOAT = 5126;

std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::optional<std::vector<std::uint8_t>> base64(std::string_view text) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        const int v = value(c);
        if (v < 0) return std::nullopt;
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

/// Percent-decoding for relative URIs ("my%20texture.png").
std::string uri_to_path(std::string_view uri) {
    std::string out;
    for (std::size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '%' && i + 2 < uri.size()) {
            out += static_cast<char>(std::stoi(std::string(uri.substr(i + 1, 2)), nullptr, 16));
            i += 2;
        } else {
            out += uri[i];
        }
    }
    return out;
}

struct Loader {
    std::filesystem::path base;
    Json doc;
    std::vector<std::vector<std::uint8_t>> buffers;
    ModelData model;
    std::string error;
    std::vector<int> image_srgb; // -1 unused, 0 linear, 1 srgb
    std::vector<int> alternate_image;
    std::size_t image_count = 0;

    bool fail(std::string message) {
        if (error.empty()) error = std::move(message);
        return false;
    }

    std::optional<std::vector<std::uint8_t>> load_uri(std::string_view uri) {
        if (uri.starts_with("data:")) {
            const std::size_t comma = uri.find(',');
            if (comma == std::string_view::npos || uri.substr(0, comma).find(";base64") == std::string_view::npos) return std::nullopt;
            return base64(uri.substr(comma + 1));
        }
        const std::filesystem::path p = base / uri_to_path(uri);
        model.files.push_back(p);
        return read_file(p);
    }

    bool load_buffers(const std::vector<std::uint8_t>* glb_bin) {
        const Json& list = doc["buffers"];
        for (std::size_t i = 0; i < list.size(); ++i) {
            const Json& b = list[i];
            if (const Json* uri = b.find("uri")) {
                auto data = load_uri(uri->as_string());
                if (!data) return fail("buffer " + std::to_string(i) + ": cannot read " + uri->as_string());
                buffers.push_back(std::move(*data));
            } else if (glb_bin && i == 0) {
                buffers.push_back(*glb_bin);
            } else {
                return fail("buffer " + std::to_string(i) + " has no data");
            }
            if (buffers.back().size() < static_cast<std::size_t>(b["byteLength"].as_number())) {
                return fail("buffer " + std::to_string(i) + " is shorter than its byteLength");
            }
        }
        return true;
    }

    /// Bytes of a buffer view (with its stride; 0 = tightly packed).
    bool view(std::size_t index, std::span<const std::uint8_t>& out, std::size_t& stride) {
        const Json& v = doc["bufferViews"][index];
        if (!v.is_object()) return fail("bufferView " + std::to_string(index) + " missing");
        const auto buffer = static_cast<std::size_t>(v["buffer"].as_number(-1));
        const auto offset = static_cast<std::size_t>(v["byteOffset"].as_number(0));
        const auto length = static_cast<std::size_t>(v["byteLength"].as_number(0));
        if (buffer >= buffers.size() || offset + length > buffers[buffer].size()) {
            return fail("bufferView " + std::to_string(index) + " out of range");
        }
        out = std::span<const std::uint8_t>(buffers[buffer]).subspan(offset, length);
        stride = static_cast<std::size_t>(v["byteStride"].as_number(0));
        return true;
    }

    /// Reads an accessor as floats (components per element) or integers.
    bool accessor(std::size_t index, std::size_t components, std::vector<float>* floats, std::vector<std::uint32_t>* ints) {
        const Json& a = doc["accessors"][index];
        if (!a.is_object()) return fail("accessor " + std::to_string(index) + " missing");
        if (a.find("sparse")) model.warnings.push_back("sparse accessors are not supported (accessor " + std::to_string(index) + ")");
        const auto count = static_cast<std::size_t>(a["count"].as_number(0));
        const int type = static_cast<int>(a["componentType"].as_number(0));
        const std::size_t size = type == GL_FLOAT || type == GL_UNSIGNED_INT ? 4 : type == GL_UNSIGNED_SHORT ? 2 : type == GL_UNSIGNED_BYTE ? 1 : 0;
        if (size == 0 || (floats && type != GL_FLOAT && !a["normalized"].as_bool())) {
            return fail("accessor " + std::to_string(index) + ": unsupported component type");
        }
        const Json* view_index = a.find("bufferView");
        if (!view_index) { // all zeros
            if (floats) floats->assign(count * components, 0.0f);
            if (ints) ints->assign(count * components, 0);
            return true;
        }
        std::span<const std::uint8_t> bytes;
        std::size_t stride = 0;
        if (!view(static_cast<std::size_t>(view_index->as_number()), bytes, stride)) return false;
        const auto offset = static_cast<std::size_t>(a["byteOffset"].as_number(0));
        const std::size_t element = size * components;
        if (stride == 0) stride = element;
        if (count > 0 && offset + (count - 1) * stride + element > bytes.size()) {
            return fail("accessor " + std::to_string(index) + " reads past its bufferView");
        }
        const bool normalized = a["normalized"].as_bool();
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t* e = bytes.data() + offset + i * stride;
            for (std::size_t c = 0; c < components; ++c) {
                const std::uint8_t* p = e + c * size;
                std::uint32_t u = 0;
                float f = 0;
                if (type == GL_FLOAT) std::memcpy(&f, p, 4);
                else if (type == GL_UNSIGNED_INT) std::memcpy(&u, p, 4);
                else if (type == GL_UNSIGNED_SHORT) { std::uint16_t s; std::memcpy(&s, p, 2); u = s; }
                else u = *p;
                if (floats) floats->push_back(type == GL_FLOAT ? f : (normalized ? static_cast<float>(u) / (size == 1 ? 255.0f : 65535.0f) : static_cast<float>(u)));
                if (ints) ints->push_back(u);
            }
        }
        return true;
    }

    int texture_image(const Json& texture_info, bool srgb) {
        if (!texture_info.is_object()) return -1;
        const Json& tex = doc["textures"][static_cast<std::size_t>(texture_info["index"].as_number(-1))];
        if (!tex.is_object() || !tex.find("source")) return -1;

        const int image = static_cast<int>(tex["source"].as_number(-1));
        if (image < 0 || static_cast<std::size_t>(image) >= image_srgb.size()) return -1;
        auto& color = image_srgb[static_cast<std::size_t>(image)];
        const int requested = srgb ? 1 : 0;
        if (color < 0) color = requested;
        if (color == requested) return image;
        auto& alternate = alternate_image[static_cast<std::size_t>(image)];
        if (alternate < 0) alternate = static_cast<int>(image_count++);
        return alternate;
    }

    ModelSampler texture_sampler(const Json& info) const {
        const Json& texture = doc["textures"][static_cast<std::size_t>(info["index"].as_number(-1))];
        const Json& sampler = doc["samplers"][static_cast<std::size_t>(texture["sampler"].as_number(-1))];
        const auto wrap = [](double value) -> std::uint8_t {
            return value == 33071 ? 0u : value == 33648 ? 2u : 1u;
        };
        const auto min = sampler["minFilter"].as_number(9987);
        const auto filter = min == 9728 ? 0 : min == 9729 ? 1 : min == 9984 ? 2 : min == 9985 ? 3 : min == 9986 ? 4 : 5;
        return {wrap(sampler["wrapS"].as_number()), wrap(sampler["wrapT"].as_number()),
                static_cast<std::uint8_t>(filter), sampler["magFilter"].as_number(9729) != 9728};
    }

    ModelTextureTransform texture_transform(const Json& info) {
        const Json& ext = info["extensions"]["KHR_texture_transform"];
        ModelTextureTransform result;
        const auto set = ext["texCoord"].as_number(info["texCoord"].as_number(0));
        if (set == 1) result.uv_set = 1;
        else if (set != 0) model.warnings.push_back("only TEXCOORD_0 and TEXCOORD_1 are supported");
        const auto pair = [](const Json& values, Vec2 fallback) {
            if (values.size() != 2) return fallback;
            return Vec2{static_cast<float>(values[0].as_number()), static_cast<float>(values[1].as_number())};
        };
        result.scale = pair(ext["scale"], {1, 1});
        result.offset = pair(ext["offset"], {});
        result.rotation = static_cast<float>(ext["rotation"].as_number());
        return result;
    }

    void load_materials() {
        image_count = doc["images"].size();
        image_srgb.assign(image_count, -1);
        alternate_image.assign(image_count, -1);
        for (const Json& m : doc["materials"].as_array()) {
            ModelMaterial mat;
            mat.name = m["name"].as_string();
            const Json& pbr = m["pbrMetallicRoughness"];
            if (pbr["baseColorFactor"].size() == 4) {
                const Json& c = pbr["baseColorFactor"];
                mat.base_color = {static_cast<float>(c[0].as_number()), static_cast<float>(c[1].as_number()),
                                  static_cast<float>(c[2].as_number()), static_cast<float>(c[3].as_number())};
            }
            mat.metallic = static_cast<float>(pbr["metallicFactor"].as_number(1.0));
            mat.roughness = static_cast<float>(pbr["roughnessFactor"].as_number(1.0));
            mat.base_color_image = texture_image(pbr["baseColorTexture"], true);
            mat.metallic_roughness_image = texture_image(pbr["metallicRoughnessTexture"], false);
            mat.normal_image = texture_image(m["normalTexture"], false);
            mat.normal_scale = static_cast<float>(m["normalTexture"]["scale"].as_number(1.0));
            mat.emissive_image = texture_image(m["emissiveTexture"], true);
            float strength = static_cast<float>(m["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"].as_number(1.0));
            if (m["emissiveFactor"].size() == 3) {
                const Json& e = m["emissiveFactor"];
                mat.emissive = Vec3{static_cast<float>(e[0].as_number()), static_cast<float>(e[1].as_number()),
                                    static_cast<float>(e[2].as_number())} * strength;
            }
            const auto alpha = m["alphaMode"].as_string();
            if (alpha == "MASK") mat.alpha_mode = ModelAlphaMode::Mask;
            else if (alpha == "BLEND") mat.alpha_mode = ModelAlphaMode::Blend;
            mat.alpha_cutoff = static_cast<float>(m["alphaCutoff"].as_number(0.5));
            mat.double_sided = m["doubleSided"].as_bool();
            mat.occlusion_image = texture_image(m["occlusionTexture"], false);
            mat.occlusion_strength = static_cast<float>(m["occlusionTexture"]["strength"].as_number(1.0));
            mat.texture_transforms = {texture_transform(pbr["baseColorTexture"]), texture_transform(pbr["metallicRoughnessTexture"]),
                                      texture_transform(m["normalTexture"]), texture_transform(m["emissiveTexture"]),
                                      texture_transform(m["occlusionTexture"])};
            mat.samplers = {texture_sampler(pbr["baseColorTexture"]), texture_sampler(pbr["metallicRoughnessTexture"]),
                            texture_sampler(m["normalTexture"]), texture_sampler(m["emissiveTexture"]),
                            texture_sampler(m["occlusionTexture"])};
            model.materials.push_back(std::move(mat));
        }
    }

    bool load_images() {
        const Json& list = doc["images"];
        model.images.resize(image_count);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const Json& img = list[i];
            const bool srgb = image_srgb[i] != 0;
            std::optional<std::vector<std::uint8_t>> bytes;
            std::string source;
            if (const Json* uri = img.find("uri")) {
                source = uri->as_string().starts_with("data:") ? "embedded #" + std::to_string(i) : uri->as_string();
                bytes = load_uri(uri->as_string());
            } else if (const Json* v = img.find("bufferView")) {
                std::span<const std::uint8_t> view_bytes;
                std::size_t stride = 0;
                if (!view(static_cast<std::size_t>(v->as_number()), view_bytes, stride)) return false;
                bytes.emplace(view_bytes.begin(), view_bytes.end());
                source = "embedded #" + std::to_string(i);
            }
            if (!bytes) return fail("image " + std::to_string(i) + ": cannot read data");
            std::string err;
            auto decoded = image_srgb[i] < 0 ? std::optional<ImageData>(ImageData{}) // unused: skip decoding
                                             : decode_image(*bytes, srgb, &err);
            if (!decoded) return fail("image " + source + ": " + err);
            if (alternate_image[i] >= 0) {
                ImageData alternate = *decoded;
                alternate.srgb = !srgb;
                model.images[static_cast<std::size_t>(alternate_image[i])] = {std::move(alternate), source};
            }
            model.images[i] = {std::move(*decoded), source};
        }
        return true;
    }

    bool load_meshes(std::vector<std::vector<std::uint32_t>>& mesh_primitives) {
        for (const Json& mesh : doc["meshes"].as_array()) {
            std::vector<std::uint32_t> pieces;
            if (mesh.find("weights")) model.warnings.push_back("morph targets are not supported");
            for (const Json& prim : mesh["primitives"].as_array()) {
                if (prim["mode"].as_number(4) != 4) {
                    model.warnings.push_back("only triangle primitives are supported");
                    continue;
                }
                const Json& attr = prim["attributes"];
                if (!attr.find("POSITION")) return fail("a primitive has no POSITION");
                std::vector<float> pos, nrm, uv, uv1;
                if (!accessor(static_cast<std::size_t>(attr["POSITION"].as_number()), 3, &pos, nullptr)) return false;
                if (attr.find("NORMAL") && !accessor(static_cast<std::size_t>(attr["NORMAL"].as_number()), 3, &nrm, nullptr)) return false;
                if (attr.find("TEXCOORD_0") && !accessor(static_cast<std::size_t>(attr["TEXCOORD_0"].as_number()), 2, &uv, nullptr)) return false;
                if (attr.find("TEXCOORD_1") && !accessor(static_cast<std::size_t>(attr["TEXCOORD_1"].as_number()), 2, &uv1, nullptr)) return false;
                if (uv1.empty() && prim.find("material")) {
                    const auto material = static_cast<std::size_t>(prim["material"].as_number(-1));
                    if (material < model.materials.size() &&
                        std::any_of(model.materials[material].texture_transforms.begin(),
                                    model.materials[material].texture_transforms.end(),
                                    [](const auto& transform) { return transform.uv_set == 1; })) {
                        model.warnings.push_back("material requests missing TEXCOORD_1; using TEXCOORD_0");
                    }
                }
                if (attr.find("JOINTS_0")) model.warnings.push_back("skinning is not supported");
                const std::size_t count = pos.size() / 3;
                ModelPrimitive p;
                p.material = static_cast<int>(prim["material"].as_number(-1));
                if (p.material >= static_cast<int>(model.materials.size())) p.material = -1;
                p.mesh.vertices.resize(count);
                for (std::size_t i = 0; i < count; ++i) {
                    Vertex& v = p.mesh.vertices[i];
                    v.position = {pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]};
                    if (nrm.size() == count * 3) v.normal = {nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]};
                    if (uv.size() == count * 2) v.uv = {uv[i * 2], uv[i * 2 + 1]};
                    v.uv1 = uv1.size() == count * 2 ? Vec2{uv1[i * 2], uv1[i * 2 + 1]} : v.uv;
                }
                if (prim.find("indices")) {
                    if (!accessor(static_cast<std::size_t>(prim["indices"].as_number()), 1, nullptr, &p.mesh.indices)) return false;
                    for (std::uint32_t idx : p.mesh.indices) {
                        if (idx >= count) return fail("index out of range");
                    }
                } else {
                    for (std::uint32_t i = 0; i < count; ++i) p.mesh.indices.push_back(i);
                }
                if (p.mesh.indices.size() % 3 != 0 || p.mesh.indices.empty()) return fail("primitive index count is not a multiple of 3");
                if (nrm.size() != count * 3) generate_normals(p.mesh);
                pieces.push_back(static_cast<std::uint32_t>(model.primitives.size()));
                model.primitives.push_back(std::move(p));
            }
            mesh_primitives.push_back(std::move(pieces));
        }
        return true;
    }

    static void generate_normals(MeshData& mesh) {
        for (Vertex& v : mesh.vertices) v.normal = {};
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            Vertex& a = mesh.vertices[mesh.indices[i]];
            Vertex& b = mesh.vertices[mesh.indices[i + 1]];
            Vertex& c = mesh.vertices[mesh.indices[i + 2]];
            const Vec3 n = cross(b.position - a.position, c.position - a.position); // area weighted
            a.normal += n;
            b.normal += n;
            c.normal += n;
        }
        for (Vertex& v : mesh.vertices) v.normal = normalize(v.normal);
    }

    bool load_nodes(const std::vector<std::vector<std::uint32_t>>& mesh_primitives) {
        const Json& list = doc["nodes"];
        for (std::size_t i = 0; i < list.size(); ++i) {
            const Json& n = list[i];
            ModelNode node;
            node.name = n["name"].as_string();
            if (n["matrix"].size() == 16) {
                Mat4 m;
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) m[c][r] = static_cast<float>(n["matrix"][static_cast<std::size_t>(c * 4 + r)].as_number());
                }
                node.translation = {m[3].x, m[3].y, m[3].z};
                node.scale = {length(Vec3{m[0].x, m[0].y, m[0].z}), length(Vec3{m[1].x, m[1].y, m[1].z}),
                              length(Vec3{m[2].x, m[2].y, m[2].z})};
                node.rotation = rotation_of(m);
            } else {
                const Json &t = n["translation"], &r = n["rotation"], &s = n["scale"];
                if (t.size() == 3) node.translation = {static_cast<float>(t[0].as_number()), static_cast<float>(t[1].as_number()), static_cast<float>(t[2].as_number())};
                if (r.size() == 4) node.rotation = {static_cast<float>(r[0].as_number()), static_cast<float>(r[1].as_number()), static_cast<float>(r[2].as_number()), static_cast<float>(r[3].as_number())};
                if (s.size() == 3) node.scale = {static_cast<float>(s[0].as_number()), static_cast<float>(s[1].as_number()), static_cast<float>(s[2].as_number())};
            }
            if (const Json* mesh = n.find("mesh")) {
                const auto m = static_cast<std::size_t>(mesh->as_number(-1));
                if (m >= mesh_primitives.size()) return fail("node " + std::to_string(i) + " references a missing mesh");
                node.primitives = mesh_primitives[m];
            }
            if (n.find("skin")) model.warnings.push_back("skins are not supported");
            for (const Json& c : n["children"].as_array()) {
                const auto child = static_cast<std::size_t>(c.as_number(-1));
                if (child >= list.size() || child == i) return fail("node " + std::to_string(i) + " has an invalid child");
                node.children.push_back(static_cast<std::uint32_t>(child));
            }
            model.nodes.push_back(std::move(node));
        }
        // Roots: the default scene's nodes, or (no scenes) every node nobody parents.
        const Json& scenes = doc["scenes"];
        if (scenes.size() > 0) {
            const Json& scene = scenes[static_cast<std::size_t>(doc["scene"].as_number(0))];
            for (const Json& r : scene["nodes"].as_array()) {
                const auto root = static_cast<std::size_t>(r.as_number(-1));
                if (root < model.nodes.size()) model.roots.push_back(static_cast<std::uint32_t>(root));
            }
        } else {
            std::vector<bool> child(model.nodes.size(), false);
            for (const auto& node : model.nodes) {
                for (auto c : node.children) child[c] = true;
            }
            for (std::uint32_t i = 0; i < model.nodes.size(); ++i) {
                if (!child[i]) model.roots.push_back(i);
            }
        }
        if (doc.find("animations")) model.warnings.push_back("animations are not supported");
        return true;
    }

    bool run(const std::filesystem::path& path) {
        base = path.parent_path();
        model.files.push_back(path);
        auto bytes = read_file(path);
        if (!bytes) return fail("cannot open " + path.string());
        std::optional<std::vector<std::uint8_t>> glb_bin;
        std::string_view json_text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        if (bytes->size() >= 12 && std::memcmp(bytes->data(), "glTF", 4) == 0) { // binary container
            std::uint32_t version = 0, total = 0;
            std::memcpy(&version, bytes->data() + 4, 4);
            std::memcpy(&total, bytes->data() + 8, 4);
            if (version != 2 || total > bytes->size()) return fail("not a glTF 2.0 binary");
            std::size_t offset = 12;
            json_text = {};
            while (offset + 8 <= total) {
                std::uint32_t length = 0, type = 0;
                std::memcpy(&length, bytes->data() + offset, 4);
                std::memcpy(&type, bytes->data() + offset + 4, 4);
                offset += 8;
                if (offset + length > total) return fail("truncated GLB chunk");
                if (type == 0x4E4F534A) json_text = {reinterpret_cast<const char*>(bytes->data() + offset), length}; // JSON
                else if (type == 0x004E4942) glb_bin.emplace(bytes->begin() + static_cast<std::ptrdiff_t>(offset), bytes->begin() + static_cast<std::ptrdiff_t>(offset + length)); // BIN
                offset += length;
            }
            if (json_text.empty()) return fail("GLB has no JSON chunk");
        }
        std::string parse_error;
        auto parsed = Json::parse(json_text, &parse_error);
        if (!parsed) return fail("invalid glTF JSON: " + parse_error);
        doc = std::move(*parsed);
        const std::string& version = doc["asset"]["version"].as_string();
        if (!version.starts_with("2.")) return fail("unsupported glTF version '" + version + "'");
        for (const Json& ext : doc["extensionsRequired"].as_array()) {
            if (ext.as_string() != "KHR_materials_emissive_strength" && ext.as_string() != "KHR_texture_transform") return fail("required extension not supported: " + ext.as_string());
        }
        std::vector<std::vector<std::uint32_t>> mesh_primitives;
        if (!load_buffers(glb_bin ? &*glb_bin : nullptr)) return false;
        load_materials();
        return load_images() && load_meshes(mesh_primitives) && load_nodes(mesh_primitives);
    }
};

} // namespace

std::optional<ModelData> load_gltf(const std::filesystem::path& path, std::string* error) {
    Loader loader;
    if (!loader.run(path)) {
        if (error) *error = path.filename().string() + ": " + loader.error;
        return std::nullopt;
    }
    return std::move(loader.model);
}

} // namespace astral
