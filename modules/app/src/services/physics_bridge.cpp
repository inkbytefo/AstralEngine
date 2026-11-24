#include "physics_bridge.hpp"
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

Quat nlerp_shortest(Quat from, Quat to, float alpha) {
    const float sign = (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w) < 0 ? -1.0f : 1.0f;
    return normalize(Quat{from.x + (to.x * sign - from.x) * alpha, from.y + (to.y * sign - from.y) * alpha,
                          from.z + (to.z * sign - from.z) * alpha, from.w + (to.w * sign - from.w) * alpha});
}

std::uint64_t default_order_key(Entity e) noexcept {
    return (static_cast<std::uint64_t>(e.index) << 32) | static_cast<std::uint64_t>(e.generation);
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

PhysicsQueryState PhysicsBridge::query_state() const noexcept {
    return {step_index_, sim_time_, in_fixed_update_};
}

PhysicsCapabilities PhysicsBridge::capabilities() const noexcept { return physics_->capabilities(); }

BodyHandle PhysicsBridge::body_of(Entity e) const noexcept {
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
    const BodyHandle handle = body_of(body);
    return handle.valid() ? physics_->linear_velocity(handle) : Vec3{};
}

void PhysicsBridge::enqueue(QueuedCommand cmd) {
    if (!cmd.entity.valid()) return;
    if (cmd.order_key == 0) cmd.order_key = default_order_key(cmd.entity);
    std::scoped_lock lock(command_mutex_);
    cmd.sequence = next_command_seq_++;
    pending_commands_.push_back(cmd);
}

void PhysicsBridge::add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::Impulse, .value = impulse});
}
void PhysicsBridge::add_force(Entity body, Vec3 force, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::Force, .value = force});
}
void PhysicsBridge::set_linear_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::SetLinearVelocity, .value = velocity});
}
void PhysicsBridge::set_angular_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::SetAngularVelocity, .value = velocity});
}
void PhysicsBridge::set_body_transform(Entity body, Vec3 position, Quat rotation, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::SetBodyTransform, .value = position, .quat = rotation});
}
void PhysicsBridge::wake_body(Entity body, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::WakeBody});
}
void PhysicsBridge::sleep_body(Entity body, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = body, .kind = CommandKind::SleepBody});
}
void PhysicsBridge::set_joint_motor_mode(Entity joint, JointMotorMode mode, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::SetJointMotorMode, .motor_mode = mode});
}
void PhysicsBridge::set_joint_motor_velocity(Entity joint, float velocity, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::SetJointMotorVelocity, .scalar = velocity});
}
void PhysicsBridge::set_joint_motor_position(Entity joint, float position, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::SetJointMotorPosition, .scalar = position});
}
void PhysicsBridge::set_joint_motor_angular_velocity(Entity joint, Vec3 velocity, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::SetJointMotorAngularVelocity, .value = velocity});
}
void PhysicsBridge::set_joint_motor_rotation(Entity joint, Quat rotation, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::SetJointMotorRotation, .quat = rotation});
}
void PhysicsBridge::break_joint(Entity joint, std::uint64_t order_key) {
    enqueue({.order_key = order_key, .entity = joint, .kind = CommandKind::BreakJoint});
}

std::size_t PhysicsBridge::pending_command_count() const noexcept {
    std::scoped_lock lock(command_mutex_);
    return pending_commands_.size();
}

void PhysicsBridge::apply_command(const QueuedCommand& cmd) {
    switch (cmd.kind) {
    case CommandKind::Impulse:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->add_impulse(h, cmd.value);
        break;
    case CommandKind::Force:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->add_force(h, cmd.value);
        break;
    case CommandKind::SetLinearVelocity:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->set_linear_velocity(h, cmd.value);
        break;
    case CommandKind::SetAngularVelocity:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->set_angular_velocity(h, cmd.value);
        break;
    case CommandKind::SetBodyTransform:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) {
            physics_->set_transform(h, cmd.value, cmd.quat);
            if (Transform* t = world_.get<Transform>(cmd.entity)) {
                t->position = cmd.value;
                t->rotation = cmd.quat;
            }
        }
        break;
    case CommandKind::WakeBody:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->wake(h);
        break;
    case CommandKind::SleepBody:
        if (const BodyHandle h = body_of(cmd.entity); h.valid()) physics_->sleep(h);
        break;
    case CommandKind::SetJointMotorMode:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->set_joint_motor_mode(jh, cmd.motor_mode);
            if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) m->mode = cmd.motor_mode;
        }
        break;
    case CommandKind::SetJointMotorVelocity:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->set_joint_motor_velocity(jh, cmd.scalar);
            if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) m->target_velocity = cmd.scalar;
        }
        break;
    case CommandKind::SetJointMotorPosition:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->set_joint_motor_position(jh, cmd.scalar);
            if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) m->target_position = cmd.scalar;
        }
        break;
    case CommandKind::SetJointMotorAngularVelocity:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->set_joint_motor_angular_velocity(jh, cmd.value);
            if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) m->target_angular_velocity = cmd.value;
        }
        break;
    case CommandKind::SetJointMotorRotation:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->set_joint_motor_rotation(jh, cmd.quat);
            if (JointMotor* m = world_.get<JointMotor>(cmd.entity)) m->target_rotation = cmd.quat;
        }
        break;
    case CommandKind::BreakJoint:
        if (const JointHandle jh = joint_of(cmd.entity); jh.valid()) {
            physics_->break_joint(jh);
            if (Joint* j = world_.get<Joint>(cmd.entity)) {
                j->broken = true;
                frame_broken_joints_.push_back({cmd.entity, j->a, j->b, 0.0f, 0.0f});
            }
        }
        break;
    }
}

