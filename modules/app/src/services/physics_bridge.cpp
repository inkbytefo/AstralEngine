#include "physics_bridge.hpp"
#include "physics_fields.hpp"
#include "terrain_bridge.hpp"

#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/script/script.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace astral {

namespace {

std::uint64_t handle_key(JointHandle h) noexcept {
    return (static_cast<std::uint64_t>(h.generation) << 32) | h.index;
}

Quat nlerp_shortest(Quat from, Quat to, float alpha) {
    const float sign = (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w) < 0 ? -1.0f : 1.0f;
    return normalize(Quat{from.x + (to.x * sign - from.x) * alpha, from.y + (to.y * sign - from.y) * alpha,
                          from.z + (to.z * sign - from.z) * alpha, from.w + (to.w * sign - from.w) * alpha});
}

bool motors_equal(const JointMotor& a, const JointMotor& b) noexcept {
    return a.mode == b.mode && a.frequency == b.frequency && a.damping == b.damping &&
           a.max_force == b.max_force && a.max_torque == b.max_torque &&
           a.target_velocity == b.target_velocity && a.target_position == b.target_position &&
           a.target_angular_velocity == b.target_angular_velocity && a.target_rotation == b.target_rotation;
}

JointDesc build_joint_desc(const Joint& j, const JointMotor* m, BodyHandle a, BodyHandle b) {
    JointDesc d{};
    d.type = j.type;
    d.body_a = a;
    d.body_b = b;
    d.anchor = j.anchor;
    d.anchor_b = j.anchor_b;
    d.axis = j.axis;
    d.secondary_axis = j.secondary_axis;
    d.limited = j.limited;
    d.min_limit = j.min_limit;
    d.max_limit = j.max_limit;
    d.spring_frequency = j.spring_frequency;
    d.spring_damping = j.spring_damping;
    d.max_friction = j.max_friction;
    d.collide_connected = j.collide_connected;
    d.break_force = j.break_force;
    d.break_torque = j.break_torque;
    d.limits.linear_min = j.linear_limit_min;
    d.limits.linear_max = j.linear_limit_max;
    d.limits.swing_normal_cone = j.swing_limits.x;
    d.limits.swing_plane_cone = j.swing_limits.y;
    d.limits.twist_min = j.min_limit;
    d.limits.twist_max = j.max_limit;
    if (j.type == JointType::SixDOF && j.limited) {
        d.limits.angular_min = {j.min_limit, -j.swing_limits.x, -j.swing_limits.y};
        d.limits.angular_max = {j.max_limit, j.swing_limits.x, j.swing_limits.y};
    }
    if (m) {
        d.motor.mode = m->mode;
        d.motor.frequency_hz = m->frequency;
        d.motor.damping = m->damping;
        d.motor.max_force = m->max_force;
        d.motor.max_torque = m->max_torque;
        d.motor.target_velocity = m->target_velocity;
        d.motor.target_position = m->target_position;
        d.motor.target_angular_velocity = m->target_angular_velocity;
        d.motor.target_rotation = m->target_rotation;
    }
    return d;
}

template <class LinkVec>
void prune_body_links(LinkVec& links, World& world, PhysicsWorld& physics, MotionType expected_motion) {
    std::size_t i = 0;
    while (i < links.size()) {
        const RigidBody* rb = world.get<const RigidBody>(links[i].entity);
        if (rb && rb->handle == links[i].body && rb->desc.motion == expected_motion) {
            ++i;
            continue;
        }
        physics.destroy(links[i].body);
        links[i] = links.back();
        links.pop_back();
    }
}

} // namespace

PhysicsBridge::PhysicsBridge(World& world, FieldWorld& fields, const Time& time, Profiler* profiler, Logger& log,
                             PhysicsBridgeConfig config)
    : world_(world),
      fields_(fields),
      time_(time),
      profiler_(profiler),
      jobs_(config.jobs),
      config_(config),
      physics_(std::make_unique<PhysicsWorld>(PhysicsDesc{.logger = &log})),
      script_physics_(make_app_script_physics(world_, *this, fields_, time_)) {}

PhysicsBridge::~PhysicsBridge() {
    script_physics_.reset();
    physics_.reset();
}

std::span<const ContactEvent> PhysicsBridge::contacts() const noexcept {
    return in_fixed_update_ ? physics_->contacts() : std::span<const ContactEvent>(frame_contacts_);
}

PhysicsQueryState PhysicsBridge::query_state() const noexcept { return {step_index_, sim_time_, in_fixed_update_}; }

PhysicsCapabilities PhysicsBridge::capabilities() const noexcept { return physics_->capabilities(); }

