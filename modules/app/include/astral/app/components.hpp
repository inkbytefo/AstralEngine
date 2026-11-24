#pragma once

#include <astral/audio/audio.hpp>
#include <astral/core/math.hpp>
#include <astral/physics/fields.hpp>
#include <astral/physics/physics.hpp>
#include <astral/render/renderer.hpp>
#include <astral/world/world.hpp>

#include <string>

namespace astral {

/// Draws a mesh at the entity's WorldTransform. Meshes and materials are shared resources
/// (Context::create_mesh / create_material); an invalid material draws with the default one.
/// The App tracks last frame's transform itself, so moving entities get motion vectors.
struct MeshRenderer {
    MeshHandle mesh{};
    MaterialHandle material{};
};

/// Built-in shapes for PrimitiveMesh (unit size: scale them with the Transform).
enum class Primitive : std::uint8_t { Cube, Sphere, Plane };

/// A built-in shape with its material written inline: the way to place meshes from a scene file
/// (MeshRenderer holds runtime handles, which files cannot). The App fills in a MeshRenderer and
/// shares one material between identical descriptions; editing the component updates it.
struct PrimitiveMesh {
    Primitive shape = Primitive::Cube;
    MaterialDesc material{};
    /// A .material file (ADR-0022); when set it replaces `material`.
    std::string material_file{};
};

/// Opt-in day cycle. The first enabled Atmosphere drives the scene's sun and ambient sky.
struct Atmosphere {
    float hour = 12.0f;              ///< Local solar time, wrapped to [0, 24).
    float hours_per_second = 0.0f;  ///< Zero keeps the authored time fixed.
    float orbit_tilt_deg = 25.0f;    ///< Tilt away from zenith at noon.
    float heading_deg = 0.0f;       ///< Rotates the solar path around +Y.
    float sun_intensity = 3.0f;      ///< Unattenuated intensity in renderer units.
    std::string sky_material;
    bool enabled = true;
};

/// A signed-distance-field shape at the entity's WorldTransform (position and rotation; use
/// `shape.size` for dimensions). Shapes combine in ascending `order` (ties: any order), so a
/// SmoothUnion melts into earlier shapes and a Subtract carves them.
struct Sdf {
    SdfShape shape{};
    std::int32_t order = 0;
};

// PointLight and RectLight (from the renderer) are also components: their position follows the entity's
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

/// A box of wind centred on the entity (axis-aligned, scale ignored). Adds to the world's wind inside it (or replaces it),
/// fading over `falloff` metres at the edge. `global`: not a box but the wind everywhere (the last one wins).
/// Fields are functions of position and time: gusts and turbulence are noise, so the same wind is seen by physics (AirDrag, Airfoil,
/// Buoyant), scripts (ctx.fields) and, in later phases, shaders (fields.glsl).
struct WindZone {
    Vec3 half_extents{5, 5, 5};
    Vec3 velocity{};               ///< m/s.
    float gust_strength = 0.0f;    ///< +- m/s along the wind.
    float gust_frequency = 0.2f;
    float turbulence = 0.0f;       ///< +- m/s on every axis.
    float turbulence_scale = 8.0f; ///< Size of the swirls (m).
    float falloff = 0.0f;          ///< Fade distance at the edge (m).
    bool replace = false;          ///< Replace the global wind inside instead of adding to it.
    bool global = false;
    std::uint32_t seed = 1;
};

/// A box of water centred on the entity (axis-aligned, scale ignored): the top is the surface, waves ride on it.
struct WaterBody {
    Vec3 half_extents{10, 2, 10};
    float density = 1000.0f;       ///< kg/m^3 (sea water 1025).
    Vec3 flow{};                   ///< Current, m/s.
    float wave_height = 0.0f;      ///< +- m.
    float wave_length = 8.0f;
    float wave_speed = 1.0f;
};

/// Pushes a RigidBody up and slows it down in water (needs a RigidBody): Archimedes with the part below the surface, and water drag.
struct Buoyant {
    float volume = 0.0f;       ///< m^3 when fully under water; 0 = the body's mass / 500 (it floats half under).
    float height = 1.0f;       ///< Vertical extent (m): how deep it sinks before it is fully submerged.
    float area = 1.0f;         ///< Cross-section facing the current (m^2).
    float drag = 1.0f;         ///< Drag coefficient in water.
    float angular_drag = 1.0f; ///< How fast spin dies in water (1/s while fully submerged).
};

/// Air resistance on a RigidBody: F = 1/2 rho Cd A |v_rel| v_rel with v_rel = wind - velocity (scaled down under water).
struct AirDrag {
    float area = 1.0f;         ///< m^2.
    float drag = 0.5f;         ///< Coefficient (sphere 0.47, cube 1.05, streamlined 0.1).
};

/// A wing: lift on a RigidBody from the air flowing past it, in proportion to the angle of attack (up to a stall).
struct Airfoil {
    float area = 1.0f;          ///< Wing area (m^2).
    float lift_slope = 5.0f;    ///< Lift coefficient per radian (a thin wing: 2 pi).
    float stall_angle = 0.3f;   ///< Radians; beyond it the lift fades out.
    Vec3 forward{0, 0, -1};     ///< Along the chord, towards the leading edge (body space).
    Vec3 up{0, 1, 0};           ///< The wing's lift side (body space).
};

/// Connects the RigidBodies of two entities (or one and the world: leave `a` empty). Created once both bodies exist, with the anchor
/// and axis in world space as the bodies stand then; removed with the entity. If a connected body unloads (streaming), the joint
/// detaches (emitting JointDetached) and re-attaches when both bodies exist again; if broken by load, `broken` becomes true (JointBroken).
struct Joint {
    JointType type = JointType::Point;
    Entity a{};                    ///< Empty: the world.
    Entity b{};
    Vec3 anchor{};                 ///< World space (on A for Distance).
    Vec3 anchor_b{};               ///< Distance only: the point on B.
    Vec3 axis{0, 1, 0};            ///< Primary axis: Hinge, Slider, SwingTwist twist, SixDOF X.
    Vec3 secondary_axis{1, 0, 0};  ///< Plane/Y axis for SwingTwist and SixDOF.
    bool limited = false;
    float min_limit = 0.0f;        ///< Hinge rad, Slider m, Distance m, SwingTwist twist_min.
    float max_limit = 0.0f;        ///< Hinge rad, Slider m, Distance m, SwingTwist twist_max.
    Vec2 swing_limits{};           ///< SwingTwist / SixDOF swing half-cone angles (normal, plane) in radians.
    Vec3 linear_limit_min{1, 1, 1};   ///< SixDOF linear min (min > max locks axis; default locked).
    Vec3 linear_limit_max{-1, -1, -1};///< SixDOF linear max.
    float spring_frequency = 0.0f;
    float spring_damping = 0.0f;
    float max_friction = 0.0f;
    bool collide_connected = false;
    float break_force = 0.0f;      ///< N; > 0 breaks the joint under excessive linear reaction force.
    float break_torque = 0.0f;     ///< N·m; > 0 breaks the joint under excessive angular reaction torque.
    bool broken = false;           ///< Written by the App when the joint breaks; reset to false to repair/re-attach.
    JointHandle handle{};          ///< Filled in by the App.
};

/// Optional motor/drive attached to the same entity as a Joint (Hinge, Slider, SixDOF, SwingTwist).
struct JointMotor {
    JointMotorMode mode = JointMotorMode::Off;
    float frequency = 2.0f;
    float damping = 1.0f;
    float max_force = 0.0f;
    float max_torque = 0.0f;
    float target_velocity = 0.0f;
    float target_position = 0.0f;
    Vec3 target_angular_velocity{};
    Quat target_rotation{};
};

/// Sent when a breakable Joint exceeds its break_force or break_torque threshold (or break_joint is called).
struct JointBroken {
    Entity joint{};
    Entity a{};
    Entity b{};
    float force = 0.0f;
    float torque = 0.0f;
};

/// Sent when a live Joint detaches because a connected body was destroyed or streamed out (distinct from JointBroken).
struct JointDetached {
    Entity joint{};
    Entity a{};
    Entity b{};
};

/// Two bodies touched or separated (physics events: astral.on("astral::Contact") in scripts).
enum class ContactKind : std::uint8_t { Begin, End };
struct Contact {
    ContactKind kind = ContactKind::Begin;
    Entity a{};
    Entity b{};
    Vec3 point{};                  ///< Begin only.
    Vec3 normal{};                 ///< Begin only, from a to b.
    std::int32_t surface_a = 0;    ///< BodyDesc::surface of each: what they are made of.
    std::int32_t surface_b = 0;
};

/// A body entered / left a sensor (RigidBody with BodyDesc::sensor): the trigger volume of ADR-0023.
struct TriggerEntered {
    Entity trigger{};
    Entity other{};
};
struct TriggerExited {
    Entity trigger{};
    Entity other{};
};

/// A sound playing at the entity (positioned in 3D unless `spatial` is false). Set `playing` to
/// start or stop it; the App clears it when a non-looping sound ends. Serializable (scene files).
struct AudioSource {
    std::string sound;              ///< Asset path ("sounds/fire.wav").
    AudioBus bus = AudioBus::Ambience;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = true;
    bool spatial = true;
    float min_distance = 1.0f;
    float max_distance = 40.0f;
    bool playing = true;
};

/// Hears the world from this entity (its WorldTransform; looks down -Z). Without one the active
/// camera listens.
struct AudioListener {};

/// Frame timing, available to every system as ctx.time.
struct Time {
    float fixed_delta = 1.0f / 60.0f; ///< Step length inside Phase::FixedUpdate systems.
    float delta = 0.0f;       ///< Seconds since the previous frame (clamped).
    double elapsed = 0.0;     ///< Seconds since the app started.
    std::uint64_t frame = 0;  ///< Frames completed.
};

/// Cells of an open map (Context::open_world) are loaded around every active StreamingSource;
/// without one, around the active camera. The first one also drives the floating origin.
struct StreamingSource {
    bool active = true;
};

} // namespace astral

