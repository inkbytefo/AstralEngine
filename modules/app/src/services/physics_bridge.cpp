#include "physics_bridge.hpp"

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

/// What scripts may do with physics: routes queries and queued commands through PhysicsService.
class AppScriptPhysics final : public ScriptPhysics {
public:
    AppScriptPhysics(World& world, PhysicsService& service, const FieldWorld& fields, const Time& time)
        : world_(world), service_(service), fields_(fields), time_(time) {}

    std::optional<ScriptHit> raycast(Vec3 origin, Vec3 direction, float max_distance, std::uint32_t layers,
                                     Entity ignore) const override {
        const auto hit = service_.raycast(origin, direction, max_distance, {.layers = layers, .ignore = body_of(ignore)});
        return hit ? std::optional<ScriptHit>(to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }
    std::optional<ScriptHit> spherecast(Vec3 origin, float radius, Vec3 displacement, std::uint32_t layers,
                                        Entity ignore) const override {
        const auto hit = service_.spherecast(origin, radius, displacement, {.layers = layers, .ignore = body_of(ignore)});
        return hit ? std::optional<ScriptHit>(to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }
    std::vector<Entity> overlap_sphere(Vec3 center, float radius, std::uint32_t layers) const override {
        return entities(service_.overlap_sphere(center, radius, {.layers = layers}));
    }
    std::vector<Entity> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation, std::uint32_t layers) const override {
        return entities(service_.overlap_box(center, half_extents, rotation, {.layers = layers}));
    }
    void impulse(Entity body, Vec3 v) override { service_.add_impulse(body, v); }
    void force(Entity body, Vec3 v) override { service_.add_force(body, v); }
    void set_velocity(Entity body, Vec3 v) override { service_.set_linear_velocity(body, v); }
    Vec3 velocity(Entity body) const override { return service_.linear_velocity(body); }
    Vec3 wind(Vec3 position) const override { return fields_.wind_at(position, time_.elapsed); }
    ScriptWater water(Vec3 position) const override {
        const WaterSample s = fields_.water_at(position, time_.elapsed);
        return {s.inside, s.surface_height, s.depth, s.flow, s.density};
    }

private:
    BodyHandle body_of(Entity e) const {
        const RigidBody* rb = (e.valid() && world_.alive(e)) ? world_.get<const RigidBody>(e) : nullptr;
        return rb ? rb->handle : BodyHandle{};
    }
    static ScriptHit to_script(std::uint64_t user_data, Vec3 position, Vec3 normal, float distance, int surface) {
        return {entity_from_user_data(user_data), position, normal, distance, surface};
    }
    static std::vector<Entity> entities(const std::vector<OverlapHit>& hits) {
        std::vector<Entity> out;
        out.reserve(hits.size());
        for (const OverlapHit& h : hits) out.push_back(entity_from_user_data(h.user_data));
        return out;
    }

    World& world_;
    PhysicsService& service_;
    const FieldWorld& fields_;
    const Time& time_;
};

Quat nlerp_shortest(Quat from, Quat to, float alpha) {
    const float sign = (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w) < 0 ? -1.0f : 1.0f;
    return normalize(Quat{from.x + (to.x * sign - from.x) * alpha, from.y + (to.y * sign - from.y) * alpha,
                          from.z + (to.z * sign - from.z) * alpha, from.w + (to.w * sign - from.w) * alpha});
}

std::uint64_t default_order_key(Entity e) noexcept {
    return (static_cast<std::uint64_t>(e.index) << 32) | static_cast<std::uint64_t>(e.generation);
}

} // namespace

PhysicsBridge::PhysicsBridge(World& world, FieldWorld& fields, const Time& time, Profiler* profiler, Logger& log,
                             PhysicsBridgeConfig config)
    : world_(world),
      fields_(fields),
      time_(time),
      profiler_(profiler),
      config_(config),
      physics_(std::make_unique<PhysicsWorld>(PhysicsDesc{.logger = &log})),
      script_physics_(std::make_unique<AppScriptPhysics>(world_, *this, fields_, time_)) {}

PhysicsBridge::~PhysicsBridge() {
    script_physics_.reset();
    physics_.reset();
}

std::span<const ContactEvent> PhysicsBridge::contacts() const noexcept {
    // Inside FixedUpdate: the previous completed physics step; elsewhere: every step of this frame.
    return in_fixed_update_ ? physics_->contacts() : std::span<const ContactEvent>(frame_contacts_);
}

PhysicsQueryState PhysicsBridge::query_state() const noexcept {
    return {step_index_, sim_time_, in_fixed_update_};
}

BodyHandle PhysicsBridge::body_of(Entity e) const noexcept {
    if (!e.valid() || !world_.alive(e)) return {};
    const RigidBody* rb = world_.get<const RigidBody>(e);
    if (!rb || !physics_->valid(rb->handle) || physics_->user_data(rb->handle) != to_user_data(e)) return {};
    return rb->handle;
}