BodyHandle PhysicsBridge::resolve_body(Entity e) const noexcept {
    if (!e.valid() || !world_.alive(e)) return {};
    const RigidBody* rb = world_.get<const RigidBody>(e);
    if (!rb || !physics_->valid(rb->handle) || physics_->user_data(rb->handle) != to_user_data(e)) return {};
    return rb->handle;
}

JointHandle PhysicsBridge::joint_of(Entity e) const noexcept {
    if (!e.valid() || !world_.alive(e)) return {};
    const Joint* j = world_.get<const Joint>(e);
    if (!j || j->broken || !physics_->valid(j->handle)) return {};
    return j->handle;
}

PhysicsBridge::JointLink* PhysicsBridge::find_joint_link(Entity e) noexcept {
    const auto it = links_.joint_by_entity.find(to_user_data(e));
    return it != links_.joint_by_entity.end() ? &links_.joints[it->second] : nullptr;
}

void PhysicsBridge::add_joint_link(JointLink link) {
    const std::size_t idx = links_.joints.size();
    links_.joint_by_entity[to_user_data(link.entity)] = idx;
    links_.joint_by_handle[handle_key(link.joint)] = idx;
    links_.joints.push_back(link);
}

void PhysicsBridge::remove_joint_link(std::size_t idx) {
    const JointLink removed = links_.joints[idx];
    links_.joint_by_entity.erase(to_user_data(removed.entity));
    links_.joint_by_handle.erase(handle_key(removed.joint));
    const std::size_t last = links_.joints.size() - 1;
    if (idx != last) {
        links_.joints[idx] = links_.joints[last];
        links_.joint_by_entity[to_user_data(links_.joints[idx].entity)] = idx;
        links_.joint_by_handle[handle_key(links_.joints[idx].joint)] = idx;
    }
    links_.joints.pop_back();
}

void PhysicsBridge::shift_origin(Vec3 offset) {
    physics_->shift_origin(offset);
    commands_.shift_positions(offset); // teleports queued in the old coordinates
    for (KinematicBodyLink& link : links_.kinematics) link.previous_position = link.previous_position - offset;
    for (DynamicBodyLink& link : links_.dynamics) link.step_position = link.step_position - offset;
    world_.each<Joint>([&](Joint& j) {
        j.anchor = j.anchor - offset;
        j.anchor_b = j.anchor_b - offset;
    });
}

void PhysicsBridge::sync_joints() {
    std::size_t i = 0;
    while (i < links_.joints.size()) {
        const JointLink& link = links_.joints[i];
        Joint* j = world_.get<Joint>(link.entity);
        if (!j || j->handle != link.joint) {
            physics_->destroy(link.joint);
            remove_joint_link(i);
            continue;
        }
        if (j->broken) {
            physics_->destroy(link.joint);
            j->handle = {};
            remove_joint_link(i);
            continue;
        }
        if (!physics_->valid(link.joint)) {
            // A connected body was destroyed or streamed out while the Joint entity stayed alive -> streaming detach.
            j->handle = {};
            world_.send(JointDetached{link.entity, link.a, link.b});
            remove_joint_link(i);
            continue;
        }
        ++i;
    }
    world_.each<Joint>([&](Entity e, Joint& j) {
        if (j.broken) return;
        if (const JointLink* existing = find_joint_link(e); existing && existing->joint == j.handle) return;
        const BodyHandle ha = j.a.valid() ? resolve_body(j.a) : BodyHandle{};
        const BodyHandle hb = resolve_body(j.b);
        if (!hb.valid() || (j.a.valid() && !ha.valid())) return;
        const JointMotor* motor = world_.get<const JointMotor>(e);
        j.handle = physics_->create_joint(build_joint_desc(j, motor, ha, hb));
        if (j.handle.valid()) {
            add_joint_link({e, j.handle, j.a, j.b, motor != nullptr, motor ? *motor : JointMotor{}});
        }
    });
}

void PhysicsBridge::sync_joint_motors() {
    for (JointLink& link : links_.joints) {
        const Joint* j = world_.get<const Joint>(link.entity);
        if (!j || j->broken || !physics_->valid(link.joint)) continue;
        const JointMotor* m = world_.get<const JointMotor>(link.entity);
        if (!m) {
            if (link.motor_synced) {
                physics_->set_joint_motor_mode(link.joint, JointMotorMode::Off);
                link.motor_synced = false;
            }
            continue;
        }
        if (link.motor_synced && motors_equal(link.last_motor, *m)) continue;
        physics_->set_joint_motor_tuning(link.joint, m->frequency, m->damping, m->max_force, m->max_torque);
        physics_->set_joint_motor_mode(link.joint, m->mode);
        if (m->mode == JointMotorMode::Velocity) {
            physics_->set_joint_motor_velocity(link.joint, m->target_velocity);
            physics_->set_joint_motor_angular_velocity(link.joint, m->target_angular_velocity);
        } else if (m->mode == JointMotorMode::Position) {
            physics_->set_joint_motor_position(link.joint, m->target_position);
            physics_->set_joint_motor_rotation(link.joint, m->target_rotation);
        }
        link.last_motor = *m;
        link.motor_synced = true;
    }
}

