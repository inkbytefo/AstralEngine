#pragma once
// Private Jolt Physics backend state shared by the physics module's translation units.

#include <astral/core/log.hpp>
#include <astral/physics/physics.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace astral {

namespace physics_layers {
inline constexpr JPH::ObjectLayer non_moving = 0; // layer 0, static
inline constexpr JPH::ObjectLayer moving = 1;     // layer 0, moving
inline constexpr JPH::BroadPhaseLayer bp_non_moving(0);
inline constexpr JPH::BroadPhaseLayer bp_moving(1);
/// Jolt object layer = astral layer * 2 + (moving ? 1 : 0).
inline constexpr JPH::ObjectLayer object_layer(unsigned layer, bool is_moving) {
    return static_cast<JPH::ObjectLayer>(layer * 2 + (is_moving ? 1 : 0));
}
inline constexpr unsigned astral_layer(JPH::ObjectLayer l) { return l >> 1; }
inline constexpr bool is_moving(JPH::ObjectLayer l) { return (l & 1) != 0; }
} // namespace physics_layers

/// collide[a] bit b: layers a and b collide.
using CollisionMatrix = std::array<std::uint32_t, max_physics_layers>;

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return physics_layers::is_moving(layer) ? physics_layers::bp_moving : physics_layers::bp_non_moving;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == physics_layers::bp_non_moving ? "non_moving" : "moving";
    }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        return physics_layers::is_moving(layer) || bp == physics_layers::bp_moving;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    explicit ObjectPairs(const CollisionMatrix& matrix) : matrix_(matrix) {}
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (!physics_layers::is_moving(a) && !physics_layers::is_moving(b)) return false;
        return ((matrix_[physics_layers::astral_layer(a)] >> physics_layers::astral_layer(b)) & 1u) != 0;
    }

private:
    const CollisionMatrix& matrix_;
};

/// A query sees these layers only.
class LayerMask final : public JPH::ObjectLayerFilter {
public:
    explicit LayerMask(std::uint32_t mask) : mask_(mask) {}
    bool ShouldCollide(JPH::ObjectLayer layer) const override {
        return ((mask_ >> physics_layers::astral_layer(layer)) & 1u) != 0;
    }

private:
    std::uint32_t mask_;
};

/// Query filter: skips one optional body, and sensors unless asked for.
/// Query body filter: skips the ignored bodies (QueryFilter::ignore + ignore_list) and, unless
/// asked for, sensors.
class RayBodyFilter final : public JPH::BodyFilter {
public:
    RayBodyFilter(std::vector<JPH::BodyID> ignore, bool sensors) : ignore_(std::move(ignore)), sensors_(sensors) {}
    bool ShouldCollide(const JPH::BodyID& id) const override {
        return std::find(ignore_.begin(), ignore_.end(), id) == ignore_.end();
    }
    bool ShouldCollideLocked(const JPH::Body& body) const override { return sensors_ || !body.IsSensor(); }

private:
    std::vector<JPH::BodyID> ignore_;
    bool sensors_;
};

