#pragma once
// Internal to Astral::App: bridges RigidBody, CharacterController, Joint, JointMotor, fields,
// queued commands, streaming detach/attach, and contacts to PhysicsWorld (ADR-0029, ADR-0034, ADR-0038).

#include <astral/app/components.hpp>
#include <astral/app/physics_service.hpp>
#include <astral/physics/fields.hpp>
#include <astral/physics/physics.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace astral {

class JobSystem;
class Logger;
class Profiler;
class ScriptPhysics;
class World;

std::unique_ptr<ScriptPhysics> make_app_script_physics(World& world, PhysicsService& service,
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
    void add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key = 0) override;
    void add_force(Entity body, Vec3 force, std::uint64_t order_key = 0) override;
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

    void step(float delta, FixedStepCallback on_fixed_update = nullptr, void* user = nullptr);
    void shift_origin(Vec3 offset);

private:
    enum class CommandKind : std::uint8_t {
        Impulse,
        Force,
        SetLinearVelocity,
        SetAngularVelocity,
        SetBodyTransform,
        WakeBody,
        SleepBody,
        SetJointMotorMode,
        SetJointMotorVelocity,
        SetJointMotorPosition,
        SetJointMotorAngularVelocity,
        SetJointMotorRotation,
        BreakJoint,
    };
    struct QueuedCommand {
        std::uint64_t order_key = 0;
        std::uint64_t sequence = 0;
        Entity entity{};
        CommandKind kind = CommandKind::Impulse;
        Vec3 value{};
        Quat quat{};
        float scalar = 0.0f;
        JointMotorMode motor_mode = JointMotorMode::Off;
    };
    struct BodyLink {
        Entity entity;
        BodyHandle body;
        Vec3 previous_position{}; // kinematic bodies: pose at the start of the frame
        Quat previous_rotation{};
        Vec3 step_position{};     // physics_interpolation: simulated pose before the last step
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
    };

    [[nodiscard]] BodyHandle body_of(Entity e) const noexcept;
    [[nodiscard]] JointHandle joint_of(Entity e) const noexcept;
    void enqueue(QueuedCommand cmd);
    void apply_command(const QueuedCommand& cmd);
    void flush_commands();
    void sync_bodies();
    void sync_joints();
    void sync_joint_motors();
    void collect_broken_joints();
    void update_fields();
    void move_kinematics(float alpha, float fixed);
    void update_characters(float fixed);
    void snapshot_interpolation();
    void apply_field_forces(float dt);
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
    std::vector<BodyLink> body_links_;
    std::vector<CharacterLink> character_links_;
    std::vector<JointLink> joint_links_;
    mutable std::mutex command_mutex_;
    std::vector<QueuedCommand> pending_commands_;
    std::uint64_t next_command_seq_ = 0;
    bool in_fixed_update_ = false;
};

} // namespace astral
