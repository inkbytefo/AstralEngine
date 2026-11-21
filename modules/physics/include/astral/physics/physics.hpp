#pragma once

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace astral {

struct BodyTag;
struct CharacterTag;
using BodyHandle = Handle<BodyTag>;
using CharacterHandle = Handle<CharacterTag>;

enum class MotionType : std::uint8_t {
    Static,    ///< Never moves (ground, walls).
    Kinematic, ///< Moved by code (move_kinematic), pushes dynamic bodies, ignores forces.
    Dynamic,   ///< Simulated: gravity, collisions, impulses.
};

enum class ShapeType : std::uint8_t { Box, Sphere, Capsule, Cylinder };

/// Collision shape. Capsules and cylinders stand along the Y axis.
struct ShapeDesc {
    ShapeType type = ShapeType::Box;
    Vec3 half_extents{0.5f, 0.5f, 0.5f}; ///< Box.
    float radius = 0.5f;                 ///< Sphere, capsule, cylinder.
    float half_height = 0.5f;            ///< Capsule (cylindrical part) and cylinder.

    static ShapeDesc box(Vec3 half_extents) { return {.type = ShapeType::Box, .half_extents = half_extents}; }
    static ShapeDesc sphere(float radius) { return {.type = ShapeType::Sphere, .radius = radius}; }
    static ShapeDesc capsule(float radius, float half_height) {
        return {.type = ShapeType::Capsule, .radius = radius, .half_height = half_height};
    }
    static ShapeDesc cylinder(float radius, float half_height) {
        return {.type = ShapeType::Cylinder, .radius = radius, .half_height = half_height};
    }
};

struct BodyDesc {
    ShapeDesc shape{};
    MotionType motion = MotionType::Dynamic;
    Vec3 position{};
    Quat rotation{};
    Vec3 linear_velocity{};
    float mass = 0.0f;          ///< kg; 0 = from the shape's volume (density 1000 kg/m³).
    float friction = 0.5f;
    float restitution = 0.0f;   ///< Bounciness 0..1.
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    float gravity_factor = 1.0f;
    bool sensor = false;        ///< Reports contacts but does not collide (triggers).
    bool continuous = false;    ///< Swept collision for fast objects (bullets).
    std::uint64_t user_data = 0; ///< Yours (e.g. an entity id), returned by queries.
};

struct RayHit {
    BodyHandle body{};
    Vec3 position{};
    Vec3 normal{};
    float distance = 0.0f;
    std::uint64_t user_data = 0;
};

struct ContactEvent {
    enum class Type : std::uint8_t { Begin, End };
    Type type = Type::Begin;
    BodyHandle a{};
    BodyHandle b{};
    Vec3 point{};   ///< Begin only.
    Vec3 normal{};  ///< Begin only: from a to b.
    std::uint64_t user_data_a = 0;
    std::uint64_t user_data_b = 0;
};

struct CharacterDesc {
    float radius = 0.4f;
    float height = 1.8f;             ///< Total height (capsule); position is at the feet.
    Vec3 position{};
    float max_slope = radians(50.0f); ///< Steeper ground counts as wall.
    float mass = 70.0f;               ///< Used when pushing dynamic bodies.
    std::uint64_t user_data = 0;
};

struct CharacterState {
    Vec3 position{}; ///< Feet.
    Vec3 velocity{};
    bool on_ground = false;
};

struct PhysicsDesc {
    Vec3 gravity{0, -9.81f, 0};
    std::uint32_t max_bodies = 65536;
    std::uint32_t max_body_pairs = 65536;         ///< Broad-phase pairs per step.
    std::uint32_t max_contact_constraints = 10240; ///< Simultaneous touching pairs.
    unsigned threads = 0; ///< Physics worker threads; 0 = hardware threads - 1.
};

/// A physics simulation (Jolt underneath, never exposed). Not thread-safe: use from one thread.
/// Jolt's type registry is process-wide; it is set up by the first PhysicsWorld and torn down
/// with the last one.
class PhysicsWorld {
public:
    explicit PhysicsWorld(const PhysicsDesc& desc = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Bodies ---------------------------------------------------------------------------
    BodyHandle create_body(const BodyDesc& desc);
    void destroy(BodyHandle body);
    [[nodiscard]] bool valid(BodyHandle body) const noexcept;
    [[nodiscard]] std::size_t body_count() const noexcept;

    [[nodiscard]] Vec3 position(BodyHandle body) const;
    [[nodiscard]] Quat rotation(BodyHandle body) const;
    [[nodiscard]] Vec3 linear_velocity(BodyHandle body) const;
    [[nodiscard]] float mass(BodyHandle body) const;
    [[nodiscard]] bool active(BodyHandle body) const;
    [[nodiscard]] std::uint64_t user_data(BodyHandle body) const;

    /// Teleports a body (no collision response on the way).
    void set_transform(BodyHandle body, Vec3 position, Quat rotation);
    /// Moves a kinematic body to the target over `dt`, so it pushes what it hits.
    void move_kinematic(BodyHandle body, Vec3 position, Quat rotation, float dt);
    void set_linear_velocity(BodyHandle body, Vec3 velocity);
    void add_impulse(BodyHandle body, Vec3 impulse);
    void add_force(BodyHandle body, Vec3 force);
    void set_gravity(Vec3 gravity);

    // Simulation -------------------------------------------------------------------------
    /// Advances the world by `dt` seconds (use a fixed dt; split large steps).
    void step(float dt, int collision_steps = 1);
    /// Contacts that began or ended during the last step().
    [[nodiscard]] std::span<const ContactEvent> contacts() const noexcept;

    // Queries ------------------------------------------------------------------------------
    /// Closest hit along a ray (direction need not be normalized), optionally ignoring one body.
    [[nodiscard]] std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                                BodyHandle ignore = {}) const;

    // Characters ---------------------------------------------------------------------------
    /// A capsule that walks: stands on ground, slides along walls, climbs gentle slopes and
    /// steps, is not pushed around by physics (it pushes dynamic bodies instead).
    CharacterHandle create_character(const CharacterDesc& desc);
    void destroy(CharacterHandle character);
    /// Moves the character with a horizontal `velocity` (y ignored) for `dt`; `jump` launches it
    /// upward with `jump_speed` when on the ground. Gravity is applied in the air.
    void update_character(CharacterHandle character, Vec3 velocity, bool jump, float dt,
                          float jump_speed = 5.0f);
    [[nodiscard]] CharacterState character_state(CharacterHandle character) const;
    void set_character_position(CharacterHandle character, Vec3 position);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