void PhysicsBridge::collect_broken_joints() {
    for (const JointBrokenEvent& ev : physics_->broken_joints()) {
        const auto it = links_.joint_by_handle.find(handle_key(ev.joint));
        if (it == links_.joint_by_handle.end()) continue;
        const JointLink& link = links_.joints[it->second];
        if (Joint* j = world_.get<Joint>(link.entity)) j->broken = true;
        frame_broken_joints_.push_back({link.entity, link.a, link.b, ev.force, ev.torque});
    }
}

void PhysicsBridge::sync_bodies() {
    prune_body_links(links_.statics, world_, *physics_, MotionType::Static);
    prune_body_links(links_.kinematics, world_, *physics_, MotionType::Kinematic);
    prune_body_links(links_.dynamics, world_, *physics_, MotionType::Dynamic);

    std::size_t ci = 0;
    while (ci < links_.characters.size()) {
        const CharacterController* cc = world_.get<const CharacterController>(links_.characters[ci].entity);
        if (cc && cc->handle == links_.characters[ci].character) {
            ++ci;
            continue;
        }
        physics_->destroy(links_.characters[ci].character);
        links_.characters[ci] = links_.characters.back();
        links_.characters.pop_back();
    }

    world_.each<RigidBody, const Transform>([&](Entity e, RigidBody& rb, const Transform& t) {
        if (physics_->valid(rb.handle) && physics_->user_data(rb.handle) == to_user_data(e)) return;
        BodyDesc body_desc = rb.desc;
        body_desc.position = t.position;
        body_desc.rotation = t.rotation;
        body_desc.user_data = to_user_data(e);
        rb.handle = physics_->create_body(body_desc);
        if (!rb.handle.valid()) return;
        if (rb.desc.motion == MotionType::Static) links_.statics.push_back({e, rb.handle});
        else if (rb.desc.motion == MotionType::Kinematic) links_.kinematics.push_back({e, rb.handle, t.position, t.rotation});
        else links_.dynamics.push_back({e, rb.handle, t.position, t.rotation});
    });

    sync_joints();

    world_.each<CharacterController, const Transform>([&](Entity e, CharacterController& cc, const Transform& t) {
        if (physics_->valid(cc.handle) && physics_->user_data(cc.handle) == to_user_data(e)) return;
        CharacterDesc character_desc = cc.desc;
        character_desc.position = t.position;
        character_desc.user_data = to_user_data(e);
        cc.handle = physics_->create_character(character_desc);
        if (cc.handle.valid()) links_.characters.push_back({e, cc.handle});
    });
}

void PhysicsBridge::move_kinematics(float alpha, float fixed) {
    for (const KinematicBodyLink& link : links_.kinematics) {
        const Transform* t = world_.get<const Transform>(link.entity);
        if (!t) continue;
        const Vec3 target = lerp(link.previous_position, t->position, alpha);
        const Quat rotation = nlerp_shortest(link.previous_rotation, t->rotation, alpha);
        physics_->move_kinematic(link.body, target, rotation, fixed);
    }
}

void PhysicsBridge::update_characters(float fixed, const TerrainBridge* terrain) {
    if (links_.characters.empty()) return;
    world_.each<CharacterController>([&](Entity e, CharacterController& cc) {
        if (physics_->user_data(cc.handle) != to_user_data(e)) return;
        Vec3 move = cc.move;
        if (terrain) {
            const Vec3 position = physics_->character_state(cc.handle).position;
            if (!terrain->character_tiles_ready(position, position, 0.0f)) return;
            const Vec3 target = position + Vec3{move.x, 0, move.z} * fixed;
            if (!terrain->character_tiles_ready(position, target, cc.desc.radius)) move = {};
        }
        physics_->update_character(cc.handle, move, cc.jump, fixed, cc.jump_speed);
        cc.jump = false;
        const CharacterState state = physics_->character_state(cc.handle);
        cc.on_ground = state.on_ground;
        cc.velocity = state.velocity;
    });
}

void PhysicsBridge::snapshot_interpolation() {
    for (DynamicBodyLink& link : links_.dynamics) {
        link.step_position = physics_->position(link.body);
        link.step_rotation = physics_->rotation(link.body);
    }
}

