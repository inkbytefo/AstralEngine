#pragma once

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>
#include <astral/physics/types.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace astral {

class JobSystem;

/// Collision layers: 16 of them (0 = default). Every body is in one; which layers collide is a symmetric matrix
/// (PhysicsWorld::set_layer_collision), everything collides by default. Queries take a mask of the layers they see.
constexpr unsigned max_physics_layers = 16;
constexpr std::uint32_t all_layers = 0xFFFFFFFFu;
constexpr std::uint32_t layer_bit(unsigned layer) noexcept { return 1u << layer; }

enum class MotionType : std::uint8_t {
    Static,    ///< Never moves (ground, walls).
    Kinematic, ///< Moved by code (move_kinematic), pushes dynamic bodies, ignores forces.
    Dynamic,   ///< Simulated: gravity, collisions, impulses.
};

struct ShapeChild;

/// Collision shape. Capsules and cylinders stand along the Y axis.
struct ShapeDesc {
    ShapeType type = ShapeType::Box;
    Vec3 half_extents{0.5f, 0.5f, 0.5f}; ///< Box.
    float radius = 0.5f;                 ///< Sphere, capsule, cylinder.
    float half_height = 0.5f;            ///< Capsule (cylindrical part) and cylinder.
    std::vector<Vec3> points{};          ///< ConvexHull: at least 4, not all in one plane.
    std::vector<Vec3> vertices{};        ///< Mesh.
    std::vector<std::uint32_t> indices{};///< Mesh: three per triangle, counter-clockwise seen from the front.
    std::vector<float> heights{};        ///< HeightField: samples x samples, row after row along +z.
    std::uint32_t samples = 0;           ///< HeightField: points per side.
    float cell_size = 1.0f;              ///< HeightField: metres between samples; the field is centred on the body.
    std::vector<ShapeChild> children{};  ///< Compound.

    static ShapeDesc box(Vec3 half_extents) { return {.type = ShapeType::Box, .half_extents = half_extents}; }
    static ShapeDesc sphere(float radius) { return {.type = ShapeType::Sphere, .radius = radius}; }
    static ShapeDesc capsule(float radius, float half_height) {
        return {.type = ShapeType::Capsule, .radius = radius, .half_height = half_height};
    }
    static ShapeDesc cylinder(float radius, float half_height) {
        return {.type = ShapeType::Cylinder, .radius = radius, .half_height = half_height};
    }
    static ShapeDesc convex_hull(std::vector<Vec3> points) {
        ShapeDesc d;
        d.type = ShapeType::ConvexHull;
        d.points = std::move(points);
        return d;
    }
    static ShapeDesc mesh(std::vector<Vec3> vertices, std::vector<std::uint32_t> indices) {
        ShapeDesc d;
        d.type = ShapeType::Mesh;
        d.vertices = std::move(vertices);
        d.indices = std::move(indices);
        return d;
    }
    static ShapeDesc height_field(std::vector<float> heights, std::uint32_t samples, float cell_size) {
        ShapeDesc d;
        d.type = ShapeType::HeightField;
        d.heights = std::move(heights);
        d.samples = samples;
        d.cell_size = cell_size;
        return d;
    }
    static ShapeDesc compound(std::vector<ShapeChild> children);
};

/// A part of a compound shape, placed relative to the body.
struct ShapeChild {
    Vec3 position{};
    Quat rotation{};
    ShapeDesc shape{};
};

inline ShapeDesc ShapeDesc::compound(std::vector<ShapeChild> children) {
    ShapeDesc d;
    d.type = ShapeType::Compound;
    d.children = std::move(children);
    return d;
}

