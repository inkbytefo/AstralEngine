// Gallery station (from shader_showcase): .ashader night gallery with custom surface, vertex, sky and post shaders.
#include "stations/stations.hpp"

using namespace astral;

namespace materials_showcase {

void build_gallery(StationBuild& build) {
    Context& ctx = build.ctx;
    const LoadResult loaded = ctx.scenes().load_scene("gallery/gallery.scene");
    for (const std::string& error : loaded.errors) ctx.log.warn("gallery station scene: {}", error);
    build.owned.insert(build.owned.end(), loaded.entities.begin(), loaded.entities.end());

    ctx.sky = ctx.materials().load_material(gallery_sky_material);
    if (const MaterialHandle post = ctx.materials().load_material(gallery_post_material); post.valid()) {
        ctx.post.push_back(post);
    }
}

} // namespace materials_showcase
