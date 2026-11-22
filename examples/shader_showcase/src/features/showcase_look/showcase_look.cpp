#include "features/showcase_look/showcase_look.hpp"

#include <astral/app/feature.hpp>

namespace shader_showcase {

void install_showcase_look(astral::FeatureContext& feature) {
    astral::Context& ctx = feature.game;
    if (!ctx.renderer) return;

    astral::RenderSettings render = ctx.renderer->settings();
    render.bloom_strength = static_cast<float>(feature.number("bloom_strength", render.bloom_strength));
    ctx.renderer->set_settings(render);

    const auto sky_path = feature.text("sky_material", "");
    if (!sky_path.empty()) {
        ctx.sky = ctx.materials().load_material(sky_path);
    }

    const auto post_path = feature.text("post_material", "");
    if (!post_path.empty()) {
        const auto post = ctx.materials().load_material(post_path);
        if (post.valid()) {
            ctx.post.push_back(post);
        }
    }
}

} // namespace shader_showcase