struct BodyDesc {
    ShapeDesc shape{};
    MotionType motion = MotionType::Dynamic;
    Vec3 position{};
    Quat rotation{};
    Vec3 linear_velocity{};
    float mass = 0.0f;          ///< kg; 0 = from the shape's volume (density from material or 1000 kg/m³).
    float friction = 0.5f;
    float restitution = 0.0f;   ///< Bounciness 0..1.
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    float gravity_factor = 1.0f;
    std::uint8_t layer = 0;      ///< Collision layer, < max_physics_layers.
    std::uint16_t surface = 0;   ///< Surface/material id (0 = default): looked up in material table & reported by contacts/queries.
    bool sensor = false;        ///< Reports contacts but does not collide (triggers).
    bool continuous = false;    ///< Swept collision for fast objects (bullets).
    std::uint64_t user_data = 0; ///< Yours (e.g. an entity id), returned by queries.
    ShapeHandle cooked_shape{};  ///< Optional shared/cooked shape handle (overrides `shape` when valid).
};

struct RayHit {
    BodyHandle body{};
    Vec3 position{};
    Vec3 normal{};
    float distance = 0.0f;
    std::uint64_t user_data = 0;
    std::uint16_t surface = 0;
};

/// Which bodies a query sees.
struct QueryFilter {
    std::uint32_t layers = all_layers; ///< layer_bit(a) | layer_bit(b) | ...
    BodyHandle ignore{};
    bool include_sensors = false;
};

/// What a swept shape hit first.
struct ShapeHit {
    BodyHandle body{};
    Vec3 position{};  ///< Contact point.
    Vec3 normal{};    ///< Surface normal of the hit body there.
    float distance = 0.0f; ///< Travelled before touching.
    std::uint64_t user_data = 0;
    std::uint16_t surface = 0;
};

struct OverlapHit {
    BodyHandle body{};
    std::uint64_t user_data = 0;
};

/// Batched raycast input item.
struct RayQuery {
    Vec3 origin{};
    Vec3 direction{0, -1, 0};
    float max_distance = 100.0f;
    QueryFilter filter{};
};

/// Batched shape sweep input item (uses `cooked_shape` if valid, otherwise `shape`).
struct ShapeCastQuery {
    ShapeDesc shape{};
    ShapeHandle cooked_shape{};
    Vec3 position{};
    Quat rotation{};
    Vec3 displacement{};
    QueryFilter filter{};
};

/// Batched overlap input item (uses `cooked_shape` if valid, otherwise `shape`).
struct OverlapQuery {
    ShapeDesc shape{};
    ShapeHandle cooked_shape{};
    Vec3 position{};
    Quat rotation{};
    QueryFilter filter{};
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
    std::uint16_t surface_a = 0;
    std::uint16_t surface_b = 0;
    bool sensor = false; ///< One of the two is a sensor: a trigger entered / left, not a collision.
};

enum class JointType : std::uint8_t {
    Fixed,      ///< Welds the two bodies together as they are.
    Point,      ///< A ball joint: the anchor point is shared, rotation is free.
    Hinge,      ///< Rotates about `axis` through the anchor.
    Slider,     ///< Slides along `axis`.
    Distance,   ///< Keeps anchor on A and anchor_b on B within [min_limit, max_limit] metres of each other (rope, rod, spring).
    SixDOF,     ///< General 6-DOF constraint with per-axis linear/angular limits and motors.
    SwingTwist, ///< Ragdoll/limb cone+twist angular constraint with swing and twist motors.
};

