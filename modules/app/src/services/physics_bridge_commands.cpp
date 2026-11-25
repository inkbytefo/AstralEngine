// PhysicsBridge queries and queued deterministic commands (ADR-0034, ADR-0043), split from
// physics_bridge.cpp to keep both files inside the size budget (ADR-0030).
#include "physics_bridge.hpp"

#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <optional>
#include <vector>

namespace astral {

std::optional<RayHit> PhysicsBridge::raycast(Vec3 origin, Vec3 direction, float max_distance, QueryFilter filter) const {
    return physics_->raycast(origin, direction, max_distance, filter);
}

void PhysicsBridge::raycast_batch(std::span<const RayQuery> queries, std::span<std::optional<RayHit>> out) const {
    physics_->raycast_batch(queries, out, jobs_);
}

std::optional<ShapeHit> PhysicsBridge::spherecast(Vec3 origin, float radius, Vec3 displacement, QueryFilter filter) const {
    return physics_->shape_cast(ShapeDesc::sphere(radius), origin, {}, displacement, filter);
}

std::vector<OverlapHit> PhysicsBridge::overlap_sphere(Vec3 center, float radius, QueryFilter filter) const {
    return physics_->overlap(ShapeDesc::sphere(radius), center, {}, filter);
}

std::vector<OverlapHit> PhysicsBridge::overlap_box(Vec3 center, Vec3 half_extents, Quat rotation, QueryFilter filter) const {
    return physics_->overlap(ShapeDesc::box(half_extents), center, rotation, filter);
}

Vec3 PhysicsBridge::linear_velocity(Entity body) const {
    const BodyHandle handle = resolve_body(body);
    return handle.valid() ? physics_->linear_velocity(handle) : Vec3{};
}

Vec3 PhysicsBridge::angular_velocity(Entity body) const {
    const BodyHandle handle = resolve_body(body);
    return handle.valid() ? physics_->angular_velocity(handle) : Vec3{};
}

void PhysicsBridge::add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::Impulse, .value = impulse});
}
void PhysicsBridge::add_force(Entity body, Vec3 force, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::Force, .value = force});
}
void PhysicsBridge::add_force_at(Entity body, Vec3 force, Vec3 world_point, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key,
                       .entity = body,
                       .kind = PhysicsCommandKind::ForceAt,
                       .value = force,
                       .point = world_point});
}
void PhysicsBridge::add_torque(Entity body, Vec3 torque, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::Torque, .value = torque});
}
void PhysicsBridge::add_angular_impulse(Entity body, Vec3 impulse, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::AngularImpulse, .value = impulse});
}
void PhysicsBridge::set_linear_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::SetLinearVelocity, .value = velocity});
}
void PhysicsBridge::set_angular_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::SetAngularVelocity, .value = velocity});
}
void PhysicsBridge::set_body_transform(Entity body, Vec3 position, Quat rotation, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key,
                       .entity = body,
                       .kind = PhysicsCommandKind::SetBodyTransform,
                       .value = position,
                       .quat = rotation});
}
void PhysicsBridge::wake_body(Entity body, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::WakeBody});
}
void PhysicsBridge::sleep_body(Entity body, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = body, .kind = PhysicsCommandKind::SleepBody});
}
void PhysicsBridge::set_joint_motor_mode(Entity joint, JointMotorMode mode, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = joint, .kind = PhysicsCommandKind::SetJointMotorMode, .motor_mode = mode});
}
void PhysicsBridge::set_joint_motor_velocity(Entity joint, float velocity, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = joint, .kind = PhysicsCommandKind::SetJointMotorVelocity, .scalar = velocity});
}
void PhysicsBridge::set_joint_motor_position(Entity joint, float position, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = joint, .kind = PhysicsCommandKind::SetJointMotorPosition, .scalar = position});
}
void PhysicsBridge::set_joint_motor_angular_velocity(Entity joint, Vec3 velocity, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key,
                       .entity = joint,
                       .kind = PhysicsCommandKind::SetJointMotorAngularVelocity,
                       .value = velocity});
}
void PhysicsBridge::set_joint_motor_rotation(Entity joint, Quat rotation, std::uint64_t order_key) {
    commands_.enqueue(
        {.order_key = order_key, .entity = joint, .kind = PhysicsCommandKind::SetJointMotorRotation, .quat = rotation});
}
void PhysicsBridge::break_joint(Entity joint, std::uint64_t order_key) {
    commands_.enqueue({.order_key = order_key, .entity = joint, .kind = PhysicsCommandKind::BreakJoint});
}

