#include "game.hpp"

namespace material_workflow {

void WorkflowGame::on_start(astral::Context& ctx) {
    ctx.scenes().load_scene("materials.scene");
    ctx.world.each<const astral::ModelRef>([&](const astral::ModelRef& model) {
        ctx.assets->load_model(model.path);
    });
    ctx.assets->wait_all();
}

} // namespace material_workflow