inline JPH::Vec3 to_jph(Vec3 v) { return {v.x, v.y, v.z}; }
inline JPH::Quat to_jph(Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
inline Vec3 from_jph(JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline Quat from_jph(JPH::Quat q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }
inline bool positive_scalar(float v) { return std::isfinite(v) && v > 0.0f; }
inline bool finite_vec(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

inline float combine_material_values(float a, float b, PhysicsCombineMode mode) noexcept {
    switch (mode) {
    case PhysicsCombineMode::Average: return 0.5f * (a + b);
    case PhysicsCombineMode::Min: return std::min(a, b);
    case PhysicsCombineMode::Multiply: return a * b;
    case PhysicsCombineMode::Max: return std::max(a, b);
    }
    return 0.5f * (a + b);
}

void acquire_jolt_registry();
void release_jolt_registry();
std::uint32_t jolt_backend_abi_hash() noexcept;

bool shape_needs_static(const ShapeDesc& d);
JPH::Ref<JPH::Shape> make_jolt_shape(const ShapeDesc& d);
void check_breakable_joints(PhysicsWorld::Impl& m, float dt);
void shift_world_anchored_joints(PhysicsWorld::Impl& m, Vec3 offset);

struct PhysicsWorld::Impl {
    struct BodyRecord {
        JPH::BodyID id;
        std::uint64_t user_data = 0;
        std::uint16_t surface = 0;
        bool sensor = false;
    };
    /// Identity of every Jolt body we created (rigid bodies and character inner bodies), keyed by
    /// the full BodyID (index + sequence). Kept for one step after the body is destroyed so the
    /// contact-removed callbacks of that step still know whose contact ended.
    struct BodyInfo {
        std::uint64_t user_data = 0;
        std::uint16_t surface = 0;
        bool sensor = false;
    };
    struct ShapeRecord {
        JPH::Ref<JPH::Shape> shape;
        bool static_only = false;
    };
    struct CharacterRecord {
        JPH::Ref<JPH::CharacterVirtual> character;
    };
    struct JointRecord {
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        BodyHandle a, b;
        std::uint64_t pair_key = 0; ///< Non-zero: this joint disables collisions between its bodies.
        JointDesc desc{};
        bool broken = false;
        JointReaction last_reaction{};
    };
    /// Which pairs of bodies must not collide (the bodies of a joint). Read by Jolt's workers during a step,
    /// changed only between steps.
    class PairFilter final : public JPH::GroupFilter {
    public:
        bool CanCollide(const JPH::CollisionGroup& a, const JPH::CollisionGroup& b) const override {
            if (disabled.empty()) return true;
            const auto lo = std::min(a.GetSubGroupID(), b.GetSubGroupID()), hi = std::max(a.GetSubGroupID(), b.GetSubGroupID());
            return !disabled.contains((std::uint64_t{lo} << 32) | hi);
        }
        std::unordered_map<std::uint64_t, int> disabled;
    };
    static std::uint64_t pair_key(JPH::BodyID a, JPH::BodyID b) {
        const auto lo = std::min(a.GetIndex(), b.GetIndex()), hi = std::max(a.GetIndex(), b.GetIndex());
        return (std::uint64_t{lo} << 32) | hi;
    }
    JPH::Ref<PairFilter> pair_filter; // created after Jolt's allocator is registered
    /// Jolt body index -> our handle plus the full Jolt id (index + sequence), so a recycled
    /// index never resolves to a newer body.
    struct IndexEntry {
        BodyHandle handle;
        JPH::BodyID id;
    };

    /// Collects contacts and applies registered PhysicsMaterial combine rules from Jolt's workers.
    class Contacts final : public JPH::ContactListener {
    public:
        explicit Contacts(Impl& owner) : owner_(owner) {}
        void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override {
            const BodyHandle ha = owner_.handle_of(a.GetID());
            const BodyHandle hb = owner_.handle_of(b.GetID());
            const std::uint16_t sa = owner_.surface_of(ha);
            const std::uint16_t sb = owner_.surface_of(hb);
            apply_materials(a, b, sa, sb, settings);
            ContactEvent e;
            e.type = ContactEvent::Type::Begin;
            e.a = ha;
            e.b = hb;
            e.point = from_jph(JPH::Vec3(manifold.GetWorldSpaceContactPointOn1(0)));
            e.normal = from_jph(manifold.mWorldSpaceNormal);
            e.user_data_a = a.GetUserData();
            e.user_data_b = b.GetUserData();
            e.surface_a = sa;
            e.surface_b = sb;
            e.sensor = a.IsSensor() || b.IsSensor();
            std::scoped_lock lock(mutex_);
            owner_.contacts.push_back(e);
        }
        void OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                                JPH::ContactSettings& settings) override {
            if (owner_.materials.empty()) return;
            const std::uint16_t sa = owner_.surface_of(owner_.handle_of(a.GetID()));
            const std::uint16_t sb = owner_.surface_of(owner_.handle_of(b.GetID()));
            apply_materials(a, b, sa, sb, settings);
        }
        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
            ContactEvent e;
            e.type = ContactEvent::Type::End;
            e.a = owner_.handle_of(pair.GetBody1ID());
            e.b = owner_.handle_of(pair.GetBody2ID());
            // By BodyID, not handle: characters have no BodyHandle, and a body destroyed since the
            // last step has none anymore, but both still have their identity here.
            const BodyInfo* ia = owner_.info_of(pair.GetBody1ID());
            const BodyInfo* ib = owner_.info_of(pair.GetBody2ID());
            e.user_data_a = ia ? ia->user_data : 0;
            e.user_data_b = ib ? ib->user_data : 0;
            e.surface_a = ia ? ia->surface : 0;
            e.surface_b = ib ? ib->surface : 0;
            e.sensor = (ia && ia->sensor) || (ib && ib->sensor);
            std::scoped_lock lock(mutex_);
            owner_.contacts.push_back(e);
        }

    private:
        void apply_materials(const JPH::Body& a, const JPH::Body& b, std::uint16_t sa, std::uint16_t sb,
                             JPH::ContactSettings& settings) const {
            if (owner_.materials.empty()) return;
            const PhysicsMaterialDesc* ma = owner_.material_of(sa);
            const PhysicsMaterialDesc* mb = owner_.material_of(sb);
            if (!ma && !mb) return;
            const float fa = ma ? ma->friction : a.GetFriction();
            const float fb = mb ? mb->friction : b.GetFriction();
            const float ra = ma ? ma->restitution : a.GetRestitution();
            const float rb = mb ? mb->restitution : b.GetRestitution();
            const PhysicsCombineMode fmode = (ma && mb) ? std::max(ma->friction_combine, mb->friction_combine)
                                                        : (ma ? ma->friction_combine : mb->friction_combine);
            const PhysicsCombineMode rmode = (ma && mb) ? std::max(ma->restitution_combine, mb->restitution_combine)
                                                        : (ma ? ma->restitution_combine : mb->restitution_combine);
            settings.mCombinedFriction = combine_material_values(fa, fb, fmode);
            settings.mCombinedRestitution = combine_material_values(ra, rb, rmode);
        }

        Impl& owner_;
        std::mutex mutex_;
    };

    CollisionMatrix collide;
    BroadPhaseLayers broad_phase_layers;
    ObjectVsBroadPhase object_vs_broad_phase;
    ObjectPairs object_pairs{collide};
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystemThreadPool> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    std::unique_ptr<Contacts> listener;

    HandlePool<BodyRecord, BodyTag> bodies;
    HandlePool<ShapeRecord, ShapeTag> shapes;
    std::vector<IndexEntry> handle_by_index; // read during steps, written only between them
    std::unordered_map<std::uint32_t, BodyInfo> info_by_id; // read during steps, written only between them
    std::vector<std::uint32_t> retired_info;                // destroyed since the last step
    HandlePool<CharacterRecord, CharacterTag> characters;
    HandlePool<JointRecord, JointTag> joints;
    JPH::CharacterVsCharacterCollisionSimple character_collision;
    std::unordered_map<std::uint16_t, PhysicsMaterialDesc> materials;
    std::vector<ContactEvent> contacts;
    std::vector<JointBrokenEvent> broken_joints;
    Logger* logger = nullptr;

    BodyHandle handle_of(JPH::BodyID id) const {
        const JPH::uint32 index = id.GetIndex();
        if (index >= handle_by_index.size() || handle_by_index[index].id != id) return {};
        return handle_by_index[index].handle;
    }
    void error(std::string_view message) const {
        if (logger) logger->log(LogLevel::Error, message);
        else std::fprintf(stderr, "[astral::physics] %.*s\n", static_cast<int>(message.size()), message.data());
    }
    const BodyInfo* info_of(JPH::BodyID id) const {
        const auto it = info_by_id.find(id.GetIndexAndSequenceNumber());
        return it != info_by_id.end() ? &it->second : nullptr;
    }
    std::uint64_t user_data_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r ? r->user_data : 0;
    }
    std::uint16_t surface_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r ? r->surface : 0;
    }
    bool sensor_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r && r->sensor;
    }
    const PhysicsMaterialDesc* material_of(std::uint16_t surface) const {
        const auto it = materials.find(surface);
        return it != materials.end() ? &it->second : nullptr;
    }
    JPH::BodyInterface& bi() { return system->GetBodyInterface(); }
    const JPH::BodyInterface& bi() const { return system->GetBodyInterface(); }
    const BodyRecord* record(BodyHandle h) const { return bodies.get(h); }
};

} // namespace astral
