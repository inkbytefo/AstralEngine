#include "gltf_writer.hpp"

#include <astral/assets/asset_manager.hpp>
#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <thread>

using namespace astral;

TEST_CASE("asset ids are stable and ignore case and separators") {
    CHECK(AssetId::from_path("Robots\\Robot.glb") == AssetId::from_path("robots/robot.glb"));
    CHECK(AssetId::from_path("./a.glb") == AssetId::from_path("a.glb"));
    CHECK_FALSE(AssetId::from_path("a.glb") == AssetId::from_path("b.glb"));
    CHECK(AssetId::from_path("x").valid());
}

TEST_CASE("models load asynchronously; the same path is loaded once") {
    const auto dir = test::temp_dir("assets_async");
    test::write_gltf(dir, "quad");
    JobSystem jobs(2);
    AssetManager assets({.root = dir, .jobs = &jobs, .hot_reload = false});
    const AssetId id = assets.load_model("quad.gltf");
    CHECK(assets.load_model("QUAD.gltf") == id);
    CHECK(assets.state(id) == AssetState::Loading);
    CHECK(assets.model(id) == nullptr);
    assets.wait_all();
    REQUIRE(assets.state(id) == AssetState::Ready);
    const Model* m = assets.model(id);
    REQUIRE(m != nullptr);
    CHECK(m->nodes.size() == 2);
    CHECK(m->nodes[0].pieces.size() == 1);
    CHECK_FALSE(m->nodes[0].pieces[0].mesh.valid()); // headless: data only, no GPU resources
    CHECK(assets.path(id) == "quad.gltf");

    const AssetId missing = assets.load_model("missing.glb");
    assets.wait_all();
    CHECK(assets.state(missing) == AssetState::Failed);
    CHECK(assets.error(missing).find("cannot open") != std::string_view::npos);
    CHECK(assets.state(AssetId::from_path("never-requested")) == AssetState::Unknown);
}

TEST_CASE("hot reload: edited files reload; same structure keeps the version, a new structure bumps it") {
    const auto dir = test::temp_dir("assets_reload");
    test::write_gltf(dir, "quad");
    AssetManager assets({.root = dir, .hot_reload = true, .reload_poll_seconds = 0.0f}); // synchronous loads
    const AssetId id = assets.load_model("quad.gltf");
    assets.update();
    REQUIRE(assets.state(id) == AssetState::Ready);
    CHECK(assets.model(id)->version == 1);

    auto touch_later = [&](auto&& write) {
        // File times can be coarse: make sure the new write gets a different timestamp.
        const auto before = std::filesystem::last_write_time(dir / "quad.gltf");
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            write();
        } while (std::filesystem::last_write_time(dir / "quad.gltf") == before);
    };
    touch_later([&] { test::write_gltf(dir, "quad", {.red = 10, .quad_size = 2.0f}); }); // same structure
    assets.update(); // notices the change, loads synchronously
    assets.update();
    CHECK(assets.reloads() == 1);
    CHECK(assets.model(id)->version == 1);

    touch_later([&] {
        const auto path = test::write_gltf(dir, "quad");
        std::ifstream stream(path);
        auto doc = Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
        REQUIRE(doc);
        const Json second_material = (*doc)["materials"][0];
        (*doc)["materials"].push_back(second_material);
        Json mesh = (*doc)["meshes"][0];
        Json primitive = mesh["primitives"][0];
        primitive["material"] = 1;
        mesh["primitives"] = Json::array({primitive});
        (*doc)["meshes"] = Json::array({mesh});
        Json root = (*doc)["nodes"][0];
        root["translation"] = Json::array({0, 3, 0});
        (*doc)["nodes"] = Json::array({root, (*doc)["nodes"][1]});
        const auto text = doc->dump();
        test::write_bytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
    });
    assets.update();
    assets.update();
    REQUIRE(assets.model(id)->version == 2);
    CHECK(assets.model(id)->nodes[0].translation.y == 3.0f);
    CHECK(assets.model(id)->nodes[0].pieces[0].material_index == 1);

    touch_later([&] { test::write_gltf(dir, "quad", {.child_node = false}); }); // one node fewer
    assets.update();
    assets.update();
    CHECK(assets.reloads() == 3);
    CHECK(assets.model(id)->version == 3);
    CHECK(assets.model(id)->nodes.size() == 1);

    // A broken edit keeps the last good version.
    touch_later([&] { test::write_bytes(dir / "quad.gltf", {'{'}); });
    assets.update();
    assets.update();
    CHECK(assets.state(id) == AssetState::Ready);
    CHECK(assets.model(id)->nodes.size() == 1);
}

TEST_CASE("documents load at once and hot-reload; a broken edit keeps the last good version") {
    const auto dir = test::temp_dir("assets_documents");
    auto write = [&](std::string_view text) {
        const auto path = dir / "level.scene";
        const auto before = std::filesystem::exists(path) ? std::filesystem::last_write_time(path) : std::filesystem::file_time_type{};
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            test::write_bytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
        } while (std::filesystem::last_write_time(path) == before);
    };
    write(R"({"name": "one"})");
    AssetManager assets({.root = dir, .hot_reload = true, .reload_poll_seconds = 0.0f});
    const AssetId id = assets.load_document("level.scene");
    REQUIRE(assets.state(id) == AssetState::Ready);
    REQUIRE(assets.document(id) != nullptr);
    CHECK(assets.document(id)->json["name"].as_string() == "one");
    CHECK(assets.document(id)->version == 1);
    CHECK(assets.load_document("LEVEL.scene") == id);

    write(R"({"name": "two"})");
    assets.update();
    CHECK(assets.document(id)->json["name"].as_string() == "two");
    CHECK(assets.document(id)->version == 2);

    write("{ broken");
    assets.update();
    CHECK(assets.document(id)->json["name"].as_string() == "two");
    CHECK(assets.document(id)->version == 2);

    const AssetId missing = assets.load_document("nope.prefab");
    CHECK(assets.state(missing) == AssetState::Failed);
    CHECK(assets.document(missing) == nullptr);
    CHECK_FALSE(assets.error(missing).empty());
}
