#include "gltf_writer.hpp"

#include <astral/assets/asset_manager.hpp>
#include <doctest/doctest.h>

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

    touch_later([&] { test::write_gltf(dir, "quad", {.child_node = false}); }); // one node fewer
    assets.update();
    assets.update();
    CHECK(assets.reloads() == 2);
    CHECK(assets.model(id)->version == 2);
    CHECK(assets.model(id)->nodes.size() == 1);

    // A broken edit keeps the last good version.
    touch_later([&] { test::write_bytes(dir / "quad.gltf", {'{'}); });
    assets.update();
    assets.update();
    CHECK(assets.state(id) == AssetState::Ready);
    CHECK(assets.model(id)->nodes.size() == 1);
}
