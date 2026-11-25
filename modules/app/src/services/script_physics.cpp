#include "physics_bridge.hpp"

#include <astral/script/script.hpp>
#include <astral/world/world.hpp>

#include <optional>
#include <vector>

namespace astral {

namespace {

JointMotorMode parse_motor_mode(std::string_view mode) noexcept {
    if (mode == "Velocity") return JointMotorMode::Velocity;
    if (mode == "Position") return JointMotorMode::Position;
    return JointMotorMode::Off;
}

/// What scripts may do with physics: routes queries and queued commands through PhysicsBridge.
class AppScriptPhysics final : public ScriptPhysics {
public:
    AppScriptPhysics(World& world, PhysicsBridge& bridge, const FieldWorld& fields, const Time& time)
        : world_(world), bridge_(bridge), fields_(fields), time_(time) {}

    std::optional<ScriptHit> raycast(Vec3 origin, Vec3 direction, float max_distance, std::uint32_t layers,
                                     Entity ignore) const override {
        const auto hit =
            bridge_.raycast(origin, direction, max_distance, {.layers = layers, .ignore = bridge_.resolve_body(ignore)});
        return hit ? std::optional<ScriptHit>(
                         to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }

    std::optional<ScriptHit> raycast_filtered(Vec3 origin, Vec3 direction, float max_distance,
                                              const ScriptQueryFilter& filter) const override {
        std::vector<BodyHandle> ignored;
        const QueryFilter native = to_native(filter, ignored);
        const auto hit = bridge_.raycast(origin, direction, max_distance, native);
        return hit ? std::optional<ScriptHit>(
                         to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }

    std::optional<ScriptHit> spherecast_filtered(Vec3 origin, float radius, Vec3 displacement,
                                                 const ScriptQueryFilter& filter) const override {
        std::vector<BodyHandle> ignored;
        const QueryFilter native = to_native(filter, ignored);
        const auto hit = bridge_.spherecast(origin, radius, displacement, native);
        return hit ? std::optional<ScriptHit>(
                         to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }

    std::vector<std::optional<ScriptHit>> raycast_batch(std::span<const ScriptRayQuery> queries) const override {
        std::vector<RayQuery> native;
        native.reserve(queries.size());
        for (const ScriptRayQuery& q : queries) {
            native.push_back(
                {q.origin, q.direction, q.max_distance, {.layers = q.layers, .ignore = bridge_.resolve_body(q.ignore)}});
        }
        std::vector<std::optional<RayHit>> hits(native.size());
        bridge_.raycast_batch(native, hits);
        std::vector<std::optional<ScriptHit>> out(hits.size());
        for (std::size_t i = 0; i < hits.size(); ++i) {
            if (hits[i]) {
                out[i] =
                    to_script(hits[i]->user_data, hits[i]->position, hits[i]->normal, hits[i]->distance, hits[i]->surface);
            }
        }
        return out;
    }

    std::optional<ScriptHit> spherecast(Vec3 origin, float radius, Vec3 displacement, std::uint32_t layers,
                                        Entity ignore) const override {
        const auto hit = bridge_.spherecast(origin, radius, displacement,
                                            {.layers = layers, .ignore = bridge_.resolve_body(ignore)});
        return hit ? std::optional<ScriptHit>(
                         to_script(hit->user_data, hit->position, hit->normal, hit->distance, hit->surface))
                   : std::nullopt;
    }

    std::vector<Entity> overlap_sphere(Vec3 center, float radius, std::uint32_t layers) const override {
        return entities(bridge_.overlap_sphere(center, radius, {.layers = layers}));
    }

    std::vector<Entity> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation, std::uint32_t layers) const override {
        return entities(bridge_.overlap_box(center, half_extents, rotation, {.layers = layers}));
    }

    void impulse(Entity body, Vec3 v) override { bridge_.add_impulse(body, v); }
    void force_at(Entity body, Vec3 force, Vec3 point) override { bridge_.add_force_at(body, force, point); }
    void torque(Entity body, Vec3 v) override { bridge_.add_torque(body, v); }
    void angular_impulse(Entity body, Vec3 v) override { bridge_.add_angular_impulse(body, v); }
    void set_angular_velocity(Entity body, Vec3 v) override { bridge_.set_angular_velocity(body, v); }
    Vec3 angular_velocity(Entity body) const override { return bridge_.angular_velocity(body); }
    void force(Entity body, Vec3 v) override { bridge_.add_force(body, v); }
    void set_velocity(Entity body, Vec3 v) override { bridge_.set_linear_velocity(body, v); }
    Vec3 velocity(Entity body) const override { return bridge_.linear_velocity(body); }

    bool move_character(Entity entity, Vec3 velocity, bool jump) override {
        if (auto* controller = world_.get<CharacterController>(entity)) {
            controller->move = {velocity.x, 0, velocity.z};
            controller->jump |= jump;
            return true;
        }
        return false;
    }

    std::optional<ScriptCharacterState> character_state(Entity entity) const override {
        if (const auto* controller = world_.get<const CharacterController>(entity);
            controller && controller->handle.valid()) {
            return ScriptCharacterState{controller->velocity, controller->on_ground};
        }
        return std::nullopt;
    }

    void teleport(Entity body, Vec3 position, Quat rotation) override { bridge_.set_body_transform(body, position, rotation); }
    void wake(Entity body) override { bridge_.wake_body(body); }
    void sleep(Entity body) override { bridge_.sleep_body(body); }
    void set_joint_motor_mode(Entity joint, std::string_view mode) override {
        bridge_.set_joint_motor_mode(joint, parse_motor_mode(mode));
    }
    void set_joint_motor_velocity(Entity joint, float v) override { bridge_.set_joint_motor_velocity(joint, v); }
    void set_joint_motor_position(Entity joint, float p) override { bridge_.set_joint_motor_position(joint, p); }
    void break_joint(Entity joint) override { bridge_.break_joint(joint); }

    Vec3 wind(Vec3 position) const override { return fields_.wind_at(position, time_.elapsed); }
    ScriptWater water(Vec3 position) const override {
        const WaterSample s = fields_.water_at(position, time_.elapsed);
        return {s.inside, s.surface_height, s.depth, s.flow, s.density};
    }

private:
    /// `storage` keeps the resolved bodies alive for the query that uses the returned filter.
    QueryFilter to_native(const ScriptQueryFilter& filter, std::vector<BodyHandle>& storage) const {
        storage.clear();
        for (Entity e : filter.ignore) {
            if (const BodyHandle h = bridge_.resolve_body(e); h.valid()) storage.push_back(h);
        }
        return {.layers = filter.layers, .ignore_list = storage, .include_sensors = filter.sensors};
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
    PhysicsBridge& bridge_;
    const FieldWorld& fields_;
    const Time& time_;
};

} // namespace

std::unique_ptr<ScriptPhysics> make_app_script_physics(World& world, PhysicsBridge& bridge,
                                                       const FieldWorld& fields, const Time& time) {
    return std::make_unique<AppScriptPhysics>(world, bridge, fields, time);
}

} // namespace astral
