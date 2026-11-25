#include "gltf_writer.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <thread>

using namespace astral;

namespace {

ImageData gradient(std::uint32_t w, std::uint32_t h) {
    ImageData img{.width = w, .height = h, .rgba = {}, .srgb = true, .format = ImageFormat::RGBA8, .levels = {}};
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            img.rgba.push_back(static_cast<std::uint8_t>(x * 255 / (w - 1)));
            img.rgba.push_back(static_cast<std::uint8_t>(y * 255 / (h - 1)));
            img.rgba.push_back(static_cast<std::uint8_t>((x + y) * 127 / (w + h)));
            img.rgba.push_back(255);
        }
    }
    return img;
}

double psnr(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b, int channels = 3) {
    double se = 0;
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.size(); i += 4) {
        for (int c = 0; c < channels; ++c) {
            const double d = double(a[i + static_cast<std::size_t>(c)]) - double(b[i + static_cast<std::size_t>(c)]);
            se += d * d;
            ++n;
        }
    }
    const double mse = se / static_cast<double>(n);
    return mse == 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

void write_text(const std::filesystem::path& p, std::string_view text) {
    std::filesystem::create_directories(p.parent_path());
    test::write_bytes(p, std::vector<std::uint8_t>(text.begin(), text.end()));
}

/// Rewrites a file so its timestamp and content both change.
void touch_text(const std::filesystem::path& p, std::string_view text) {
    const auto before = std::filesystem::last_write_time(p);
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        write_text(p, text);
    } while (std::filesystem::last_write_time(p) == before);
}

} // namespace

TEST_CASE("cooked images: full mip chains, BC7 for colour, BC5 for normals, close to the source") {
    const ImageData src = gradient(64, 40); // not a multiple of 4 below some mips, not square
    const ImageData colour = cook_image(src, TextureUse::Color);
    CHECK(colour.format == ImageFormat::BC7);
    CHECK(colour.srgb);
    REQUIRE(colour.levels.size() == 7); // 64x40 .. 1x1
    CHECK(colour.levels[0].size() == 16 * 10 * 16);
    CHECK(colour.levels[6].size() == 16);
    const ImageData back = decompress_image(colour);
    REQUIRE(back.levels.size() == 7);
    CHECK(back.levels[0].size() == 64 * 40 * 4);
    CHECK(psnr(back.levels[0], src.rgba) > 38.0);

    ImageData flat = gradient(16, 16);
    for (std::size_t i = 0; i < flat.rgba.size(); i += 4) {
        flat.rgba[i] = 200;     // tilted along X
        flat.rgba[i + 1] = 128;
        flat.rgba[i + 2] = 232; // (0.57, 0, 0.82): unit length
    }
    const ImageData normal = cook_image(flat, TextureUse::Normal);
    CHECK(normal.format == ImageFormat::BC5);
    CHECK_FALSE(normal.srgb);
    const ImageData normal_back = decompress_image(normal);
    CHECK(std::abs(int(normal_back.levels[0][0]) - 200) <= 2);
    CHECK(std::abs(int(normal_back.levels[0][1]) - 128) <= 2);
    CHECK(std::abs(int(normal_back.levels[4][0]) - 200) <= 3); // the 1x1 mip keeps the direction

    const ImageData plain = cook_image(src, TextureUse::Data, {.compress = false});
    CHECK(plain.format == ImageFormat::RGBA8);
    CHECK(plain.levels[0] == src.rgba);
    CHECK(cook_image(colour, TextureUse::Color).levels == colour.levels); // already cooked: unchanged
}