void PhysicsBridge::flush_commands() {
    std::vector<QueuedCommand> batch;
    {
        std::scoped_lock lock(command_mutex_);
        batch.swap(pending_commands_);
    }
    if (batch.empty()) return;
    std::sort(batch.begin(), batch.end(), [](const QueuedCommand& a, const QueuedCommand& b) {
        return a.order_key != b.order_key ? a.order_key < b.order_key : a.sequence < b.sequence;
    });
    for (const QueuedCommand& cmd : batch) apply_command(cmd);
}

void PhysicsBridge::shift_origin(Vec3 offset) {
    physics_->shift_origin(offset);
    for (BodyLink& link : body_links_) {
        link.previous_position = link.previous_position - offset;
        link.step_position = link.step_position - offset;
    }
    world_.each<Joint>([&](Joint& j) {
        j.anchor = j.anchor - offset;
        j.anchor_b = j.anchor_b - offset;
    });
}

void PhysicsBridge::sync_joints() {
    std::erase_if(joint_links_, [&](const JointLink& link) {
        Joint* j = world_.get<Joint>(link.entity);
        if (!j || j->handle != link.joint) {
            physics_->destroy(link.joint);
            return true;
        }
        if (j->broken) {
            physics_->destroy(link.joint);
            j->handle = {};
            return true;
        }
        if (!physics_->valid(link.joint)) {
            // A connected body was destroyed or streamed out while the Joint entity stayed alive -> streaming detach.
            j->handle = {};
            world_.send(JointDetached{link.entity, link.a, link.b});
            return true;
        }
        return false;
    });
    world_.each<Joint>([&](Entity e, Joint& j) {
        if (j.broken) return;
        const bool ours = std::any_of(joint_links_.begin(), joint_links_.end(),
                                      [&](const JointLink& l) { return l.entity == e && l.joint == j.handle; });
        if (ours) return;
        const BodyHandle ha = j.a.valid() ? body_of(j.a) : BodyHandle{};
        const BodyHandle hb = body_of(j.b);
        if (!hb.valid() || (j.a.valid() && !ha.valid())) return;
        const JointMotor* motor = world_.get<const JointMotor>(e);
        j.handle = physics_->create_joint(build_joint_desc(j, motor, ha, hb));
        if (j.handle.valid()) joint_links_.push_back({e, j.handle, j.a, j.b});
    });
}

void PhysicsBridge::sync_joint_motors() {
    world_.each<const Joint, const JointMotor>([&](const Joint& j, const JointMotor& m) {
        if (j.broken || !physics_->valid(j.handle)) return;
        physics_->set_joint_motor_tuning(j.handle, m.frequency, m.damping, m.max_force, m.max_torque);
        physics_->set_joint_motor_mode(j.handle, m.mode);
        if (m.mode == JointMotorMode::Velocity) {
            physics_->set_joint_motor_velocity(j.handle, m.target_velocity);
            physics_->set_joint_motor_angular_velocity(j.handle, m.target_angular_velocity);
        } else if (m.mode == JointMotorMode::Position) {
            physics_->set_joint_motor_position(j.handle, m.target_position);
            physics_->set_joint_motor_rotation(j.handle, m.target_rotation);
        }
    });
}

void PhysicsBridge::collect_broken_joints() {
    for (const JointBrokenEvent& ev : physics_->broken_joints()) {
        for (const JointLink& link : joint_links_) {
            if (link.joint != ev.joint) continue;
            if (Joint* j = world_.get<Joint>(link.entity)) j->broken = true;
            frame_broken_joints_.push_back({link.entity, link.a, link.b, ev.force, ev.torque});
            break;
        }
    }
}

