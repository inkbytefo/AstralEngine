#include "physics_fields.hpp"

#include <astral/app/components.hpp>
#include <astral/physics/fields.hpp>
#include <astral/physics/physics.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

namespace {

bool is_active_dynamic(const PhysicsWorld& physics, const RigidBody& rb) noexcept {
    return rb.desc.motion == MotionType::Dynamic && physics.valid(rb.handle);
}

Vec3 entity_centre(World& world, Entity e) noexcept {
    if (const WorldTransform* w = world.get<const WorldTransform>(e)) {
        return {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
    }
    const Transform* t = world.get<const Transform>(e);
    return t ? t->position : Vec3{};
}

void apply_buoyancy(World& world, PhysicsWorld& physics, const FieldWorld& fields, double now, float dt) {
    if (fields.water_volumes().empty()) return;
    const float gravity = length(physics.gravity());
    const Vec3 up{0.0f, 1.0f, 0.0f};
    world.each<const RigidBody, const Buoyant>([&](const RigidBody& rb, const Buoyant& b) {
        if (!is_active_dynamic(physics, rb)) return;
        const Vec3 p = physics.position(rb.handle);
        const float height = std::max(b.height, 0.01f);
        const WaterSample water = fields.water_at({p.x, p.y - 0.5f * height, p.z}, now);
        if (!water.inside) return;

        const float fraction = std::clamp(water.depth / height, 0.0f, 1.0f);
        const float mass = physics.mass(rb.handle);
        const float volume = b.volume > 0.0f ? b.volume : mass / 500.0f;
        if (gravity > 0.0f) physics.add_force(rb.handle, up * (water.density * gravity * volume * fraction));

        const Vec3 v = physics.linear_velocity(rb.handle);
        const Vec3 relative = water.flow - v;
        physics.add_force(rb.handle, relative * (0.5f * water.density * b.drag * b.area * length(relative) * fraction));

        const Vec3 spin = physics.angular_velocity(rb.handle);
        physics.set_angular_velocity(rb.handle, spin * std::max(0.0f, 1.0f - b.angular_drag * fraction * dt));
    });
}

void apply_air_drag(World& world, PhysicsWorld& physics, const FieldWorld& fields, double now) {
    const bool has_water = !fields.water_volumes().empty();
    world.each<const RigidBody, const AirDrag>([&](Entity e, const RigidBody& rb, const AirDrag& d) {
        if (!is_active_dynamic(physics, rb)) return;
        const Vec3 p = physics.position(rb.handle);
        float in_air = 1.0f;
        if (has_water) {
            if (const Buoyant* b = world.get<const Buoyant>(e)) {
                const float height = std::max(b->height, 0.01f);
                const WaterSample water = fields.water_at({p.x, p.y - 0.5f * height, p.z}, now);
                if (water.inside) in_air = 1.0f - std::clamp(water.depth / height, 0.0f, 1.0f);
            }
        }
        const Vec3 relative = fields.wind_at(p, now) - physics.linear_velocity(rb.handle);
        physics.add_force(rb.handle, relative * (0.5f * air_density * d.drag * d.area * length(relative) * in_air));
    });
}

void apply_airfoil_lift(World& world, PhysicsWorld& physics, const FieldWorld& fields, double now) {
    world.each<const RigidBody, const Airfoil>([&](const RigidBody& rb, const Airfoil& wing) {
        if (!is_active_dynamic(physics, rb)) return;
        const Vec3 p = physics.position(rb.handle);
        const Vec3 flow = fields.wind_at(p, now) - physics.linear_velocity(rb.handle);
        const float speed = length(flow);
        if (speed < 0.1f) return;

        const Quat q = physics.rotation(rb.handle);
        const Vec3 forward = q * normalize(wing.forward);
        const Vec3 up = q * normalize(wing.up);
        const float alpha = std::atan2(dot(flow, up), -dot(flow, forward));
        const float stall = std::max(wing.stall_angle, 0.01f);
        float coefficient = wing.lift_slope * std::clamp(alpha, -stall, stall);
        if (std::abs(alpha) > stall) {
            coefficient *= std::clamp(1.0f - (std::abs(alpha) - stall) / stall, 0.0f, 1.0f);
        }
        const Vec3 direction = flow / speed;
        const Vec3 lift_axis = normalize(up - direction * dot(up, direction));
        physics.add_force(rb.handle, lift_axis * (0.5f * air_density * speed * speed * wing.area * coefficient));
    });
}

} // namespace

void rebuild_field_world(World& world, FieldWorld& fields) {
    fields.clear_volumes();
    fields.set_global_wind({});
    world.each<const WindZone>([&](Entity e, const WindZone& z) {
        const WindDesc wind{.velocity = z.velocity,
                            .gust_strength = z.gust_strength,
                            .gust_frequency = z.gust_frequency,
                            .turbulence = z.turbulence,
                            .turbulence_scale = z.turbulence_scale,
                            .seed = z.seed};
        if (z.global) {
            fields.set_global_wind(wind);
        } else {
            fields.add_wind_volume({.center = entity_centre(world, e),
                                    .half_extents = z.half_extents,
                                    .wind = wind,
                                    .falloff = z.falloff,
                                    .replace = z.replace});
        }
    });
    world.each<const WaterBody>([&](Entity e, const WaterBody& w) {
        fields.add_water_volume({.center = entity_centre(world, e),
                                 .half_extents = w.half_extents,
                                 .density = w.density,
                                 .flow = w.flow,
                                 .wave_height = w.wave_height,
                                 .wave_length = w.wave_length,
                                 .wave_speed = w.wave_speed});
    });
}

void apply_physics_field_forces(World& world, PhysicsWorld& physics, const FieldWorld& fields, double now, float dt) {
    apply_buoyancy(world, physics, fields, now, dt);
    apply_air_drag(world, physics, fields, now);
    apply_airfoil_lift(world, physics, fields, now);
}

} // namespace astral
