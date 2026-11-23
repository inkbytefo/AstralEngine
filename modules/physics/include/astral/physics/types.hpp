#pragma once
// Extended value types for Astral::Physics v2 (ADR-0038). Included by <astral/physics/physics.hpp>.

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace astral {

struct BodyTag;
struct CharacterTag;
struct JointTag;
struct ShapeTag;
using BodyHandle = Handle<BodyTag>;
using CharacterHandle = Handle<CharacterTag>;
using JointHandle = Handle<JointTag>;
using ShapeHandle = Handle<ShapeTag>;

enum class ShapeType : std::uint8_t {
    Box, Sphere, Capsule, Cylinder,
    ConvexHull,  ///< The hull of `points` (dynamic bodies too).
    Mesh,        ///< Triangles: static or kinematic bodies only.
    HeightField, ///< A grid of heights: static or kinematic bodies only.
    Compound,    ///< Several shapes in one body.
};

/// Narrow backend capability flags queried via PhysicsWorld::capabilities() / supports().
enum class PhysicsCapability : std::uint32_t {
    None            = 0,
    SixDOFJoints    = 1u << 0,
    SwingTwistJoints = 1u << 1,
    JointMotors     = 1u << 2,
    BreakableJoints = 1u << 3,
    CookedShapes    = 1u << 4,
    MaterialTable   = 1u << 5,
    BatchedQueries  = 1u << 6,
    OriginShift     = 1u << 7,
};

struct PhysicsCapabilities {
    std::uint32_t mask = 0;

    [[nodiscard]] constexpr bool supports(PhysicsCapability cap) const noexcept {
        return (mask & static_cast<std::uint32_t>(cap)) != 0;
    }
};

/// How friction or restitution of two colliding surfaces is combined.
/// When two registered materials differ, the higher-precedence mode wins (Average < Min < Multiply < Max).
enum class PhysicsCombineMode : std::uint8_t {
    Average = 0,
    Min = 1,
    Multiply = 2,
    Max = 3,
};

/// Surface material properties registered per `BodyDesc::surface` id on a PhysicsWorld.
struct PhysicsMaterialDesc {
    float friction = 0.5f;
    float restitution = 0.0f;
    PhysicsCombineMode friction_combine = PhysicsCombineMode::Multiply;
    PhysicsCombineMode restitution_combine = PhysicsCombineMode::Max;
    float density = 1000.0f; ///< kg/m^3 when BodyDesc::mass == 0.
};

/// Header metadata inspected from a versioned cooked physics shape (`ASSH` v1 binary).
struct CookedShapeInfo {
    std::uint32_t format_version = 0;
    std::uint32_t backend_hash = 0;
    ShapeType shape_type = ShapeType::Box;
    bool static_only = false;
    std::uint32_t payload_bytes = 0;
    bool compatible = false; ///< True when format_version and backend_hash match this runtime.
};

enum class JointSwingType : std::uint8_t { Cone, Pyramid };

enum class JointMotorMode : std::uint8_t { Off, Velocity, Position };

enum class JointAxis : std::uint8_t {
    TranslationX = 0,
    TranslationY = 1,
    TranslationZ = 2,
    RotationX = 3,
    RotationY = 4,
    RotationZ = 5,
};

/// Detailed per-axis limits for SixDOF and SwingTwist joints.
/// For SixDOF axes: `min > max` locks the axis at 0; `min <= -free_limit && max >= free_limit` leaves it free.
struct JointLimitsDesc {
    static constexpr float free_limit = 1e30f;

    JointSwingType swing_type = JointSwingType::Cone;
    Vec3 linear_min{1.0f, 1.0f, 1.0f};   ///< Default: translation locked on all 3 axes.
    Vec3 linear_max{-1.0f, -1.0f, -1.0f};
    Vec3 angular_min{-free_limit, -free_limit, -free_limit}; ///< SixDOF rotation limits (rad): x=twist, y=swingY, z=swingZ.
    Vec3 angular_max{free_limit, free_limit, free_limit};
    float swing_normal_cone = 0.0f; ///< SwingTwist: half-cone angle about the normal axis (rad).
    float swing_plane_cone = 0.0f;  ///< SwingTwist: half-cone angle about the plane axis (rad).
    float twist_min = 0.0f;         ///< SwingTwist: min twist angle (rad, in [-pi, pi]).
    float twist_max = 0.0f;         ///< SwingTwist: max twist angle (rad, in [-pi, pi]).

    void fix_translation() noexcept {
        linear_min = {1.0f, 1.0f, 1.0f};
        linear_max = {-1.0f, -1.0f, -1.0f};
    }
    void free_translation() noexcept {
        linear_min = {-free_limit, -free_limit, -free_limit};
        linear_max = {free_limit, free_limit, free_limit};
    }
    void fix_rotation() noexcept {
        angular_min = {1.0f, 1.0f, 1.0f};
        angular_max = {-1.0f, -1.0f, -1.0f};
    }
    void free_rotation() noexcept {
        angular_min = {-free_limit, -free_limit, -free_limit};
        angular_max = {free_limit, free_limit, free_limit};
    }
};

/// Motor/drive settings for Hinge, Slider, SixDOF, and SwingTwist joints.
struct JointMotorDesc {
    JointMotorMode mode = JointMotorMode::Off;
    float frequency_hz = 2.0f;      ///< Position motor spring frequency (Hz).
    float damping = 1.0f;           ///< Position motor damping ratio (1 = critical).
    float max_force = 0.0f;         ///< Linear motor limit (N); <= 0 = unlimited.
    float max_torque = 0.0f;        ///< Angular motor limit (N·m); <= 0 = unlimited.
    float target_velocity = 0.0f;   ///< Hinge (rad/s) or Slider (m/s).
    float target_position = 0.0f;   ///< Hinge (rad) or Slider (m).
    Vec3 target_angular_velocity{}; ///< SixDOF / SwingTwist angular velocity (rad/s, constraint space).
    Quat target_rotation{};         ///< SixDOF / SwingTwist target orientation (constraint space).
};

/// Constraint reaction loads reported from the previous completed physics step.
struct JointReaction {
    float force = 0.0f;  ///< Effective linear reaction force (N).
    float torque = 0.0f; ///< Effective angular reaction torque (N·m).
};

/// Emitted by PhysicsWorld::broken_joints() when a joint exceeds break_force or break_torque.
struct JointBrokenEvent {
    JointHandle joint{};
    BodyHandle body_a{};
    BodyHandle body_b{};
    float force = 0.0f;
    float torque = 0.0f;
};

} // namespace astral