/// Creates bodies/characters/joints for new components, destroys those whose entity or component is gone.
void PhysicsBridge::sync_bodies() {
    std::erase_if(body_links_, [&](const BodyLink& link) {
        const RigidBody* rb = world_.get<RigidBody>(link.entity);
        if (rb && rb->handle == link.body) return false;
        physics_->destroy(link.body);
        return true;
    });
    std::erase_if(character_links_, [&](const CharacterLink& link) {
        const CharacterController* cc = world_.get<CharacterController>(link.entity);
        if (cc && cc->handle == link.character) return false;
        physics_->destroy(link.character);
        return true;
    });
    world_.each<RigidBody, const Transform>([&](Entity e, RigidBody& rb, const Transform& t) {
        if (physics_->valid(rb.handle) && physics_->user_data(rb.handle) == to_user_data(e)) return;
        BodyDesc body_desc = rb.desc;
        body_desc.position = t.position;
        body_desc.rotation = t.rotation;
        body_desc.user_data = to_user_data(e);
        rb.handle = physics_->create_body(body_desc);
        if (rb.handle.valid()) body_links_.push_back({e, rb.handle, t.position, t.rotation, t.position, t.rotation});
    });
    sync_joints();
    world_.each<CharacterController, const Transform>([&](Entity e, CharacterController& cc, const Transform& t) {
        if (physics_->valid(cc.handle) && physics_->user_data(cc.handle) == to_user_data(e)) return;
        CharacterDesc character_desc = cc.desc;
        character_desc.position = t.position;
        character_desc.user_data = to_user_data(e);
        cc.handle = physics_->create_character(character_desc);
        if (cc.handle.valid()) character_links_.push_back({e, cc.handle});
    });
}

void PhysicsBridge::update_fields() {
    fields_.clear_volumes();
    fields_.set_global_wind({});
    const auto centre = [&](Entity e) {
        if (const WorldTransform* w = world_.get<const WorldTransform>(e)) return Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        const Transform* t = world_.get<const Transform>(e);
        return t ? t->position : Vec3{};
    };
    world_.each<const WindZone>([&](Entity e, const WindZone& z) {
        const WindDesc wind{.velocity = z.velocity, .gust_strength = z.gust_strength, .gust_frequency = z.gust_frequency,
                            .turbulence = z.turbulence, .turbulence_scale = z.turbulence_scale, .seed = z.seed};
        if (z.global) fields_.set_global_wind(wind);
        else fields_.add_wind_volume({.center = centre(e), .half_extents = z.half_extents, .wind = wind,
                                      .falloff = z.falloff, .replace = z.replace});
    });
    world_.each<const WaterBody>([&](Entity e, const WaterBody& w) {
        fields_.add_water_volume({.center = centre(e), .half_extents = w.half_extents, .density = w.density, .flow = w.flow,
                                  .wave_height = w.wave_height, .wave_length = w.wave_length, .wave_speed = w.wave_speed});
    });
}

void PhysicsBridge::move_kinematics(float alpha, float fixed) {
    for (const BodyLink& link : body_links_) {
        const RigidBody* rb = world_.get<RigidBody>(link.entity);
        const Transform* t = world_.get<Transform>(link.entity);
        if (!rb || !t || rb->desc.motion != MotionType::Kinematic) continue;
        const Vec3 target = lerp(link.previous_position, t->position, alpha);
        const Quat rotation = nlerp_shortest(link.previous_rotation, t->rotation, alpha);
        physics_->move_kinematic(rb->handle, target, rotation, fixed);
    }
}

void PhysicsBridge::update_characters(float fixed, const TerrainBridge* terrain) {
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
    for (BodyLink& link : body_links_) {
        link.step_position = physics_->position(link.body);
        link.step_rotation = physics_->rotation(link.body);
    }
}

