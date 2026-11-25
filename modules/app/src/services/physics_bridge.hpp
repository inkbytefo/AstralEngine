#pragma once
// Internal to Astral::App: bridges RigidBody, CharacterController, Joint, JointMotor, fields,
// queued commands, streaming detach/attach, and contacts to PhysicsWorld (ADR-0029, ADR-0034, ADR-0038).

#include "physics_commands.hpp"

#include <astral/app/components.hpp>
#include <astral/app/physics_service.hpp>
#include <astral/physics/fields.hpp>
#include <astral/physics/physics.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace astral {

class JobSystem;
class Logger;
class PhysicsBridge;
class Profiler;
class ScriptPhysics;
class TerrainBridge;
class World;

std::unique_ptr<ScriptPhysics> make_app_script_physics(World& world, PhysicsBridge& bridge,
                                                       const FieldWorld& fields, const Time& time);

struct PhysicsBridgeConfig {
    std::uint32_t max_steps = 4;
    bool interpolation = false;
    JobSystem* jobs = nullptr;
};

class PhysicsBridge final : public PhysicsService {
public:
    using FixedStepCallback = void (*)(void*);

    PhysicsBridge(World& world, FieldWorld& fields, const Time& time, Profiler* profiler, Logger& log,
                  PhysicsBridgeConfig config);
    ~PhysicsBridge() override;
    PhysicsBridge(const PhysicsBridge&) = delete;
    PhysicsBridge& operator=(const PhysicsBridge&) = delete;

    [[nodiscard]] PhysicsWorld* world() noexcept { return physics_.get(); }
    [[nodiscard]] ScriptPhysics* script_physics() noexcept { return script_physics_.get(); }
    [[nodiscard]] BodyHandle resolve_body(Entity e) const noexcept;

    // PhysicsService
    [[nodiscard]] std::span<const ContactEvent> contacts() const noexcept override;
    [[nodiscard]] PhysicsQueryState query_state() const noexcept override;
    [[nodiscard]] PhysicsCapabilities capabilities() const noexcept override;
    [[nodiscard]] std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                                QueryFilter filter = {}) const override;
    void raycast_batch(std::span<const RayQuery> queries, std::span<std::optional<RayHit>> out) const override;
    [[nodiscard]] std::optional<ShapeHit> spherecast(Vec3 origin, float radius, Vec3 displacement,
                                                     QueryFilter filter = {}) const override;
    [[nodiscard]] std::vector<OverlapHit> overlap_sphere(Vec3 center, float radius,
                                                         QueryFilter filter = {}) const override;
    [[nodiscard]] std::vector<OverlapHit> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation = {},
                                                      QueryFilter filter = {}) const override;
    [[nodiscard]] Vec3 linear_velocity(Entity body) const override;
    [[nodiscard]] Vec3 angular_velocity(Entity body) const override;
    void add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key = 0) override;
    void add_force(Entity body, Vec3 force, std::uint64_t order_key = 0) override;
    void add_force_at(Entity body, Vec3 force, Vec3 world_point, std::uint64_t order_key = 0) override;
    void add_torque(Entity body, Vec3 torque, std::uint64_t order_key = 0) override;
    void add_angular_impulse(Entity body, Vec3 impulse, std::uint64_t order_key = 0) override;
    void set_linear_velocity(Entity body, Vec3 velocity, std::uint64_t order_key = 0) override;
    void set_angular_velocity(Entity body, Vec3 velocity, std::uint64_t order_key = 0) override;
    void set_body_transform(Entity body, Vec3 position, Quat rotation, std::uint64_t order_key = 0) override;
    void wake_body(Entity body, std::uint64_t order_key = 0) override;
    void sleep_body(Entity body, std::uint64_t order_key = 0) override;
    void set_joint_motor_mode(Entity joint, JointMotorMode mode, std::uint64_t order_key = 0) override;
    void set_joint_motor_velocity(Entity joint, float velocity, std::uint64_t order_key = 0) override;
    void set_joint_motor_position(Entity joint, float position, std::uint64_t order_key = 0) override;
    void set_joint_motor_angular_velocity(Entity joint, Vec3 velocity, std::uint64_t order_key = 0) override;
    void set_joint_motor_rotation(Entity joint, Quat rotation, std::uint64_t order_key = 0) override;
    void break_joint(Entity joint, std::uint64_t order_key = 0) override;
    [[nodiscard]] std::size_t pending_command_count() const noexcept override;

    void step(float delta, FixedStepCallback on_fixed_update = nullptr, void* user = nullptr,
              const TerrainBridge* terrain = nullptr);
    void shift_origin(Vec3 offset);

private:
    struct StaticBodyLink {
        Entity entity;
        BodyHandle body;
    };
    struct KinematicBodyLink {
        Entity entity;
        BodyHandle body;
        Vec3 previous_position{};
        Quat previous_rotation{};
    };
    struct DynamicBodyLink {
        Entity entity;
        BodyHandle body;
        Vec3 step_position{};
        Quat step_rotation{};
    };
    struct CharacterLink {
        Entity entity;
        CharacterHandle character;
    };
    struct JointLink {
        Entity entity;
        JointHandle joint;
        Entity a{};
        Entity b{};
        bool motor_synced = false;
        JointMotor last_motor{};
    };
    struct LinkRegistry {
        std::vector<StaticBodyLink> statics;
        std::vector<KinematicBodyLink> kinematics;
        std::vector<DynamicBodyLink> dynamics;
        std::vector<CharacterLink> characters;
        std::vector<JointLink> joints;
        std::unordered_map<std::uint64_t, std::size_t> joint_by_entity;
        std::unordered_map<std::uint64_t, std::size_t> joint_by_handle;
    };

    [[nodiscard]] JointHandle joint_of(Entity e) const noexcept;
    [[nodiscard]] JointLink* find_joint_link(Entity e) noexcept;
    void add_joint_link(JointLink link);
    void remove_joint_link(std::size_t idx);

    void apply_joint_command(const PhysicsCommand& cmd);
    void apply_command(const PhysicsCommand& cmd);
    void flush_commands();
    void sync_bodies();
    void sync_joints();
    void sync_joint_motors();
    void collect_broken_joints();
    void move_kinematics(float alpha, float fixed);
    void update_characters(float fixed, const TerrainBridge* terrain);
    void snapshot_interpolation();
    void emit_contact_events();
    void write_simulated_transforms(float blend);

    World& world_;
    FieldWorld& fields_;
    const Time& time_;
    Profiler* profiler_;
    JobSystem* jobs_;
    PhysicsBridgeConfig config_;
    std::unique_ptr<PhysicsWorld> physics_;
    std::unique_ptr<ScriptPhysics> script_physics_;
    float accumulator_ = 0.0f;
    std::uint64_t step_index_ = 0;
    double sim_time_ = 0.0;
    std::vector<ContactEvent> frame_contacts_;
    std::vector<JointBroken> frame_broken_joints_;
    LinkRegistry links_;
    PhysicsCommandQueue commands_;
    std::vector<PhysicsCommand> command_scratch_;
    bool in_fixed_update_ = false;
};

} // namespace astral