std::optional<RayHit> PhysicsBridge::raycast(Vec3 origin, Vec3 direction, float max_distance, QueryFilter filter) const {
    return physics_->raycast(origin, direction, max_distance, filter);
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

void PhysicsBridge::enqueue(Entity body, CommandKind kind, Vec3 value, std::uint64_t order_key) {
    if (!body.valid()) return;
    const std::uint64_t key = order_key != 0 ? order_key : default_order_key(body);
    std::scoped_lock lock(command_mutex_);
    pending_commands_.push_back({key, next_command_seq_++, body, kind, value});
}

void PhysicsBridge::add_impulse(Entity body, Vec3 impulse, std::uint64_t order_key) {
    enqueue(body, CommandKind::Impulse, impulse, order_key);
}

void PhysicsBridge::add_force(Entity body, Vec3 force, std::uint64_t order_key) {
    enqueue(body, CommandKind::Force, force, order_key);
}

void PhysicsBridge::set_linear_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    enqueue(body, CommandKind::SetLinearVelocity, velocity, order_key);
}

void PhysicsBridge::set_angular_velocity(Entity body, Vec3 velocity, std::uint64_t order_key) {
    enqueue(body, CommandKind::SetAngularVelocity, velocity, order_key);
}

std::size_t PhysicsBridge::pending_command_count() const noexcept {
    std::scoped_lock lock(command_mutex_);
    return pending_commands_.size();
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
    for (const QueuedCommand& cmd : batch) {
        const BodyHandle handle = body_of(cmd.entity);
        if (!handle.valid()) continue; // dead entity or entity without a synced RigidBody
        switch (cmd.kind) {
        case CommandKind::Impulse: physics_->add_impulse(handle, cmd.value); break;
        case CommandKind::Force: physics_->add_force(handle, cmd.value); break;
        case CommandKind::SetLinearVelocity: physics_->set_linear_velocity(handle, cmd.value); break;
        case CommandKind::SetAngularVelocity: physics_->set_angular_velocity(handle, cmd.value); break;
        }
    }
}

void PhysicsBridge::shift_origin(Vec3 offset) {
    physics_->shift_origin(offset);
    for (BodyLink& link : body_links_) link.previous_position = link.previous_position - offset;
}

/// Creates bodies/characters for new components, destroys those whose entity or component is gone.
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
    std::erase_if(joint_links_, [&](const JointLink& link) {
        const Joint* j = world_.get<const Joint>(link.entity);
        if (j && j->handle == link.joint && physics_->valid(link.joint)) return false;
        physics_->destroy(link.joint);
        return true;
    });
    world_.each<Joint>([&](Entity e, Joint& j) {
        const bool ours = std::any_of(joint_links_.begin(), joint_links_.end(),
                                      [&](const JointLink& l) { return l.entity == e && l.joint == j.handle; });
        if (ours) return;
        const RigidBody* ra = j.a.valid() ? world_.get<const RigidBody>(j.a) : nullptr;
        const RigidBody* rb = j.b.valid() ? world_.get<const RigidBody>(j.b) : nullptr;
        if (!rb || !physics_->valid(rb->handle) || (j.a.valid() && (!ra || !physics_->valid(ra->handle)))) return;
        j.handle = physics_->create_joint({.type = j.type, .body_a = ra ? ra->handle : BodyHandle{}, .body_b = rb->handle,
                                           .anchor = j.anchor, .anchor_b = j.anchor_b, .axis = j.axis, .limited = j.limited,
                                           .min_limit = j.min_limit, .max_limit = j.max_limit,
                                           .spring_frequency = j.spring_frequency, .spring_damping = j.spring_damping,
                                           .max_friction = j.max_friction, .collide_connected = j.collide_connected});
        if (j.handle.valid()) joint_links_.push_back({e, j.handle});
    });
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

void PhysicsBridge::update_characters(float fixed) {
    world_.each<CharacterController>([&](Entity e, CharacterController& cc) {
        if (physics_->user_data(cc.handle) != to_user_data(e)) return;
        physics_->update_character(cc.handle, cc.move, cc.jump, fixed, cc.jump_speed);
        cc.jump = false;
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

void PhysicsBridge::step(float delta, FixedStepCallback on_fixed_update, void* user) {
    frame_contacts_.clear();
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
        sync_bodies();   // FixedUpdate systems may have spawned or removed bodies
        flush_commands(); // Apply queued C++/Luau commands deterministically after body sync
        const float alpha = planned > 0 ? std::min(1.0f, static_cast<float>(steps + 1) / static_cast<float>(planned)) : 1.0f;
        move_kinematics(alpha, fixed);
        update_characters(fixed);
        if (config_.interpolation) snapshot_interpolation();
        apply_field_forces(fixed);
        {
            ASTRAL_PROFILE_SCOPE(profiler_, "physics.step");
            physics_->step(fixed);
        }
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