/// Connects two bodies (or one body and the world). Positions and axes are in world space, as the bodies stand
/// when the joint is created. At least one body must be dynamic.
struct JointDesc {
    JointType type = JointType::Point;
    BodyHandle body_a{};          ///< Invalid: the world.
    BodyHandle body_b{};
    Vec3 anchor{};                ///< Where the joint is (on body A for Distance).
    Vec3 anchor_b{};              ///< Distance only: the point on body B.
    Vec3 axis{0, 1, 0};           ///< Primary axis: Hinge, Slider, SwingTwist twist axis, SixDOF X axis.
    Vec3 secondary_axis{1, 0, 0}; ///< Plane/Y axis for SwingTwist and SixDOF (auto-perpendicularized if collinear).
    bool limited = false;         ///< Hinge: [min_limit, max_limit] rad; Slider/Distance: metres.
    float min_limit = 0.0f;
    float max_limit = 0.0f;
    float spring_frequency = 0.0f; ///< Hz; > 0 makes the limits soft (Distance springs, soft stops). 0 = rigid.
    float spring_damping = 0.0f;   ///< 0 = undamped, 1 = critical.
    float max_friction = 0.0f;     ///< Resistance force (N) or torque (N·m) when motor is off.
    bool collide_connected = false; ///< Whether the two bodies still collide with each other.
    JointLimitsDesc limits{};       ///< Per-axis limits for SixDOF and SwingTwist.
    JointMotorDesc motor{};         ///< Optional motor/drive settings for Hinge, Slider, SixDOF, SwingTwist.
    float break_force = 0.0f;       ///< N; > 0 breaks the joint when reaction force reaches this threshold.
    float break_torque = 0.0f;      ///< N·m; > 0 breaks the joint when reaction torque reaches this threshold.
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

class Logger;

struct PhysicsDesc {
    Logger* logger = nullptr; ///< Receives physics errors (Jolt traces, overflows).
    Vec3 gravity{0, -9.81f, 0};
    std::uint32_t max_bodies = 65536;
    std::uint32_t max_body_pairs = 65536;         ///< Broad-phase pairs per step.
    std::uint32_t max_contact_constraints = 10240; ///< Simultaneous touching pairs.
    unsigned threads = 0; ///< Physics worker threads; 0 = hardware threads - 1.
};

/// Cooks a ShapeDesc into a versioned `ASSH` v1 binary blob without requiring a PhysicsWorld instance.
[[nodiscard]] std::vector<std::byte> cook_physics_shape(const ShapeDesc& desc);
/// Inspects the header of a cooked shape binary and checks compatibility with the current backend.
[[nodiscard]] std::optional<CookedShapeInfo> inspect_cooked_shape(std::span<const std::byte> data);

/// A physics simulation (Jolt underneath, never exposed). Not thread-safe: use from one thread.
/// Jolt's type registry is process-wide; it is set up by the first PhysicsWorld and torn down
/// with the last one.
class PhysicsWorld {
public:
    explicit PhysicsWorld(const PhysicsDesc& desc = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Backend capabilities -----------------------------------------------------------------
    [[nodiscard]] PhysicsCapabilities capabilities() const noexcept;
    [[nodiscard]] bool supports(PhysicsCapability cap) const noexcept { return capabilities().supports(cap); }

    // Shapes & materials (P0) --------------------------------------------------------------
    ShapeHandle create_shape(const ShapeDesc& desc);
    [[nodiscard]] std::vector<std::byte> cook_shape(const ShapeDesc& desc) const;
    ShapeHandle create_shape_from_cooked(std::span<const std::byte> data);
    void destroy(ShapeHandle shape);
    [[nodiscard]] bool valid(ShapeHandle shape) const noexcept;

    void register_material(std::uint16_t surface, const PhysicsMaterialDesc& desc);
    [[nodiscard]] std::optional<PhysicsMaterialDesc> material(std::uint16_t surface) const;
    void clear_material(std::uint16_t surface);

    // Bodies -------------------------------------------------------------------------------
    /// Returns an invalid handle for a bad shape (non-positive or non-finite sizes) or when
    /// max_bodies is reached.
    BodyHandle create_body(const BodyDesc& desc);
    void destroy(BodyHandle body);
    [[nodiscard]] bool valid(BodyHandle body) const noexcept;
    [[nodiscard]] std::size_t body_count() const noexcept;

    [[nodiscard]] Vec3 position(BodyHandle body) const;
    [[nodiscard]] Quat rotation(BodyHandle body) const;
    [[nodiscard]] Vec3 linear_velocity(BodyHandle body) const;
    [[nodiscard]] Vec3 angular_velocity(BodyHandle body) const;
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
    /// Applies a force at a world point (it also turns the body).
    void add_force_at(BodyHandle body, Vec3 force, Vec3 point);
    void set_angular_velocity(BodyHandle body, Vec3 velocity);
    void add_torque(BodyHandle body, Vec3 torque);
    void wake(BodyHandle body);
    void sleep(BodyHandle body);
    void set_gravity(Vec3 gravity);
    [[nodiscard]] Vec3 gravity() const;
    /// Whether bodies of layers `a` and `b` collide (symmetric). Two static bodies never do.
    void set_layer_collision(unsigned a, unsigned b, bool collide);
    [[nodiscard]] bool layers_collide(unsigned a, unsigned b) const noexcept;
    /// Floating origin: moves every body, character, and world-anchored joint by -offset. Call between steps.
    void shift_origin(Vec3 offset);

    // Joints -------------------------------------------------------------------------------
    /// Returns an invalid handle (and logs) for a bad description: no bodies, the same body twice, a dead body, a zero axis.
    /// Destroying a body destroys its joints.
    JointHandle create_joint(const JointDesc& desc);
    void destroy(JointHandle joint);
    [[nodiscard]] bool valid(JointHandle joint) const noexcept;
    void set_joint_motor_mode(JointHandle joint, JointMotorMode mode);
    void set_joint_motor_velocity(JointHandle joint, float velocity);
    void set_joint_motor_position(JointHandle joint, float position);
    void set_joint_motor_angular_velocity(JointHandle joint, Vec3 angular_velocity);
    void set_joint_motor_rotation(JointHandle joint, Quat target_rotation);
    void set_joint_limits(JointHandle joint, float min_limit, float max_limit);
    void set_joint_limits(JointHandle joint, const JointLimitsDesc& limits);
    void break_joint(JointHandle joint);
    [[nodiscard]] bool joint_broken(JointHandle joint) const noexcept;
    [[nodiscard]] JointReaction joint_reaction(JointHandle joint) const noexcept;
    [[nodiscard]] std::span<const JointBrokenEvent> broken_joints() const noexcept;

    // Simulation ---------------------------------------------------------------------------
    /// Advances the world by `dt` seconds (use a fixed dt; split large steps).
    void step(float dt, int collision_steps = 1);
    /// Contacts that began or ended during the last step().
    [[nodiscard]] std::span<const ContactEvent> contacts() const noexcept;

    // Queries ------------------------------------------------------------------------------
    /// Closest hit along a ray (direction need not be normalized), optionally ignoring one body.
    [[nodiscard]] std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                                const QueryFilter& filter = {}) const;
    /// Sweeps `shape` (at position/rotation) by `displacement` and reports the first body it touches.
    [[nodiscard]] std::optional<ShapeHit> shape_cast(const ShapeDesc& shape, Vec3 position, Quat rotation, Vec3 displacement,
                                                     const QueryFilter& filter = {}) const;
    /// Bodies that touch `shape` placed at position/rotation.
    [[nodiscard]] std::vector<OverlapHit> overlap(const ShapeDesc& shape, Vec3 position, Quat rotation = {},
                                                  const QueryFilter& filter = {}) const;
    /// Batched queries (output span must have at least `queries.size()` elements; runs in parallel when `jobs != nullptr`).
    void raycast_batch(std::span<const RayQuery> queries, std::span<std::optional<RayHit>> out, JobSystem* jobs = nullptr) const;
    void shape_cast_batch(std::span<const ShapeCastQuery> queries, std::span<std::optional<ShapeHit>> out,
                          JobSystem* jobs = nullptr) const;
    void overlap_batch(std::span<const OverlapQuery> queries, std::span<std::vector<OverlapHit>> out,
                       JobSystem* jobs = nullptr) const;

    // Characters ---------------------------------------------------------------------------
    CharacterHandle create_character(const CharacterDesc& desc);
    void destroy(CharacterHandle character);
    [[nodiscard]] bool valid(CharacterHandle character) const noexcept;
    [[nodiscard]] std::uint64_t user_data(CharacterHandle character) const;
    void update_character(CharacterHandle character, Vec3 velocity, bool jump, float dt,
                          float jump_speed = 5.0f);
    [[nodiscard]] CharacterState character_state(CharacterHandle character) const;
    void set_character_position(CharacterHandle character, Vec3 position);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
