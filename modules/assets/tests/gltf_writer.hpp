#pragma once
// Writes small glTF test files (no binary fixtures in the repo): a textured unit quad mesh used by
// two nodes (root + translated child), one material with base colour and normal textures.
#include <astral/core/json.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace astral::test {

inline std::vector<std::uint8_t> png(std::uint8_t r, std::uint8_t g, std::uint8_t b, int size = 4) {
    std::vector<std::uint8_t> pixels;
    for (int i = 0; i < size * size; ++i) pixels.insert(pixels.end(), {r, g, b, 255});
    std::vector<std::uint8_t> out;
    stbi_write_png_to_func([](void* ctx, void* data, int n) {
        auto* v = static_cast<std::vector<std::uint8_t>*>(ctx);
        v->insert(v->end(), static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + n);
    }, &out, size, size, 4, pixels.data(), size * 4);
    return out;
}

inline void write_bytes(const std::filesystem::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

struct GltfOptions {
    std::uint8_t red = 200;       ///< Base colour texture colour (red channel).
    float quad_size = 1.0f;
    bool child_node = true;
    bool normals = true;
};

/// Geometry buffer: positions, (normals), uvs, uint16 indices of one quad in the XY plane.
inline std::vector<std::uint8_t> quad_buffer(const GltfOptions& o, std::size_t& pos_off, std::size_t& nrm_off,
                                             std::size_t& uv_off, std::size_t& idx_off) {
    const float h = o.quad_size * 0.5f;
    const float pos[] = {-h, -h, 0, h, -h, 0, h, h, 0, -h, h, 0};
    const float nrm[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    const float uv[] = {0, 1, 1, 1, 1, 0, 0, 0};
    const std::uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    std::vector<std::uint8_t> buf;
    auto put = [&](const void* data, std::size_t n) {
        const std::size_t off = buf.size();
        buf.insert(buf.end(), static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + n);
        while (buf.size() % 4) buf.push_back(0);
        return off;
    };
    pos_off = put(pos, sizeof(pos));
    nrm_off = o.normals ? put(nrm, sizeof(nrm)) : 0;
    uv_off = put(uv, sizeof(uv));
    idx_off = put(idx, sizeof(idx));
    return buf;
}

/// Builds the JSON; `buffer_uri` empty = GLB BIN chunk; images by uri or by buffer view.
inline Json quad_json(const GltfOptions& o, std::size_t buf_len, std::size_t pos_off, std::size_t nrm_off, std::size_t uv_off,
                      std::size_t idx_off, const std::string& buffer_uri, const std::vector<Json>& images,
                      const std::vector<Json>& extra_views) {
    Json doc = Json::object();
    doc["asset"]["version"] = "2.0";
    doc["scene"] = 0;
    Json b = Json::object();
    b["byteLength"] = static_cast<std::uint64_t>(buf_len);
    if (!buffer_uri.empty()) b["uri"] = buffer_uri;
    doc["buffers"] = Json::array({b});
    auto view = [](std::size_t off, std::size_t len) {
        Json v = Json::object();
        v["buffer"] = 0;
        v["byteOffset"] = static_cast<std::uint64_t>(off);
        v["byteLength"] = static_cast<std::uint64_t>(len);
        return v;
    };
    Json views = Json::array({view(pos_off, 48), view(uv_off, 32), view(idx_off, 12)});
    if (o.normals) views.push_back(view(nrm_off, 48));
    for (const Json& v : extra_views) views.push_back(v);
    doc["bufferViews"] = views;
    auto accessor = [](int view_index, int type, int count, const char* kind) {
        Json a = Json::object();
        a["bufferView"] = view_index;
        a["componentType"] = type;
        a["count"] = count;
        a["type"] = kind;
        return a;
    };
    Json accessors = Json::array({accessor(0, 5126, 4, "VEC3"), accessor(1, 5126, 4, "VEC2"), accessor(2, 5123, 6, "SCALAR")});
    if (o.normals) accessors.push_back(accessor(3, 5126, 4, "VEC3"));
    doc["accessors"] = accessors;
    Json attributes = Json::object();
    attributes["POSITION"] = 0;
    attributes["TEXCOORD_0"] = 1;
    if (o.normals) attributes["NORMAL"] = 3;
    Json prim = Json::object();
    prim["attributes"] = attributes;
    prim["indices"] = 2;
    prim["material"] = 0;
    Json mesh = Json::object();
    mesh["primitives"] = Json::array({prim});
    doc["meshes"] = Json::array({mesh});
    Json mat = Json::object();
    mat["name"] = "painted";
    mat["pbrMetallicRoughness"]["baseColorTexture"]["index"] = 0;
    mat["pbrMetallicRoughness"]["metallicFactor"] = 0.25;
    mat["pbrMetallicRoughness"]["roughnessFactor"] = 0.5;
    mat["normalTexture"]["index"] = 1;
    mat["normalTexture"]["scale"] = 0.8;
    mat["emissiveFactor"] = Json::array({1, 0.5, 0});
    mat["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"] = 4;
    doc["materials"] = Json::array({mat});
    Json t0 = Json::object(), t1 = Json::object();
    t0["source"] = 0;
    t1["source"] = 1;
    doc["textures"] = Json::array({t0, t1});
    Json imgs = Json::array();
    for (const Json& i : images) imgs.push_back(i);
    doc["images"] = imgs;
    Json root = Json::object();
    root["name"] = "base";
    root["mesh"] = 0;
    root["translation"] = Json::array({0, 1, 0});
    Json nodes = Json::array({root});
    if (o.child_node) {
        Json child = Json::object();
        child["name"] = "top";
        child["mesh"] = 0;
        child["translation"] = Json::array({0, 0, 2});
        child["scale"] = Json::array({2, 2, 2});
        Json root_with_child = root;
        root_with_child["children"] = Json::array({1});
        nodes = Json::array({root_with_child, child});
    }
    doc["nodes"] = nodes;
    Json scene = Json::object();
    scene["nodes"] = Json::array({0});
    doc["scenes"] = Json::array({scene});
    return doc;
}

/// dir/name.gltf + name.bin + name_color.png + name_normal.png
inline std::filesystem::path write_gltf(const std::filesystem::path& dir, const std::string& name, const GltfOptions& o = {}) {
    std::size_t p, n, u, i;
    const auto buf = quad_buffer(o, p, n, u, i);
    write_bytes(dir / (name + ".bin"), buf);
    write_bytes(dir / (name + "_color.png"), png(o.red, 40, 30));
    write_bytes(dir / (name + "_normal.png"), png(128, 128, 255));
    Json c = Json::object(), nm = Json::object();
    c["uri"] = name + "_color.png";
    nm["uri"] = name + "_normal.png";
    const Json doc = quad_json(o, buf.size(), p, n, u, i, name + ".bin", {c, nm}, {});
    const std::string text = doc.dump(2);
    write_bytes(dir / (name + ".gltf"), std::vector<std::uint8_t>(text.begin(), text.end()));
    return dir / (name + ".gltf");
}

/// The same model as one .glb with the images embedded in the BIN chunk.
inline std::filesystem::path write_glb(const std::filesystem::path& dir, const std::string& name, const GltfOptions& o = {}) {
    std::size_t p, n, u, i;
    auto buf = quad_buffer(o, p, n, u, i);
    std::vector<Json> images, views;
    for (const auto& image : {png(o.red, 40, 30), png(128, 128, 255)}) {
        const std::size_t off = buf.size();
        buf.insert(buf.end(), image.begin(), image.end());
        while (buf.size() % 4) buf.push_back(0);
        Json v = Json::object();
        v["buffer"] = 0;
        v["byteOffset"] = static_cast<std::uint64_t>(off);
        v["byteLength"] = static_cast<std::uint64_t>(image.size());
        views.push_back(v);
        Json img = Json::object();
        img["bufferView"] = static_cast<std::uint64_t>((o.normals ? 4 : 3) + images.size());
        img["mimeType"] = "image/png";
        images.push_back(img);
    }
    std::string json = quad_json(o, buf.size(), p, n, u, i, "", images, views).dump();
    while (json.size() % 4) json += ' ';
    std::vector<std::uint8_t> glb;
    auto u32 = [&](std::uint32_t v) {
        for (int k = 0; k < 4; ++k) glb.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
    };
    u32(0x46546C67); // "glTF"
    u32(2);
    u32(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + buf.size()));
    u32(static_cast<std::uint32_t>(json.size()));
    u32(0x4E4F534A);
    glb.insert(glb.end(), json.begin(), json.end());
    u32(static_cast<std::uint32_t>(buf.size()));
    u32(0x004E4942);
    glb.insert(glb.end(), buf.begin(), buf.end());
    write_bytes(dir / (name + ".glb"), glb);
    return dir / (name + ".glb");
}

/// A fresh empty directory under the system temp dir.
inline std::filesystem::path temp_dir(const std::string& name) {
    const auto dir = std::filesystem::temp_directory_path() / ("astral_" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace astral::test
