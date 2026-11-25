// Regression tests for the Master Stabilization Roadmap (docs/status/master-stabilization-roadmap.md).
#include "gltf_writer.hpp"

#include <astral/assets/cook.hpp>
#include <astral/assets/model.hpp>
#include <astral/core/bytes.hpp>
#include <astral/core/math.hpp>
#include <doctest/doctest.h>

#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

using namespace astral;

namespace {

/// Edits element `index` of the array `parent[key]` (Json arrays are only mutable by rebuilding).
void edit_item(Json& parent, const char* key, std::size_t index, const std::function<void(Json&)>& edit) {
    Json rebuilt = Json::array();
    const Json& list = parent[key];
    for (std::size_t i = 0; i < list.size(); ++i) {
        Json item = list[i];
        if (i == index) edit(item);
        rebuilt.push_back(std::move(item));
    }
    parent[key] = std::move(rebuilt);
}

/// Writes the test quad, lets `edit` change its JSON, and loads it again.
std::optional<ModelData> load_edited(const char* name, const std::function<void(Json&)>& edit, std::string* error) {
    const auto dir = test::temp_dir(name);
    const auto path = test::write_gltf(dir, "quad");
    std::ifstream stream(path);
    auto doc = Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    REQUIRE(doc);
    stream.close();
    edit(*doc);
    const auto text = doc->dump();
    test::write_bytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
    return load_gltf(path, error);
}

std::vector<std::uint8_t> amdl_header() {
    ByteWriter w;
    const std::uint8_t magic[4] = {'A', 'M', 'D', 'L'};
    w.bytes(magic);
    w.u32(3);
    return w.take();
}

} // namespace

TEST_CASE("A11: a cooked index count whose byte size wraps is rejected") {
    ByteWriter w;
    w.bytes(amdl_header());
    w.varint(1);                     // one primitive
    w.u32(0);                        // material
    w.varint(1);                     // one vertex
    for (int i = 0; i < 10; ++i) w.f32(0.0f);
    w.varint(std::uint64_t{1} << 62); // indices * 4 wraps to 0
    std::string error;
    std::optional<ModelData> model;
    CHECK_NOTHROW(model = read_cooked_model(w.take(), &error));
    CHECK_FALSE(model.has_value());
}

TEST_CASE("A12: a cooked record count larger than the file can hold is rejected before allocating") {
    for (int section = 0; section < 2; ++section) {
        ByteWriter w;
        w.bytes(amdl_header());
        if (section == 1) w.varint(0); // no primitives; the material count is damaged instead
        w.varint(std::uint64_t{1} << 27);
        std::string error;
        std::optional<ModelData> model;
        CHECK_NOTHROW(model = read_cooked_model(w.take(), &error));
        CHECK_FALSE(model.has_value());
    }
}

TEST_CASE("AS01: node cycles and nodes with two parents are rejected") {
    std::string error;
    // 0 -> 1 -> 0 (cycle longer than a self reference).
    auto cyclic = load_edited("msr_gltf_cycle", [](Json& doc) { edit_item(doc, "nodes", 1, [](Json& n) { n["children"] = Json::array({0}); }); }, &error);
    CHECK_FALSE(cyclic.has_value());
    CHECK(error.find("cycle") != std::string::npos);
    // Node 1 listed twice by node 0: two parents for the same node.
    auto duplicate = load_edited("msr_gltf_two_parents", [](Json& doc) { edit_item(doc, "nodes", 0, [](Json& n) { n["children"] = Json::array({1, 1}); }); }, &error);
    CHECK_FALSE(duplicate.has_value());
    CHECK(error.find("parent") != std::string::npos);
}

TEST_CASE("AS02: a mirrored node matrix keeps its negative determinant") {
    std::string error;
    auto model = load_edited("msr_gltf_mirror", [](Json& doc) {
        edit_item(doc, "nodes", 1, [](Json& node) {
            node.erase("translation");
            node.erase("rotation");
            node.erase("scale");
            node["matrix"] = Json::array({-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
        });
    }, &error);
    REQUIRE_MESSAGE(model.has_value(), error);
    const ModelNode& n = model->nodes[1];
    const Mat4 m = compose(n.translation, n.rotation, n.scale);
    CHECK(m[0].x == doctest::Approx(-1.0f));
    CHECK(m[1].y == doctest::Approx(1.0f));
    CHECK(m[2].z == doctest::Approx(1.0f));
}

TEST_CASE("AS03: bufferView and accessor ranges that wrap size_t are rejected") {
    std::string error;
    auto view = load_edited("msr_gltf_view_wrap", [](Json& doc) {
        edit_item(doc, "bufferViews", 0, [](Json& v) { v["byteOffset"] = 18446744073709549568.0; }); // offset + length wraps
    }, &error);
    CHECK_FALSE(view.has_value());
    auto accessor = load_edited("msr_gltf_accessor_wrap", [](Json& doc) {
        edit_item(doc, "accessors", 0, [](Json& a) { a["count"] = 4611686018427387904.0; }); // (count - 1) * stride wraps
    }, &error);
    CHECK_FALSE(accessor.has_value());
    auto stride = load_edited("msr_gltf_small_stride", [](Json& doc) {
        edit_item(doc, "bufferViews", 0, [](Json& v) { v["byteStride"] = 4; }); // smaller than a VEC3 element
    }, &error);
    CHECK_FALSE(stride.has_value());
}

TEST_CASE("AS04: negative, fractional and missing glTF indices are treated as absent, not cast") {
    std::string error;
    auto model = load_edited("msr_gltf_bad_indices", [](Json& doc) {
        edit_item(doc, "materials", 0, [](Json& m) {
            m["pbrMetallicRoughness"]["baseColorTexture"]["index"] = -3;
            m["normalTexture"]["index"] = 0.5;
        });
        edit_item(doc, "textures", 0, [](Json& t) { t.erase("sampler"); });
    }, &error);
    REQUIRE_MESSAGE(model.has_value(), error);
    CHECK(model->materials[0].base_color_image == -1);
    CHECK(model->materials[0].normal_image == -1);
}

TEST_CASE("AS05: a malformed percent escape in a URI fails the load instead of throwing") {
    std::string error;
    std::optional<ModelData> model;
    CHECK_NOTHROW(model = load_edited("msr_gltf_bad_uri", [](Json& doc) { edit_item(doc, "buffers", 0, [](Json& b) { b["uri"] = "quad%zz.bin"; }); }, &error));
    CHECK_FALSE(model.has_value());
}
