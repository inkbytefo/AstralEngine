// The water and wind station (the lake in the middle): light crates float, stones sink, beach balls drift in the wind,
// a plank bridge on hinges crosses the lake, a jump pad (a sensor with a Luau rule) throws what lands on it, a dropper
// feeds it, and a glider rides the wind (Airfoil).
#include "stations/stations.hpp"

#include "features/playground/playground.hpp"

#include <vector>

using namespace astral;

namespace physics_showcase {

namespace {

void build_floaters(Context& ctx) {
    World& w = ctx.world;
    const BodyDesc crate{.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}), .friction = 0.7f, .surface = 2};
    const MaterialHandle crate_mat = ctx.materials().create_material({.base_color = {0.85f, 0.6f, 0.3f, 1}, .roughness = 0.6f});
    const MaterialHandle stone_mat = ctx.materials().create_material({.base_color = {0.45f, 0.45f, 0.5f, 1}, .roughness = 0.5f, .metallic = 0.6f});
    for (int i = 0; i < 6; ++i) {
        BodyDesc body = crate;
        body.mass = 90.0f; // lighter than the water it displaces: floats
        w.create(Transform{.position = {-6.0f + static_cast<float>(i) * 1.5f, 3.0f + static_cast<float>(i % 3), -4.0f}, .scale = {0.6f, 0.6f, 0.6f}},
                 MeshRenderer{ctx.meshes.cube, crate_mat}, RigidBody{.desc = body},
                 Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f}, AirDrag{.area = 0.36f, .drag = 1.0f});
    }
    for (int i = 0; i < 3; ++i) {
        BodyDesc body = crate;
        body.mass = 420.0f; // denser than water: sinks
        w.create(Transform{.position = {4.0f + static_cast<float>(i), 4.0f, 3.0f}, .scale = {0.6f, 0.6f, 0.6f}},
                 MeshRenderer{ctx.meshes.cube, stone_mat}, RigidBody{.desc = body},
                 Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f});
    }
    for (int i = 0; i < 5; ++i) {
        w.create(Transform{.position = {-3.0f + static_cast<float>(i) * 1.8f, 2.0f, 6.0f}, .scale = {0.8f, 0.8f, 0.8f}},
                 MeshRenderer{ctx.meshes.sphere, ctx.materials().create_material({.base_color = {0.95f, 0.3f + 0.12f * static_cast<float>(i), 0.25f, 1}, .roughness = 0.4f})},
                 RigidBody{.desc = {.shape = ShapeDesc::sphere(0.4f), .mass = 1.2f, .restitution = 0.6f, .linear_damping = 0.02f, .surface = 2}},
                 Buoyant{.volume = 0.0f, .height = 0.8f, .area = 0.5f, .drag = 0.8f}, AirDrag{.area = 0.5f, .drag = 0.47f});
    }
}

void build_bridge(Context& ctx) {
    World& w = ctx.world;
    // Each plank is hinged to the next, both ends to the shore.
    const float bridge_z = 17.0f, bridge_y = terrain_height(-14.0f, bridge_z) + 0.4f;
    constexpr int planks = 14;
    const float plank = 2.0f;
    const float start_x = -0.5f * plank * planks;
    const MaterialHandle plank_mat = ctx.materials().create_material({.base_color = {0.5f, 0.35f, 0.2f, 1}, .roughness = 0.8f});
    std::vector<Entity> chain;
    chain.reserve(planks);
    for (int i = 0; i < planks; ++i) {
        chain.push_back(w.create(Transform{.position = {start_x + (static_cast<float>(i) + 0.5f) * plank, bridge_y, bridge_z}, .scale = {plank, 0.15f, 1.8f}},
                                 MeshRenderer{ctx.meshes.cube, plank_mat},
                                 RigidBody{.desc = {.shape = ShapeDesc::box({plank * 0.5f, 0.075f, 0.9f}), .mass = 20.0f, .friction = 1.0f, .surface = 2}}));
    }
    w.create(Joint{.type = JointType::Hinge, .b = chain.front(), .anchor = {start_x, bridge_y, bridge_z}, .axis = {0, 0, 1}});
    for (int i = 0; i + 1 < planks; ++i) {
        w.create(Joint{.type = JointType::Hinge, .a = chain[static_cast<std::size_t>(i)], .b = chain[static_cast<std::size_t>(i + 1)],
                       .anchor = {start_x + static_cast<float>(i + 1) * plank, bridge_y, bridge_z}, .axis = {0, 0, 1}});
    }
    w.create(Joint{.type = JointType::Hinge, .b = chain.back(), .anchor = {start_x + plank * planks, bridge_y, bridge_z}, .axis = {0, 0, 1}});
}

void build_pad_and_glider(Context& ctx) {
    World& w = ctx.world;
    const float pad_x = 24.0f, pad_y = terrain_height(pad_x, 0.0f);
    w.create(Name{"pad"}, Transform{.position = {pad_x, pad_y + 0.2f, 0}, .scale = {2.4f, 0.3f, 2.4f}},
             MeshRenderer{ctx.meshes.cube, ctx.materials().create_material({.base_color = {0.1f, 0.9f, 0.5f, 1}, .roughness = 0.3f, .emissive = {0.1f, 1.2f, 0.5f}})},
             RigidBody{.desc = {.shape = ShapeDesc::box({1.2f, 0.6f, 1.2f}), .motion = MotionType::Static, .sensor = true}},
             playground::JumpPad{.speed = 14.0f});
    w.create(Name{"dropper"}, Transform{.position = {pad_x, pad_y + 7.0f, 0}}, playground::Dropper{.interval = 3.0f, .timer = 2.0f});
    w.create(Name{"glider"}, Transform{.position = {-30.0f, 14.0f, 10.0f}, .rotation = Quat::from_axis_angle({1, 0, 0}, radians(5.0f)), .scale = {2.4f, 0.08f, 0.9f}},
             MeshRenderer{ctx.meshes.cube, ctx.materials().create_material({.base_color = {0.95f, 0.95f, 0.9f, 1}, .roughness = 0.6f})},
             RigidBody{.desc = {.shape = ShapeDesc::box({1.2f, 0.04f, 0.45f}), .linear_velocity = {0, 0, -9.0f}, .mass = 4.0f, .linear_damping = 0.0f}},
             Airfoil{.area = 2.0f, .lift_slope = 6.0f}, AirDrag{.area = 0.2f, .drag = 0.3f});
}

} // namespace

void build_water_station(Context& ctx) {
    build_floaters(ctx);
    build_bridge(ctx);
    build_pad_and_glider(ctx);
    add_station(ctx, water_centre, "Su ve Rüzgâr",
                "Hafif kasalar yüzer, taşlar batar · rüzgâr dönüp eser · menteşeli köprü · zıplama pedi (sensör + Luau) · planör", 27.0f);
}

} // namespace physics_showcase
