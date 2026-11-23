#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "gltf_writer.hpp"

#include <astral/assets/model.hpp>
#include <astral/assets/material_export.hpp>
#include <astral/assets/cook.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {
void check_model(const ModelData& m) {
    REQUIRE(m.primitives.size() == 1);
    CHECK(m.primitives[0].mesh.vertices.size() == 4);
    CHECK(m.primitives[0].mesh.indices.size() == 6);
    CHECK(m.primitives[0].mesh.vertices[2].uv.x == 1.0f);
    CHECK(m.primitives[0].mesh.vertices[0].normal.z == doctest::Approx(1.0f));
    REQUIRE(m.materials.size() == 1);
    const ModelMaterial& mat = m.materials[0];
    CHECK(mat.name == "painted");
    CHECK(mat.metallic == 0.25f);
    CHECK(mat.roughness == 0.5f);
    CHECK(mat.normal_scale == doctest::Approx(0.8f));
    CHECK(mat.emissive.x == doctest::Approx(4.0f)); // emissiveFactor x KHR_materials_emissive_strength
    CHECK(mat.emissive.y == doctest::Approx(2.0f));
    CHECK(mat.base_color_image == 0);
    CHECK(mat.normal_image == 1);
    REQUIRE(m.images.size() == 2);
    CHECK(m.images[0].image.srgb);        // used as colour
    CHECK_FALSE(m.images[1].image.srgb);  // used as normal map: linear data
    CHECK(m.images[0].image.width == 4);
    CHECK(m.images[0].image.rgba[0] == 200);
    REQUIRE(m.nodes.size() == 2);
    CHECK(m.nodes[0].name == "base");
    CHECK(m.nodes[0].translation.y == 1.0f);
    CHECK(m.nodes[0].children == std::vector<std::uint32_t>{1});
    CHECK(m.nodes[1].scale.x == 2.0f);
    CHECK(m.nodes[1].primitives == std::vector<std::uint32_t>{0}); // mesh shared by both nodes
    CHECK(m.roots == std::vector<std::uint32_t>{0});
    CHECK(m.warnings.empty());
}
} // namespace

TEST_CASE("a .gltf with external buffer and images loads completely") {
    const auto dir = test::temp_dir("gltf_external");
    std::string error;
    const auto m = load_gltf(test::write_gltf(dir, "quad"), &error);
    REQUIRE_MESSAGE(m.has_value(), error);
    check_model(*m);
    CHECK(m->files.size() == 4); // gltf, bin, two images: watched for hot reload
}

TEST_CASE("a .glb with embedded images loads the same model") {
    const auto dir = test::temp_dir("gltf_glb");
    std::string error;
    const auto m = load_gltf(test::write_glb(dir, "quad"), &error);
    REQUIRE_MESSAGE(m.has_value(), error);
    check_model(*m);
    CHECK(m->files.size() == 1);
}

TEST_CASE("one glTF image can be both color and linear data") {
    const auto dir = test::temp_dir("gltf_image_roles");
    const auto path = test::write_gltf(dir, "quad");
    std::ifstream stream(path);
    auto doc = Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    REQUIRE(doc);
    Json material = (*doc)["materials"][0];
    material["normalTexture"]["index"] = 0;
    (*doc)["materials"] = Json::array({material});
    const auto text = doc->dump();
    test::write_bytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
    const auto model = load_gltf(path);
    REQUIRE(model);
    const auto& mat = model->materials[0];
    REQUIRE(mat.base_color_image != mat.normal_image);
    CHECK(model->images[mat.base_color_image].image.srgb);
    CHECK_FALSE(model->images[mat.normal_image].image.srgb);
    CHECK(model->images[mat.base_color_image].image.rgba == model->images[mat.normal_image].image.rgba);
    const auto cooked = read_cooked_model(write_cooked_model(*model));
    REQUIRE(cooked);
    CHECK(cooked->images[mat.base_color_image].image.srgb);
    CHECK_FALSE(cooked->images[mat.normal_image].image.srgb);
}

