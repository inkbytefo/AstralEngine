#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/showcase_look/showcase_look.hpp"

#include <astral/app/feature.hpp>

namespace shader_showcase {

void ShowcaseGame::on_start(astral::Context& ctx) {
    astral::FeatureSet features;
    features.add("fly_camera", fly::install_fly_camera);
    features.add("showcase_look", install_showcase_look);
    if (!features.install(ctx)) {
        ctx.quit();
        return;
    }

    if (!ctx.scenes().load_scene("showcase.scene").errors.empty()) {
        ctx.quit();
    }
}

} // namespace shader_showcase
