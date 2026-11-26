// Workflow station (from material_workflow): .material parent inheritance, triplanar projection, GLB slot override,
// scene loading, hot reload from loose assets, and U toggling uv_inspect / texel density inspection.
#include "stations/stations.hpp"

#include <algorithm>
#include <string>

using namespace astral;

namespace materials_showcase {

void build_workflow(StationBuild& build) {
    Context& ctx = build.ctx;
    const LoadResult loaded = ctx.scenes().load_scene("workflow/workflow.scene");
    for (const std::string& error : loaded.errors) ctx.log.warn("workflow station scene: {}", error);
    build.owned.insert(build.owned.end(), loaded.entities.begin(), loaded.entities.end());

    for (const Entity e : loaded.entities) {
        if (const auto* pm = ctx.world.get<const PrimitiveMesh>(e)) {
            build.workflow.primitives.push_back({e, pm->material_file});
        }
        if (const auto* mr = ctx.world.get<const ModelRef>(e)) {
            ctx.assets->load_model(mr->path);
            build.workflow.models.push_back({e, mr->material_overrides});
        }
    }
    ctx.assets->wait_all();
}

void apply_workflow_uv_mode(Context& ctx, const WorkflowState& state, UvInspectMode mode) {
    const std::string_view inspect = uv_inspect_material(mode);
    for (const WorkflowPrimitiveRecord& record : state.primitives) {
        if (auto* pm = ctx.world.get<PrimitiveMesh>(record.entity)) {
            pm->material_file = inspect.empty() ? record.material_file : std::string(inspect);
        }
    }
    for (const WorkflowModelRecord& record : state.models) {
        auto* mr = ctx.world.get<ModelRef>(record.entity);
        if (!mr) continue;
        if (inspect.empty()) {
            mr->material_overrides = record.overrides;
            continue;
        }
        mr->material_overrides.clear();
        const AssetId id = ctx.assets->load_model(mr->path);
        const Model* model = ctx.assets->model(id);
        if (!model) continue;
        for (const std::string& slot : model->material_slots) {
            if (slot.empty()) continue;
            if (std::count(model->material_slots.begin(), model->material_slots.end(), slot) != 1) continue;
            mr->material_overrides.push_back({slot, std::string(inspect)});
        }
    }
    if (ctx.renderer) ctx.renderer->reset_history();
}

} // namespace materials_showcase