ASTRAL_REFLECT_ENUM(astral::Primitive, Cube, Sphere, Plane)
ASTRAL_REFLECT(astral::PrimitiveMesh, shape, material, material_file)
ASTRAL_REFLECT(astral::Atmosphere, hour, hours_per_second, orbit_tilt_deg, heading_deg, sun_intensity, sky_material, enabled)
ASTRAL_REFLECT(astral::Sdf, shape, order)
ASTRAL_REFLECT(astral::StreamingSource, active)
ASTRAL_REFLECT(astral::Camera, fov_y, near_plane, far_plane, active)
ASTRAL_REFLECT(astral::AudioSource, sound, bus, volume, pitch, loop, spatial, min_distance, max_distance, playing)
ASTRAL_REFLECT_EMPTY(astral::AudioListener)
ASTRAL_REFLECT(astral::WindZone, half_extents, velocity, gust_strength, gust_frequency, turbulence, turbulence_scale, falloff, replace, global, seed)
ASTRAL_REFLECT(astral::WaterBody, half_extents, density, flow, wave_height, wave_length, wave_speed)
ASTRAL_REFLECT(astral::Buoyant, volume, height, area, drag, angular_drag)
ASTRAL_REFLECT(astral::AirDrag, area, drag)
ASTRAL_REFLECT(astral::Airfoil, area, lift_slope, stall_angle, forward, up)
ASTRAL_REFLECT_ENUM(astral::ContactKind, Begin, End)
ASTRAL_REFLECT(astral::Contact, kind, a, b, point, normal, surface_a, surface_b)
ASTRAL_REFLECT(astral::TriggerEntered, trigger, other)
ASTRAL_REFLECT(astral::TriggerExited, trigger, other)
ASTRAL_REFLECT_ENUM(astral::JointType, Fixed, Point, Hinge, Slider, Distance, SixDOF, SwingTwist)
ASTRAL_REFLECT_ENUM(astral::JointMotorMode, Off, Velocity, Position)
ASTRAL_REFLECT(astral::Joint, type, a, b, anchor, anchor_b, axis, secondary_axis, limited, min_limit, max_limit, swing_limits, linear_limit_min, linear_limit_max, spring_frequency, spring_damping, max_friction, collide_connected, break_force, break_torque, broken)
ASTRAL_REFLECT(astral::JointMotor, mode, frequency, damping, max_force, max_torque, target_velocity, target_position, target_angular_velocity, target_rotation)
ASTRAL_REFLECT(astral::JointBroken, joint, a, b, force, torque)
ASTRAL_REFLECT(astral::JointDetached, joint, a, b)