std::size_t PhysicsBridge::pending_command_count() const noexcept { return commands_.pending_count(); }

void PhysicsBridge::apply_joint_command(const PhysicsCommand& cmd) {
    const JointHandle jh = joint_of(cmd.entity);
    if (!jh.valid()) return;
    switch (cmd.kind) {
    case PhysicsCommandKind::SetJointMotorMode:
        physics_->set_joint_motor_mode(jh, cmd.motor_mode);
        if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) {
            m->mode = cmd.motor_mode;
            if (JointLink* link = find_joint_link(cmd.entity)) link->last_motor.mode = cmd.motor_mode;
        }
        break;
    case PhysicsCommandKind::SetJointMotorVelocity:
        physics_->set_joint_motor_velocity(jh, cmd.scalar);
        if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) {
            m->target_velocity = cmd.scalar;
            if (JointLink* link = find_joint_link(cmd.entity)) link->last_motor.target_velocity = cmd.scalar;
        }
        break;
    case PhysicsCommandKind::SetJointMotorPosition:
        physics_->set_joint_motor_position(jh, cmd.scalar);
        if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) {
            m->target_position = cmd.scalar;
            if (JointLink* link = find_joint_link(cmd.entity)) link->last_motor.target_position = cmd.scalar;
        }
        break;
    case PhysicsCommandKind::SetJointMotorAngularVelocity:
        physics_->set_joint_motor_angular_velocity(jh, cmd.value);
        if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) {
            m->target_angular_velocity = cmd.value;
            if (JointLink* link = find_joint_link(cmd.entity)) link->last_motor.target_angular_velocity = cmd.value;
        }
        break;
    case PhysicsCommandKind::SetJointMotorRotation:
        physics_->set_joint_motor_rotation(jh, cmd.quat);
        if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) {
            m->target_rotation = cmd.quat;
            if (JointLink* link = find_joint_link(cmd.entity)) link->last_motor.target_rotation = cmd.quat;
        }
        break;
    case PhysicsCommandKind::BreakJoint:
        physics_->break_joint(jh);
        if (Joint* j = world_.get<Joint>(cmd.entity)) {
            j->broken = true;
            frame_broken_joints_.push_back({cmd.entity, j->a, j->b, 0.0f, 0.0f});
        }
        break;
    default:
        break;
    }
}

void PhysicsBridge::apply_command(const PhysicsCommand& cmd) {
    switch (cmd.kind) {
    case PhysicsCommandKind::Impulse:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->add_impulse(h, cmd.value);
        break;
    case PhysicsCommandKind::Force:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->add_force(h, cmd.value);
        break;
    case PhysicsCommandKind::ForceAt:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->add_force_at(h, cmd.value, cmd.point);
        break;
    case PhysicsCommandKind::Torque:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->add_torque(h, cmd.value);
        break;
    case PhysicsCommandKind::AngularImpulse:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->add_angular_impulse(h, cmd.value);
        break;
    case PhysicsCommandKind::SetLinearVelocity:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->set_linear_velocity(h, cmd.value);
        break;
    case PhysicsCommandKind::SetAngularVelocity:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->set_angular_velocity(h, cmd.value);
        break;
    case PhysicsCommandKind::SetBodyTransform:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) {
            physics_->set_transform(h, cmd.value, cmd.quat);
            if (Transform* t = world_.get<Transform>(cmd.entity)) {
                t->position = cmd.value;
                t->rotation = cmd.quat;
            }
        }
        break;
    case PhysicsCommandKind::WakeBody:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->wake(h);
        break;
    case PhysicsCommandKind::SleepBody:
        if (const BodyHandle h = resolve_body(cmd.entity); h.valid()) physics_->sleep(h);
        break;
    default:
        apply_joint_command(cmd);
        break;
    }
}

void PhysicsBridge::flush_commands() {
    commands_.drain_sorted(command_scratch_);
    for (const PhysicsCommand& cmd : command_scratch_) apply_command(cmd);
}

} // namespace astral