void PhysicsBridge::emit_contact_events() {
    for (const ContactEvent& c : frame_contacts_) {
        const Entity a = c.user_data_a ? entity_from_user_data(c.user_data_a) : Entity{};
        const Entity b = c.user_data_b ? entity_from_user_data(c.user_data_b) : Entity{};
        const bool begin = c.type == ContactEvent::Type::Begin;
        if (c.sensor) {
            const auto is_sensor = [&](Entity e) {
                const RigidBody* rb = e.valid() ? world_.get<const RigidBody>(e) : nullptr;
                return rb && rb->desc.sensor;
            };
            const bool a_is = is_sensor(a);
            const Entity trigger = a_is ? a : b;
            const Entity other = a_is ? b : a;
            if (!trigger.valid() || !other.valid()) continue;
            if (begin) world_.send(TriggerEntered{trigger, other});
            else world_.send(TriggerExited{trigger, other});
            continue;
        }
        world_.send(
            Contact{begin ? ContactKind::Begin : ContactKind::End, a, b, c.point, c.normal, c.surface_a, c.surface_b});
    }
    for (const JointBroken& jb : frame_broken_joints_) world_.send(jb);
}

void PhysicsBridge::write_simulated_transforms(float blend) {
    const bool interpolate = config_.interpolation;
    for (const DynamicBodyLink& link : links_.dynamics) {
        if (!physics_->valid(link.body)) continue;
        const Transform* cur = world_.get<const Transform>(link.entity);
        if (!cur) continue;
        Vec3 pos = physics_->position(link.body);
        Quat rot = physics_->rotation(link.body);
        if (interpolate) {
            pos = lerp(link.step_position, pos, blend);
            rot = nlerp_shortest(link.step_rotation, rot, blend);
        }
        if (cur->position != pos || cur->rotation != rot) {
            if (Transform* t = world_.get<Transform>(link.entity)) {
                t->position = pos;
                t->rotation = rot;
            }
        }
    }
    if (links_.characters.empty()) return;
    world_.each<CharacterController, Transform>([&](Entity e, CharacterController& cc, Transform& t) {
        if (!physics_->valid(cc.handle) || physics_->user_data(cc.handle) != to_user_data(e)) return;
        const CharacterState state = physics_->character_state(cc.handle);
        t.position = state.position;
        cc.on_ground = state.on_ground;
        cc.velocity = state.velocity;
    });
}

void PhysicsBridge::step(float delta, FixedStepCallback on_fixed_update, void* user, const TerrainBridge* terrain) {
    frame_contacts_.clear();
    frame_broken_joints_.clear();
    sync_bodies();
    if (!std::isfinite(delta) || delta < 0.0f) delta = 0.0f;
    const float fixed = time_.fixed_delta > 0.0f ? time_.fixed_delta : (1.0f / 60.0f);
    const std::uint32_t max_steps = std::max(config_.max_steps, 1u);
    accumulator_ += delta;
    const auto planned = static_cast<std::uint32_t>(std::min<float>(accumulator_ / fixed, static_cast<float>(max_steps)));
    std::uint32_t steps = 0;
    while (accumulator_ >= fixed && steps < max_steps) {
        rebuild_field_world(world_, fields_);
        in_fixed_update_ = true;
        if (on_fixed_update) on_fixed_update(user);
        in_fixed_update_ = false;
        world_.apply_commands();
        sync_bodies();
        sync_joint_motors();
        flush_commands();
        const float alpha = planned > 0 ? std::min(1.0f, static_cast<float>(steps + 1) / static_cast<float>(planned)) : 1.0f;
        move_kinematics(alpha, fixed);
        update_characters(fixed, terrain);
        if (config_.interpolation) snapshot_interpolation();
        // Simulation time, not frame time: several steps in one frame sample wind/waves at their own t.
        apply_physics_field_forces(world_, *physics_, fields_, sim_time_, fixed);
        {
            ASTRAL_PROFILE_SCOPE(profiler_, "physics.step");
            physics_->step(fixed);
        }
        collect_broken_joints();
        ++step_index_;
        sim_time_ += static_cast<double>(fixed);
        frame_contacts_.insert(frame_contacts_.end(), physics_->contacts().begin(), physics_->contacts().end());
        accumulator_ -= fixed;
        ++steps;
    }
    if (steps == max_steps) accumulator_ = std::min(accumulator_, fixed);
    if (steps > 0) {
        for (KinematicBodyLink& link : links_.kinematics) {
            if (const Transform* t = world_.get<const Transform>(link.entity)) {
                link.previous_position = t->position;
                link.previous_rotation = t->rotation;
            }
        }
    }
    emit_contact_events();
    const float blend = config_.interpolation ? std::clamp(accumulator_ / fixed, 0.0f, 1.0f) : 1.0f;
    write_simulated_transforms(blend);
}

} // namespace astral
