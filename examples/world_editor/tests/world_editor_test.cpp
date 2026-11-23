// End-to-end E0 acceptance test (ADR-0035):
// In examples/world_editor, an AI agent proposes a prefab, the user reviews the field diff & isolated ECS preview,
// approves it as one atomic transaction, the scene reloads live with the new prefab, and undo/redo yields byte-identical files.
#include <astral/editor/workspace.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace astral;
using namespace astral::editor;

namespace {

std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::filesystem::path copy_workshop_assets() {
    const std::filesystem::path dst = std::filesystem::temp_directory_path() / "astral_world_editor_e0_test";
    std::error_code ec;
    std::filesystem::remove_all(dst, ec);
    std::filesystem::create_directories(dst, ec);
    std::filesystem::copy(std::filesystem::path(ASTRAL_EDITOR_ASSETS), dst,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    return dst;
}

} // namespace

TEST_CASE("world_editor AI prefab proposal -> review -> approve -> live reload -> byte-identical undo/redo") {
    const std::filesystem::path sandbox_assets = copy_workshop_assets();
    const std::filesystem::path scene_file = sandbox_assets / "scenes" / "workshop.scene";
    const std::filesystem::path generated_prefab = sandbox_assets / "prefabs" / "ai_crate.prefab";
    REQUIRE(std::filesystem::exists(scene_file));

    struct AcceptanceDriver final : Game {
        EditorWorkspace editor;
        std::filesystem::path scene_path;
        std::filesystem::path prefab_path;
        bool passed = false;

        AcceptanceDriver(std::filesystem::path root, std::filesystem::path scene, std::filesystem::path prefab)
            : editor({.asset_root = std::move(root), .initial_document = "scenes/workshop.scene"}),
              scene_path(std::move(scene)),
              prefab_path(std::move(prefab)) {}

        void on_start(Context& ctx) override {
            editor.on_start(ctx);
            REQUIRE(editor.session() != nullptr);
            REQUIRE(editor.session()->is_open());

            const std::string initial_canonical = serialize_document(editor.session()->document().root);
            CHECK(find_by_name(ctx.world, "Workshop Floor").valid());
            CHECK(find_by_name(ctx.world, "Workshop Lamp").valid());
            CHECK_FALSE(find_by_name(ctx.world, "AI Crate").valid());

            // 1. Agent proposes creating & placing a reusable prefab.
            std::string err;
            REQUIRE(editor.request_ai_proposal("Create reusable crate prefab", &err));
            REQUIRE(editor.review()->has_active());
            REQUIRE(editor.review()->preview() != nullptr);
            CHECK(editor.review()->preview()->valid);
            CHECK_FALSE(editor.review()->preview()->diff.empty());
            CHECK(editor.review()->preview()->entity_count == 3); // Floor + Lamp + AI Crate in temporary ECS preview

            // Before approval: live world and disk remain completely untouched.
            CHECK_FALSE(find_by_name(ctx.world, "AI Crate").valid());
            CHECK_FALSE(std::filesystem::exists(prefab_path));

            // 2. User approves -> single atomic transaction persists prefab + scene and reloads live ECS world.
            const TransactionResult approved = editor.approve_proposal(ctx);
            REQUIRE(approved.ok);
            CHECK(std::filesystem::exists(prefab_path));
            CHECK(find_by_name(ctx.world, "AI Crate").valid());
            const std::string approved_scene_bytes = read_all(scene_path);

            // 3. Undo & Redo produce byte-for-byte identical scene files and keep live ECS in sync.
            REQUIRE(editor.undo(ctx).ok);
            CHECK(read_all(scene_path) == initial_canonical);
            CHECK_FALSE(find_by_name(ctx.world, "AI Crate").valid());

            REQUIRE(editor.redo(ctx).ok);
            CHECK(read_all(scene_path) == approved_scene_bytes);
            CHECK(find_by_name(ctx.world, "AI Crate").valid());

            passed = true;
        }
        void on_update(Context& ctx) override { editor.on_update(ctx); }
        void on_shutdown(Context& ctx) override { editor.on_shutdown(ctx); }
    };

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = sandbox_assets.string(), .hot_reload = false});
    AcceptanceDriver driver(sandbox_assets, scene_file, generated_prefab);
    app.run_frames(driver, 2);
    CHECK(driver.passed);
}
