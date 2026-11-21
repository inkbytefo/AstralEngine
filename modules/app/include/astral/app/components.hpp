#pragma once

#include <astral/core/math.hpp>
#include <astral/physics/physics.hpp>
#include <astral/render/renderer.hpp>
#include <astral/world/world.hpp>

namespace astral {

/// Draws a mesh at the entity's WorldTransform with a simple PBR material.
struct MeshRenderer {
    MeshHandle mesh{};
    Vec4 color{1, 1, 1, 1};
    float roughness = 0.6f;
    float metallic = 0.0f;
    float emissive = 0.0f;
};

/// A signed-distance-field shape at the entity's WorldTransform (position and rotation; use
/// `shape.size` for dimensions). Shapes combine in ascending `order` (ties: any order), so a
/// SmoothUnion melts into earlier shapes and a Subtract carves them.
struct Sdf {
    SdfShape shape{};
    std::int32_t order = 0;
};

// PointLight (from the renderer) is also a component: its position follows the entity's
// WorldTransform when the entity has one.

/// Turns the entity into the camera (its WorldTransform is the camera pose; looks down -Z).
/// The first active camera found is used.
struct Camera {
    float fov_y = radians(70.0f);
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    bool active = true;
};

/// A physics body for this entity. Position/rotation come from the entity's Transform when the
/// body is created; afterwards dynamic bodies drive the Transform, kinematic bodies follow it.
/// Only root entities (no Parent) are supported. `desc` is read once at creation (to change the
/// shape or motion type, replace the component). `handle` is filled in by the App; copying the
/// component to another entity gives that entity its own body.
struct RigidBody {
    BodyDesc desc{};
    BodyHandle handle{};
};

/// A walking capsule (FPS / third-person). Set `move` (horizontal velocity, m/s) and `jump`
/// every frame; the App moves the character in the fixed update and writes the Transform.
struct CharacterController {
    CharacterDesc desc{};
    Vec3 move{};
    bool jump = false;
    float jump_speed = 5.0f;
    // Written by the App:
    bool on_ground = false;
    Vec3 velocity{};
    CharacterHandle handle{};
};

/// Packs an entity into physics user data (the App does this for RigidBody/CharacterController).
inline std::uint64_t to_user_data(Entity e) noexcept {
    return (std::uint64_t{e.generation} << 32) | e.index;
}
inline Entity entity_from_user_data(std::uint64_t data) noexcept {
    return {static_cast<std::uint32_t>(data & 0xFFFFFFFFu), static_cast<std::uint32_t>(data >> 32)};
}
/// The entity a raycast hit. Only meaningful for bodies created by the App (RigidBody,
/// CharacterController); bodies you create directly carry whatever user_data you gave them.
inline Entity entity_of(const RayHit& hit) noexcept { return entity_from_user_data(hit.user_data); }

/// Frame timing, available to every system as ctx.time.
struct Time {
    float fixed_delta = 1.0f / 60.0f; ///< Step length inside Phase::FixedUpdate systems.
    float delta = 0.0f;       ///< Seconds since the previous frame (clamped).
    double elapsed = 0.0;     ///< Seconds since the app started.
    std::uint64_t frame = 0;  ///< Frames completed.
};

} // namespace astral
