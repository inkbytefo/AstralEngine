#pragma once

#include <astral/core/math.hpp>
#include <astral/physics/physics.hpp>
#include <astral/world/world.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace astral {

/// Describes the completed physics simulation snapshot that PhysicsService queries read from (ADR-0034).
struct PhysicsQueryState {
    std::uint64_t step_index = 0; ///< Number of completed fixed physics steps since App startup.
    double sim_time = 0.0;        ///< Total simulated physics time across completed steps (seconds).
    bool in_fixed_update = false; ///< True while inside FixedUpdate (contacts/queries see step_index, i.e. previous step).
};

/// Narrow physics command, query, and contact interface bound to an App's Context (ADR-0034).
/// - Commands (add_impulse, add_force, set_linear_velocity, set_angular_velocity) are thread-safe,
///   queued deterministically, and applied at the fixed-step boundary after world.apply_commands()
///   and body synchronization, immediately before PhysicsWorld::step().
/// - Queries always read from the last completed physics step (`query_state()`).
class PhysicsService {
public:
    virtual ~PhysicsService() = default;
    PhysicsService(const PhysicsService&) = delete;
    PhysicsService& operator=(const PhysicsService&) = delete;

    /// Contacts that began or ended: inside FixedUpdate, the previous completed step;
    /// elsewhere, every step completed during this frame.
    [[nodiscard]] virtual std::span<const ContactEvent> contacts() const noexcept = 0;
    /// Metadata describing which completed simulation step queries currently observe.
    [[nodiscard]] virtual PhysicsQueryState query_state() const noexcept = 0;
    /// Backend capability flags (ADR-0038).
    [[nodiscard]] virtual PhysicsCapabilities capabilities() const noexcept = 0;

    // Queries against the last completed step -----------------------------------------------------
    [[nodiscard]] virtual std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                                        QueryFilter filter = {}) const = 0;
    virtual void raycast_batch(std::span<const RayQuery> queries, std::span<std::optional<RayHit>> out) const = 0;
    [[nodiscard]] virtual std::optional<ShapeHit> spherecast(Vec3 origin, float radius, Vec3 displacement,
                                                             QueryFilter filter = {}) const = 0;
    [[nodiscard]] virtual std::vector<OverlapHit> overlap_sphere(Vec3 center, float radius,
                                                                 QueryFilter filter = {}) const = 0;
    [[nodiscard]] virtual std::vector<OverlapHit> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation = {},
                                                              QueryFilter filter = {}) const = 0;
    [[nodiscard]] virtual Vec3 linear_velocity(Entity body) const = 0;

    // Queued deterministic commands (safe from parallel systems and scripts) ----------------------
    /// `order_key == 0` orders deterministically by entity (index, generation). Equal keys apply in
    /// producer order (ADR-0043): code outside systems first, then systems in registration order (the
    /// later one wins a competing setter), then submission order within one producer. A system that
    /// queues competing commands from its own parallel jobs must give them explicit order keys.
    virtual void add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key = 0) = 0;
    virtual void add_force(Entity body, Vec3 force, std::uint64_t order_key = 0) = 0;
    virtual void set_linear_velocity(Entity body, Vec3 velocity, std::uint64_t order_key = 0) = 0;
    virtual void set_angular_velocity(Entity body, Vec3 velocity, std::uint64_t order_key = 0) = 0;
    virtual void set_body_transform(Entity body, Vec3 position, Quat rotation, std::uint64_t order_key = 0) = 0;
    virtual void wake_body(Entity body, std::uint64_t order_key = 0) = 0;
    virtual void sleep_body(Entity body, std::uint64_t order_key = 0) = 0;
    virtual void set_joint_motor_mode(Entity joint, JointMotorMode mode, std::uint64_t order_key = 0) = 0;
    virtual void set_joint_motor_velocity(Entity joint, float velocity, std::uint64_t order_key = 0) = 0;
    virtual void set_joint_motor_position(Entity joint, float position, std::uint64_t order_key = 0) = 0;
    virtual void set_joint_motor_angular_velocity(Entity joint, Vec3 velocity, std::uint64_t order_key = 0) = 0;
    virtual void set_joint_motor_rotation(Entity joint, Quat rotation, std::uint64_t order_key = 0) = 0;
    virtual void break_joint(Entity joint, std::uint64_t order_key = 0) = 0;
    /// Number of queued commands waiting for the next fixed physics step.
    [[nodiscard]] virtual std::size_t pending_command_count() const noexcept = 0;

protected:
    PhysicsService() = default;
};

} // namespace astral