void PhysicsBridge::apply_field_forces(float dt) {
    const double now = time_.elapsed;
    const float gravity = length(physics_->gravity());
    const auto body_of_rb = [&](const RigidBody& rb) { return rb.desc.motion == MotionType::Dynamic && physics_->valid(rb.handle); };
    world_.each<const RigidBody, const Buoyant>([&](const RigidBody& rb, const Buoyant& b) {
        if (!body_of_rb(rb)) return;
        const Vec3 p = physics_->position(rb.handle);
        const Vec3 v = physics_->linear_velocity(rb.handle);
        const float height = std::max(b.height, 0.01f);
        const WaterSample water = fields_.water_at({p.x, p.y - 0.5f * height, p.z}, now);
        if (!water.inside) return;
        const float fraction = std::clamp(water.depth / height, 0.0f, 1.0f);
        const float mass = physics_->mass(rb.handle);
        const float volume = b.volume > 0.0f ? b.volume : mass / 500.0f;
        const Vec3 up = gravity > 0.0f ? normalize(Vec3{0, 1, 0}) : Vec3{0, 1, 0};
        physics_->add_force(rb.handle, up * (water.density * gravity * volume * fraction));
        const Vec3 relative = water.flow - v;
        physics_->add_force(rb.handle, relative * (0.5f * water.density * b.drag * b.area * length(relative) * fraction));
        const Vec3 spin = physics_->angular_velocity(rb.handle);
        physics_->set_angular_velocity(rb.handle, spin * std::max(0.0f, 1.0f - b.angular_drag * fraction * dt));
    });
    world_.each<const RigidBody, const AirDrag>([&](Entity e, const RigidBody& rb, const AirDrag& d) {
        if (!body_of_rb(rb)) return;
        const Vec3 p = physics_->position(rb.handle);
        float in_air = 1.0f;
        if (const Buoyant* b = world_.get<const Buoyant>(e)) {
            const WaterSample water = fields_.water_at({p.x, p.y - 0.5f * std::max(b->height, 0.01f), p.z}, now);
            if (water.inside) in_air = 1.0f - std::clamp(water.depth / std::max(b->height, 0.01f), 0.0f, 1.0f);
        }
        const Vec3 relative = fields_.wind_at(p, now) - physics_->linear_velocity(rb.handle);
        physics_->add_force(rb.handle, relative * (0.5f * air_density * d.drag * d.area * length(relative) * in_air));
    });
    world_.each<const RigidBody, const Airfoil>([&](const RigidBody& rb, const Airfoil& wing) {
        if (!body_of_rb(rb)) return;
        const Vec3 p = physics_->position(rb.handle);
        const Quat q = physics_->rotation(rb.handle);
        const Vec3 flow = fields_.wind_at(p, now) - physics_->linear_velocity(rb.handle);
        const float speed = length(flow);
        if (speed < 0.1f) return;
        const Vec3 forward = q * normalize(wing.forward), up = q * normalize(wing.up);
        const float alpha = std::atan2(dot(flow, up), -dot(flow, forward));
        const float stall = std::max(wing.stall_angle, 0.01f);
        float coefficient = wing.lift_slope * std::clamp(alpha, -stall, stall);
        if (std::abs(alpha) > stall) coefficient *= std::clamp(1.0f - (std::abs(alpha) - stall) / stall, 0.0f, 1.0f);
        const Vec3 direction = flow / speed;
        const Vec3 lift_axis = normalize(up - direction * dot(up, direction));
        physics_->add_force(rb.handle, lift_axis * (0.5f * air_density * speed * speed * wing.area * coefficient));
    });
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
            const Entity trigger = a_is ? a : b, other = a_is ? b : a;
            if (!trigger.valid() || !other.valid()) continue;
            if (begin) world_.send(TriggerEntered{trigger, other});
            else world_.send(TriggerExited{trigger, other});
            continue;
        }
        world_.send(Contact{begin ? ContactKind::Begin : ContactKind::End, a, b, c.point, c.normal, c.surface_a, c.surface_b});
    }
    for (const JointBroken& jb : frame_broken_joints_) world_.send(jb);
}

void PhysicsBridge::write_simulated_transforms(float blend) {
    world_.each<const RigidBody, Transform>([&](const RigidBody& rb, Transform& t) {
        if (rb.desc.motion != MotionType::Dynamic || !physics_->valid(rb.handle)) return;
        t.position = physics_->position(rb.handle);
        t.rotation = physics_->rotation(rb.handle);
    });
    if (config_.interpolation) {
        for (const BodyLink& link : body_links_) {
            Transform* t = world_.get<Transform>(link.entity);
            const RigidBody* rb = world_.get<RigidBody>(link.entity);
            if (!t || !rb || rb->desc.motion != MotionType::Dynamic || !physics_->valid(rb->handle)) continue;
            t->position = lerp(link.step_position, t->position, blend);
            t->rotation = nlerp_shortest(link.step_rotation, t->rotation, blend);
        }
    }
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
    const float fixed = time_.fixed_delta;
    const std::uint32_t max_steps = std::max(config_.max_steps, 1u);
    accumulator_ += delta;
    const auto planned = static_cast<std::uint32_t>(std::min<float>(accumulator_ / fixed, static_cast<float>(max_steps)));
    std::uint32_t steps = 0;
    while (accumulator_ >= fixed && steps < max_steps) {
        update_fields();
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
        apply_field_forces(fixed);
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
        for (BodyLink& link : body_links_) {
            if (const Transform* t = world_.get<Transform>(link.entity)) {
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