TEST_CASE("missing normals are generated") {
    const auto dir = test::temp_dir("gltf_normals");
    const auto m = load_gltf(test::write_gltf(dir, "quad", {.normals = false}));
    REQUIRE(m.has_value());
    for (const auto& v : m->primitives[0].mesh.vertices) CHECK(v.normal.z == doctest::Approx(1.0f));
}

TEST_CASE("broken files fail with a reason instead of crashing") {
    const auto dir = test::temp_dir("gltf_broken");
    std::string error;
    CHECK_FALSE(load_gltf(dir / "nope.gltf", &error).has_value());
    CHECK(error.find("cannot open") != std::string::npos);

    test::write_bytes(dir / "bad.gltf", {'{', '"', 'a'});
    CHECK_FALSE(load_gltf(dir / "bad.gltf", &error).has_value());
    CHECK(error.find("invalid glTF JSON") != std::string::npos);

    const auto path = test::write_gltf(dir, "quad");
    std::filesystem::remove(dir / "quad.bin");
    CHECK_FALSE(load_gltf(path, &error).has_value());
    CHECK(error.find("buffer 0") != std::string::npos);

    test::write_bytes(dir / "v1.gltf", {'{', '"', 'a', 's', 's', 'e', 't', '"', ':', '{', '"', 'v', 'e', 'r', 's', 'i', 'o', 'n', '"', ':', '"', '1', '.', '0', '"', '}', '}'});
    CHECK_FALSE(load_gltf(dir / "v1.gltf", &error).has_value());
    CHECK(error.find("version") != std::string::npos);
}

TEST_CASE("images decode from memory; garbage is refused") {
    const auto bytes = test::png(10, 20, 30, 2);
    const auto img = decode_image(bytes, true);
    REQUIRE(img.has_value());
    CHECK(img->width == 2);
    CHECK(img->rgba[1] == 20);
    const std::vector<std::uint8_t> junk{1, 2, 3, 4};
    std::string error;
    CHECK_FALSE(decode_image(junk, true, &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("embedded glTF material extraction writes reusable PBR assets without replacing files") {
    const auto root = test::temp_dir("gltf_material_export");
    test::write_glb(root, "quad");
    std::string error;
    const auto material = export_model_material(root, "quad.glb", 0, "materials/painted", &error);
    REQUIRE_MESSAGE(material, error);
    CHECK(*material == "materials/painted/surface.material");
    std::ifstream stream(root / *material);
    const auto doc = Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    REQUIRE(doc);
    CHECK((*doc)["type"].as_string() == "pbr");
    const std::string image_path = (*doc)["values"]["base_color_texture"].as_string();
    const auto image = load_image(root / image_path, true, &error);
    REQUIRE_MESSAGE(image, error);
    CHECK(image->rgba[0] == 200);
    CHECK_FALSE(export_model_material(root, "quad.glb", 0, "materials/painted", &error));
}

TEST_CASE("glTF alpha sidedness occlusion and samplers survive cooking") {
    const auto dir = test::temp_dir("gltf_surface_features");
    const auto model = load_gltf(test::write_glb(dir, "mask", {.alpha_mode = "MASK", .alpha = 0.2f,
                                                             .gltf_sampler = true, .double_sided = true}));
    REQUIRE(model);
    const auto cooked = read_cooked_model(write_cooked_model(*model));
    REQUIRE(cooked);
    const auto& mat = cooked->materials[0];
    CHECK(mat.alpha_mode == ModelAlphaMode::Mask);
    CHECK(mat.alpha_cutoff == doctest::Approx(0.6));
    CHECK(mat.double_sided);
    REQUIRE(mat.occlusion_image >= 0);
    CHECK(mat.occlusion_strength == doctest::Approx(0.75));
    CHECK(mat.samplers[0] == ModelSampler{0, 2, 4, false});
    CHECK_FALSE(cooked->images[static_cast<std::size_t>(mat.occlusion_image)].image.srgb);
}