TEST_CASE("cooked models round-trip and refuse damaged data") {
    const auto dir = test::temp_dir("cook_model");
    test::write_gltf(dir, "quad");
    std::string error;
    auto model = load_gltf(dir / "quad.gltf", &error);
    REQUIRE_MESSAGE(model, error);
    cook_model_images(*model);
    CHECK(model->images[0].image.format == ImageFormat::BC7); // base colour
    CHECK(model->images[1].image.format == ImageFormat::BC5); // normal map
    const std::vector<std::uint8_t> bytes = write_cooked_model(*model);
    CHECK(is_cooked_model(bytes));
    auto back = read_cooked_model(bytes, &error);
    REQUIRE_MESSAGE(back, error);
    CHECK(back->nodes.size() == model->nodes.size());
    CHECK(back->nodes[1].name == "top");
    CHECK(back->nodes[1].scale.x == 2.0f);
    CHECK(back->roots == model->roots);
    CHECK(back->primitives[0].mesh.indices == model->primitives[0].mesh.indices);
    CHECK(back->primitives[0].mesh.vertices[2].uv.x == model->primitives[0].mesh.vertices[2].uv.x);
    CHECK(back->materials[0].normal_scale == doctest::Approx(0.8f));
    CHECK(back->materials[0].emissive.x == doctest::Approx(4.0f));
    CHECK(back->images[1].image.levels == model->images[1].image.levels);
    CHECK(write_cooked_model(*back) == bytes);

    for (std::size_t cut : {std::size_t{3}, std::size_t{9}, bytes.size() / 2, bytes.size() - 1}) {
        CHECK_FALSE(read_cooked_model(std::span(bytes.data(), cut), &error));
    }
    std::vector<std::uint8_t> bad = bytes;
    bad[4] = 99; // version
    CHECK_FALSE(read_cooked_model(bad, &error));
}

TEST_CASE("cook_directory: models fold in their files, documents become binary, the rest is copied; reruns are incremental") {
    const auto src = test::temp_dir("cook_src");
    const auto out = test::temp_dir("cook_out");
    test::write_gltf(src / "props", "quad");
    write_text(src / "levels" / "arena.scene", R"({"format": "astral.scene", "version": 1, "entities": []})");
    write_text(src / "notes.txt", "hello");
    write_text(src / ".git" / "config", "ignored");
    write_text(src / ".hidden.txt", "ignored");
    JobSystem jobs(2);
    CookReport r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = &jobs, .logger = nullptr});
    REQUIRE(r.ok());
    CHECK(r.cooked == 2);
    CHECK(r.copied == 1);
    CHECK(r.up_to_date == 0);
    auto model = test::read_bytes(out / "props" / "quad.gltf");
    CHECK(is_cooked_model(model));
    CHECK(Json::is_binary(test::read_bytes(out / "levels" / "arena.scene")));
    CHECK(test::read_bytes(out / "notes.txt").size() == 5);
    CHECK_FALSE(std::filesystem::exists(out / "props" / "quad.bin"));       // folded into the model
    CHECK_FALSE(std::filesystem::exists(out / "props" / "quad_color.png"));
    CHECK_FALSE(std::filesystem::exists(out / ".git"));
    CHECK_FALSE(std::filesystem::exists(out / ".hidden.txt"));

    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK(r.ok());
    CHECK(r.cooked + r.copied == 0);
    CHECK(r.up_to_date == 3);

    // An image the model uses changed: the model cooks again. A removed file disappears from the output.
    touch_text(src / "props" / "quad_color.png", "");
    test::write_bytes(src / "props" / "quad_color.png", test::png(10, 200, 10));
    std::filesystem::remove(src / "notes.txt");
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK(r.ok());
    CHECK(r.cooked == 1);
    CHECK(r.up_to_date == 1);
    CHECK(r.removed == 1);
    CHECK_FALSE(std::filesystem::exists(out / "notes.txt"));
    CHECK(test::read_bytes(out / "props" / "quad.gltf") != model);

    // Other options cook everything again.
    r = cook_directory({.source = src, .output = out, .textures = {.compress = false}, .jobs = nullptr, .logger = nullptr});
    CHECK(r.cooked == 2);

    write_text(src / "broken.scene", "{ nope");
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].find("broken.scene") != std::string::npos);
}

