#include "game.hpp"

using namespace astral;

namespace scene_viewer {

void ViewerGame::on_start(Context& ctx) {
    ctx.debug_show.lights = debug_;
    ctx.debug_show.physics = debug_;
    ctx.show_debug_ui = ui_; // F1 toggles it at any time
    load(ctx);
}

void ViewerGame::on_update(Context& ctx) {
    // The scene file itself: reload it whole when it changes on disk.
    const Document* doc = ctx.assets->document(ctx.assets->load_document(file_));
    if (debug_) {
        ctx.debug.grid({0, 0.01f, 1}, 6, 12, {1, 1, 1, 0.25f});
        ctx.world.each<const Name, const WorldTransform>([&](const Name&, const WorldTransform& w) { ctx.debug.axes(w.matrix, 0.3f); });
    }
    if (doc && doc->version != version_) {
        for (Entity e : loaded_) ctx.world.destroy(e); // their prefab parts go in the next frame
        load(ctx);
    }
}

void ViewerGame::load(Context& ctx) {
    const LoadResult r = ctx.load_scene(file_);
    loaded_ = r.entities;
    const Document* doc = ctx.assets->document(ctx.assets->load_document(file_));
    version_ = doc ? doc->version : 0;
    ctx.log.info("{}: {} entities, {} problems", file_, r.entities.size(), r.errors.size());
}

} // namespace scene_viewer
