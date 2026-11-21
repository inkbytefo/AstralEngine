// Opens a scene file (.scene) and shows it. Prefab and scene files reload while it runs: edit
// assets/prefabs/box.prefab in a text editor and every box in the room changes.
//
//   scene_viewer [path/to/level.scene]                       window
//   scene_viewer [scene] --capture out.ppm [--frames 48] [--width 1280] [--height 720]
//
// Without a path it opens the RGB room (showcase.scene): from assets/ while developing, from
// scene_viewer.apak once packaged (`cmake --build ... --target scene_viewer_package`).

#include <astral/astral.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

class SceneViewer final : public Game {
public:
    explicit SceneViewer(std::string file) : file_(std::move(file)) {}

    void on_start(Context& ctx) override { load(ctx); }

    void on_update(Context& ctx) override {
        // The scene file itself: reload it whole when it changes on disk.
        const Document* doc = ctx.assets->document(ctx.assets->load_document(file_));
        if (doc && doc->version != version_) {
            for (Entity e : loaded_) ctx.world.destroy(e); // their prefab parts go in the next frame
            load(ctx);
        }
    }

private:
    void load(Context& ctx) {
        const LoadResult r = ctx.load_scene(file_);
        loaded_ = r.entities;
        const Document* doc = ctx.assets->document(ctx.assets->load_document(file_));
        version_ = doc ? doc->version : 0;
        ctx.log.info("{}: {} entities, {} problems", file_, r.entities.size(), r.errors.size());
    }

    std::string file_;
    std::vector<Entity> loaded_;
    std::uint32_t version_ = 0;
};

bool write_ppm(const char* path, const std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", width, height);
    for (std::size_t p = 0; p < std::size_t{width} * height; ++p) std::fwrite(&rgba[p * 4], 1, 3, f);
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path scene;
    const char* out = nullptr;
    std::uint32_t frames = 48, width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
        else scene = argv[i];
    }
    // A scene given on the command line is read from its folder; otherwise the game's own assets.
    std::string root = ASTRAL_GAME_ASSETS, file = "showcase.scene";
    std::string_view pack = ASTRAL_GAME_PACK;
    if (!scene.empty()) {
        scene = std::filesystem::absolute(scene);
        root = scene.parent_path().string();
        file = scene.filename().string();
        pack = {};
    }

    RenderSettings render;
    render.bloom_strength = 0.05f;
    App app({.title = "Astral — scene viewer", .width = width, .height = height, .render = render,
             .asset_root = root, .pack = pack, .offscreen = out != nullptr});
    SceneViewer viewer(file);
    if (!out) return app.run(viewer);

    app.run_frames(viewer, frames);
    std::vector<std::uint8_t> rgba;
    if (!app.read_frame(rgba) || !write_ppm(out, rgba, width, height)) {
        std::fprintf(stderr, "capture failed (no GPU?)\n");
        return 1;
    }
    std::printf("wrote %s\n", out);
    return 0;
}