TEST_CASE(".apak: every cooked file packs, reads back identically, and damage is detected") {
    const auto src = test::temp_dir("pack_src");
    const auto out = test::temp_dir("pack_cooked");
    test::write_gltf(src, "quad");
    write_text(src / "levels" / "arena.scene", R"({"format": "astral.scene", "version": 1, "entities": []})");
    write_text(src / "big.txt", std::string(10000, 'a')); // compresses well
    REQUIRE(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    const auto pack_file = test::temp_dir("pack_file") / "game.apak";
    std::string error;
    REQUIRE_MESSAGE(write_pack(out, pack_file, &error), error);

    auto pack = Pack::open(pack_file, &error);
    REQUIRE_MESSAGE(pack, error);
    CHECK(pack->entries().size() == 3); // the cook cache is not shipped
    for (const PackEntry& e : pack->entries()) {
        auto data = pack->read(e.path, &error);
        REQUIRE_MESSAGE(data, error);
        CHECK(*data == test::read_bytes(out / e.path));
    }
    REQUIRE(pack->find("BIG.TXT"));
    CHECK(pack->find("big.txt")->compressed);
    CHECK(pack->find("big.txt")->stored_size < 1000);
    CHECK_FALSE(pack->read("missing.txt", &error));

    // Damage: a flipped byte inside an entry fails its read; a cut file fails to open.
    auto bytes = test::read_bytes(pack_file);
    const PackEntry& scene = *pack->find("levels/arena.scene");
    bytes[scene.offset + 2] ^= 0x55;
    test::write_bytes(pack_file, bytes);
    auto damaged = Pack::open(pack_file, &error);
    REQUIRE(damaged);
    CHECK_FALSE(damaged->read("levels/arena.scene", &error));
    CHECK(error.find("damaged") != std::string::npos);
    bytes.resize(bytes.size() / 2);
    test::write_bytes(pack_file, bytes);
    CHECK_FALSE(Pack::open(pack_file, &error));
    CHECK_FALSE(Pack::open(src / "quad.gltf", &error));
}

TEST_CASE("the asset manager reads cooked assets from a pack or a loose cooked folder") {
    const auto src = test::temp_dir("am_pack_src");
    const auto out = test::temp_dir("am_pack_cooked");
    test::write_gltf(src / "props", "quad");
    write_text(src / "levels" / "arena.scene", R"({"format": "astral.scene", "version": 1, "entities": [{"id": 7}]})");
    REQUIRE(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    const auto pack_file = test::temp_dir("am_pack_file") / "game.apak";
    REQUIRE(write_pack(out, pack_file));

    for (const bool packed : {true, false}) {
        CAPTURE(packed);
        AssetManager assets({.root = packed ? std::filesystem::path("does-not-matter") : out, .hot_reload = false,
                             .pack = packed ? pack_file : std::filesystem::path()});
        CHECK(assets.from_pack() == packed);
        const AssetId model = assets.load_model("props/quad.gltf");
        assets.wait_all();
        REQUIRE_MESSAGE(assets.state(model) == AssetState::Ready, assets.error(model));
        CHECK(assets.model(model)->nodes.size() == 2);
        const AssetId doc = assets.load_document("levels/arena.scene");
        REQUIRE(assets.document(doc));
        CHECK(assets.document(doc)->json["entities"][0]["id"].as_number() == 7);
        CHECK(assets.load_model("props/missing.glb").valid());
        assets.wait_all();
        CHECK(assets.state(AssetId::from_path("props/missing.glb")) == AssetState::Failed);
    }
    AssetManager broken({.root = ".", .hot_reload = false, .pack = src / "nope.apak"});
    CHECK_FALSE(broken.from_pack());
    CHECK(broken.state(broken.load_document("levels/arena.scene")) == AssetState::Failed);
}

TEST_CASE("cook_directory: a .world and its entity folder become an index and cell files") {
    const auto src = test::temp_dir("cook_world_src");
    const auto out = test::temp_dir("cook_world_out");
    write_text(src / "maps" / "island.world",
               R"({"format":"astral.map","version":1,"grids":[{"name":"main","cell_size":100,"load_range":300}]})");
    auto entity = [&](int id, double x) {
        write_text(src / "maps" / "island.entities" / (std::to_string(id) + ".ent"),
                   R"({"format":"astral.entity","version":1,"entity":{"id":)" + std::to_string(id) +
                       R"(,"components":{"astral::Transform":{"position":[)" + std::to_string(x) + R"(,0,0]}}}})");
    };
    entity(1, 10);
    entity(2, 250);
    CookReport r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    REQUIRE(r.ok());
    CHECK(r.cooked == 1);
    CHECK(r.copied == 0); // entity files are folded into the cells
    CHECK_FALSE(std::filesystem::exists(out / "maps" / "island.entities"));
    auto index = Json::from_binary(test::read_bytes(out / "maps" / "island.world"));
    REQUIRE(index.has_value());
    CHECK((*index)["cells"].size() == 2);
    CHECK(std::filesystem::exists(out / "maps" / "island.cells" / "always.cell"));
    CHECK(std::filesystem::exists(out / "maps" / "island.cells" / "main_0_0.cell"));
    CHECK(std::filesystem::exists(out / "maps" / "island.cells" / "main_2_0.cell"));

    // Nothing changed: up to date. A new entity file: cooked again, stale cells removed.
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK(r.up_to_date == 1);
    CHECK(r.cooked == 0);
    std::filesystem::remove(src / "maps" / "island.entities" / "2.ent");
    entity(3, -50);
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    REQUIRE(r.ok());
    CHECK(r.cooked == 1);
    CHECK_FALSE(std::filesystem::exists(out / "maps" / "island.cells" / "main_2_0.cell"));
    CHECK(std::filesystem::exists(out / "maps" / "island.cells" / "main_-1_0.cell"));

    // A bad entity file is reported.
    write_text(src / "maps" / "island.entities" / "bad.ent", "{ nope");
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK_FALSE(r.ok());
}

TEST_CASE("cook_directory: .material becomes binary JSON; a shader that does not compile fails the cook with its line") {
    const auto src = test::temp_dir("cook_shader_src");
    const auto out = test::temp_dir("cook_shader_out");
    write_text(src / "shaders" / "ok.ashader", "shader_type surface;\nuniform vec3 c = vec3(1.0);\nvoid surface() { ALBEDO = c; }\n");
    write_text(src / "shaders" / "fog.ashader", "shader_type post;\nvoid post() { COLOR *= 0.9; }\n");
    write_text(src / "materials" / "a.material", R"({"format":"astral.material","version":1,"shader":"shaders/ok.ashader"})");
    CookReport r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    REQUIRE(r.ok());
    CHECK(Json::is_binary(test::read_bytes(out / "materials" / "a.material")));
    CHECK(test::read_bytes(out / "shaders" / "ok.ashader").size() > 20); // the source ships; the game compiles it

    write_text(src / "shaders" / "bad.ashader", "shader_type surface;\nvoid surface() {\n    ALBEDO = vec3(nope);\n}\n");
    write_text(src / "shaders" / "typo.ashader", "shader_type surfce;\n");
    r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK_FALSE(r.ok());
    bool line_reported = false, header_reported = false;
    for (const std::string& e : r.errors) {
        line_reported = line_reported || (e.find("bad.ashader:3") != std::string::npos && e.find("nope") != std::string::npos);
        header_reported = header_reported || e.find("typo.ashader:1") != std::string::npos;
    }
    CHECK(line_reported);
    CHECK(header_reported);
}

TEST_CASE("cook_directory: .luau goes through the script compiler when one is given, a failing script is reported with its name") {
    namespace fs = std::filesystem;
    const fs::path source = test::temp_dir("cook_scripts_src"), output = test::temp_dir("cook_scripts_out");
    const auto write = [&](const char* name, std::string_view text) {
        std::ofstream(source / name, std::ios::binary) << text;
    };
    write("good.luau", "print('hi')");
    write("bad.luau", "syntax error here");
    CookOptions options{.source = source, .output = output,
                        .compile_script = [](std::string_view name, const std::vector<std::uint8_t>& text, std::vector<std::uint8_t>& out, std::string* error) {
                            if (std::string(text.begin(), text.end()).find("error") != std::string::npos) {
                                *error = std::string(name) + ":1: bad script";
                                return false;
                            }
                            out = {'B', 'C'};
                            return true;
                        }};
    const CookReport report = cook_directory(options);
    REQUIRE(report.errors.size() == 1);
    CHECK(report.errors[0] == "bad.luau:1: bad script");
    CHECK(fs::file_size(output / "good.luau") == 2); // the compiled form, not the source
    options.compile_script = nullptr; // without a compiler: copied as source
    fs::remove(source / "bad.luau");
    options.output = test::temp_dir("cook_scripts_out2");
    CHECK(cook_directory(options).ok());
    CHECK(fs::file_size(options.output / "good.luau") == 11);
}

TEST_CASE("AS10: a deleted cell output is cooked again although its source did not change") {
    const auto src = test::temp_dir("cook_as10_src");
    const auto out = test::temp_dir("cook_as10_out");
    write_text(src / "maps" / "island.world",
               R"({"format":"astral.map","version":1,"grids":[{"name":"main","cell_size":100,"load_range":300}]})");
    write_text(src / "maps" / "island.entities" / "1.ent",
               R"({"format":"astral.entity","version":1,"entity":{"id":1,"components":{"astral::Transform":{"position":[10,0,0]}}}})");
    REQUIRE(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    REQUIRE(std::filesystem::exists(out / "maps" / "island.cells" / "main_0_0.cell"));
    std::filesystem::remove(out / "maps" / "island.cells" / "main_0_0.cell");
    const CookReport r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    CHECK(r.cooked == 1);
    CHECK(std::filesystem::exists(out / "maps" / "island.cells" / "main_0_0.cell"));
}

TEST_CASE("AS09: a source that fails to cook keeps its last good output") {
    const auto src = test::temp_dir("cook_as09_src");
    const auto out = test::temp_dir("cook_as09_out");
    write_text(src / "materials" / "a.material", R"({"format":"astral.material","version":1})");
    REQUIRE(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    REQUIRE(std::filesystem::exists(out / "materials" / "a.material"));
    write_text(src / "materials" / "a.material", "{ broken");
    CHECK_FALSE(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    CHECK(std::filesystem::exists(out / "materials" / "a.material")); // the last good one is not deleted
    // Fixed again: cooked again.
    write_text(src / "materials" / "a.material", R"({"format":"astral.material","version":1})");
    CHECK(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    // Source removed: now the output goes.
    std::filesystem::remove(src / "materials" / "a.material");
    CHECK(cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    CHECK_FALSE(std::filesystem::exists(out / "materials" / "a.material"));
}

TEST_CASE("AS08: an image both embedded in a model and named by a .material still ships as a file") {
    const auto src = test::temp_dir("cook_as08_src");
    const auto out = test::temp_dir("cook_as08_out");
    test::write_gltf(src / "props", "quad");
    write_text(src / "materials" / "painted.material",
               R"({"format":"astral.material","version":1,"textures":{"albedo":"props/quad_color.png"}})");
    const CookReport r = cook_directory({.source = src, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr});
    REQUIRE(r.ok());
    CHECK(std::filesystem::exists(out / "props" / "quad_color.png")); // the material opens it at run time
    CHECK_FALSE(std::filesystem::exists(out / "props" / "quad.bin"));  // still only inside the model
}
