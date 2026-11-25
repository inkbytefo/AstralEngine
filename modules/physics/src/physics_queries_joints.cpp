#include "physics_internal.hpp"

#include <astral/core/jobs.hpp>

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <optional>

namespace astral {

namespace {

constexpr std::size_t kBatchParallelThreshold = 32;

JPH::Ref<JPH::Shape> resolve_query_shape(const PhysicsWorld::Impl& m, ShapeHandle cooked, const ShapeDesc& fallback) {
    if (cooked.valid()) {
        const auto* rec = m.shapes.get(cooked);
        return (rec && !rec->static_only) ? rec->shape : nullptr;
    }
    return shape_needs_static(fallback) ? nullptr : make_jolt_shape(fallback);
}

RayBodyFilter make_body_filter(const PhysicsWorld::Impl& m, const QueryFilter& filter) {
    std::vector<JPH::BodyID> ids;
    ids.reserve(1 + filter.ignore_list.size());
    if (const auto* r = m.record(filter.ignore)) ids.push_back(r->id);
    for (BodyHandle h : filter.ignore_list) {
        if (const auto* r = m.record(h)) ids.push_back(r->id);
    }
    return RayBodyFilter(std::move(ids), filter.include_sensors);
}

std::optional<ShapeHit> cast_jolt_shape(const PhysicsWorld::Impl& m, const JPH::Shape* shape, Vec3 position, Quat rotation,
                                        Vec3 displacement, const QueryFilter& filter) {
    if (!shape || length(displacement) <= 0.0f) return std::nullopt;
    const JPH::RMat44 start = JPH::RMat44::sRotationTranslation(to_jph(rotation), JPH::RVec3(to_jph(position)));
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(shape, JPH::Vec3::sReplicate(1.0f), start, to_jph(displacement));
    JPH::ShapeCastSettings settings;
    settings.mReturnDeepestPoint = false;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const RayBodyFilter body_filter = make_body_filter(m, filter);
    const LayerMask layer_filter(filter.layers);
    m.system->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return std::nullopt;
    const JPH::ShapeCastResult& r = collector.mHit;
    ShapeHit hit;
    hit.body = m.handle_of(r.mBodyID2);
    hit.distance = r.mFraction * length(displacement);
    hit.position = from_jph(JPH::Vec3(r.mContactPointOn2));
    hit.surface = m.surface_of(hit.body);
    JPH::BodyLockRead lock(m.system->GetBodyLockInterface(), r.mBodyID2);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        hit.normal = from_jph(body.GetWorldSpaceSurfaceNormal(r.mSubShapeID2, r.mContactPointOn2));
        hit.user_data = body.GetUserData();
    }
    return hit;
}

std::vector<OverlapHit> overlap_jolt_shape(const PhysicsWorld::Impl& m, const JPH::Shape* shape, Vec3 position, Quat rotation,
                                           const QueryFilter& filter) {
    std::vector<OverlapHit> out;
    if (!shape) return out;
    const JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(to_jph(rotation), JPH::RVec3(to_jph(position)));
    JPH::CollideShapeSettings settings;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    const RayBodyFilter body_filter = make_body_filter(m, filter);
    const LayerMask layer_filter(filter.layers);
    m.system->GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(),
                                                 collector, {}, layer_filter, body_filter);
    std::vector<JPH::BodyID> seen;
    for (const JPH::CollideShapeResult& r : collector.mHits) {
        if (std::find(seen.begin(), seen.end(), r.mBodyID2) != seen.end()) continue;
        seen.push_back(r.mBodyID2);
        // Characters' inner bodies have no BodyHandle: report them by user data (body stays invalid).
        const BodyHandle h = m.handle_of(r.mBodyID2);
        const auto* info = m.info_of(r.mBodyID2);
        if (!h.valid() && !info) continue;
        out.push_back({h, h.valid() ? m.user_data_of(h) : info->user_data});
    }
    return out;
}

} // namespace

// --- Queries ---------------------------------------------------------------------------------

std::optional<RayHit> PhysicsWorld::raycast(Vec3 origin, Vec3 direction, float max_distance, const QueryFilter& filter) const {
    const auto& m = *impl_;
    const Vec3 dir = normalize(direction);
    if (max_distance <= 0.0f || length(dir) == 0.0f) return std::nullopt;
    const JPH::RRayCast ray{JPH::RVec3(to_jph(origin)), to_jph(dir * max_distance)};
    JPH::RayCastResult result;
    const RayBodyFilter body_filter = make_body_filter(m, filter);
    const LayerMask layer_filter(filter.layers);
    if (!m.system->GetNarrowPhaseQuery().CastRay(ray, result, {}, layer_filter, body_filter)) return std::nullopt;

    RayHit hit;
    hit.body = m.handle_of(result.mBodyID);
    hit.distance = result.mFraction * max_distance;
    hit.position = origin + dir * hit.distance;
    hit.surface = m.surface_of(hit.body);
    JPH::BodyLockRead lock(m.system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        hit.normal = from_jph(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, JPH::RVec3(to_jph(hit.position))));
        hit.user_data = body.GetUserData();
    }
    return hit;
}

std::optional<ShapeHit> PhysicsWorld::shape_cast(const ShapeDesc& shape_desc, Vec3 position, Quat rotation, Vec3 displacement,
                                                 const QueryFilter& filter) const {
    if (shape_needs_static(shape_desc)) return std::nullopt;
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(shape_desc);
    return cast_jolt_shape(*impl_, shape.GetPtr(), position, rotation, displacement, filter);
}

std::vector<OverlapHit> PhysicsWorld::overlap(const ShapeDesc& shape_desc, Vec3 position, Quat rotation, const QueryFilter& filter) const {
    if (shape_needs_static(shape_desc)) return {};
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(shape_desc);
    return overlap_jolt_shape(*impl_, shape.GetPtr(), position, rotation, filter);
}

void PhysicsWorld::raycast_batch(std::span<const RayQuery> queries, std::span<std::optional<RayHit>> out, JobSystem* jobs) const {
    const std::size_t n = std::min(queries.size(), out.size());
    auto run_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const RayQuery& q = queries[i];
            out[i] = raycast(q.origin, q.direction, q.max_distance, q.filter);
        }
    };
    if (jobs && n >= kBatchParallelThreshold) {
        JobCounter counter;
        jobs->parallel_for(counter, n, 16, run_range);
        jobs->wait(counter);
    } else {
        run_range(0, n);
    }
}

void PhysicsWorld::shape_cast_batch(std::span<const ShapeCastQuery> queries, std::span<std::optional<ShapeHit>> out,
                                    JobSystem* jobs) const {
    const std::size_t n = std::min(queries.size(), out.size());
    auto run_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const ShapeCastQuery& q = queries[i];
            const JPH::Ref<JPH::Shape> shape = resolve_query_shape(*impl_, q.cooked_shape, q.shape);
            out[i] = cast_jolt_shape(*impl_, shape.GetPtr(), q.position, q.rotation, q.displacement, q.filter);
        }
    };
    if (jobs && n >= kBatchParallelThreshold) {
        JobCounter counter;
        jobs->parallel_for(counter, n, 8, run_range);
        jobs->wait(counter);
    } else {
        run_range(0, n);
    }
}

void PhysicsWorld::overlap_batch(std::span<const OverlapQuery> queries, std::span<std::vector<OverlapHit>> out,
                                 JobSystem* jobs) const {
    const std::size_t n = std::min(queries.size(), out.size());
    auto run_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const OverlapQuery& q = queries[i];
            const JPH::Ref<JPH::Shape> shape = resolve_query_shape(*impl_, q.cooked_shape, q.shape);
            out[i] = overlap_jolt_shape(*impl_, shape.GetPtr(), q.position, q.rotation, q.filter);
        }
    };
    if (jobs && n >= kBatchParallelThreshold) {
        JobCounter counter;
        jobs->parallel_for(counter, n, 8, run_range);
        jobs->wait(counter);
    } else {
        run_range(0, n);
    }
}

// --- Characters ------------------------------------------------------------------------------

CharacterHandle PhysicsWorld::create_character(const CharacterDesc& desc) {
    auto& m = *impl_;
    if (!positive_scalar(desc.radius) || !positive_scalar(desc.height)) {
        m.error("create_character: radius and height must be positive");
        return {};
    }
    // The capsule's total height is the contract: it must fit two hemispheres plus Jolt's minimum
    // cylinder, otherwise the collider would silently be taller than requested.
    constexpr float min_half_cylinder = 0.01f;
    if (desc.height < 2.0f * (desc.radius + min_half_cylinder)) {
        m.error("create_character: height must be at least 2 * radius + 0.02 (it is the capsule's total height)");
        return {};
    }
    const float half_cylinder = desc.height * 0.5f - desc.radius;
    auto capsule_shape = JPH::CapsuleShapeSettings(half_cylinder, desc.radius).Create();
    if (!capsule_shape.IsValid()) return {};
    JPH::RotatedTranslatedShapeSettings capsule(JPH::Vec3(0, half_cylinder + desc.radius, 0), JPH::Quat::sIdentity(),
                                                capsule_shape.Get());
    auto shape = capsule.Create();
    if (!shape.IsValid()) return {};
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = shape.Get();
    settings->mMaxSlopeAngle = desc.max_slope;
    settings->mMass = desc.mass;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.radius);
    settings->mInnerBodyShape = shape.Get();
    settings->mInnerBodyLayer = physics_layers::moving;
    JPH::Ref<JPH::CharacterVirtual> character =
        new JPH::CharacterVirtual(settings, JPH::RVec3(to_jph(desc.position)), JPH::Quat::sIdentity(), desc.user_data, m.system.get());
    character->SetCharacterVsCharacterCollision(&m.character_collision);
    m.character_collision.Add(character);
    if (!character->GetInnerBodyID().IsInvalid()) {
        m.info_by_id[character->GetInnerBodyID().GetIndexAndSequenceNumber()] = {desc.user_data, 0, false};
    }
    return m.characters.create({character});
}

void PhysicsWorld::destroy(CharacterHandle character) {
    auto& m = *impl_;
    if (auto* r = m.characters.get(character)) {
        if (!r->character->GetInnerBodyID().IsInvalid()) {
            m.retired_info.push_back(r->character->GetInnerBodyID().GetIndexAndSequenceNumber());
        }
        m.character_collision.Remove(r->character);
        m.characters.destroy(character);
    }
}

bool PhysicsWorld::valid(CharacterHandle character) const noexcept { return impl_->characters.get(character) != nullptr; }

std::uint64_t PhysicsWorld::user_data(CharacterHandle character) const {
    const auto* r = impl_->characters.get(character);
    return r ? r->character->GetUserData() : 0;
}

void PhysicsWorld::update_character(CharacterHandle handle, Vec3 velocity, bool jump, float dt, float jump_speed) {
    auto& m = *impl_;
    auto* r = m.characters.get(handle);
    if (!r || dt <= 0.0f) return;
    JPH::CharacterVirtual& c = *r->character;
    const JPH::Vec3 gravity = m.system->GetGravity();

    c.UpdateGroundVelocity();
    const JPH::Vec3 current = c.GetLinearVelocity();
    const bool on_ground = c.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
    JPH::Vec3 next;
    if (on_ground && current.GetY() - c.GetGroundVelocity().GetY() < 0.1f) {
        next = c.GetGroundVelocity();
        if (jump) next += JPH::Vec3(0, jump_speed, 0);
    } else {
        next = JPH::Vec3(0, current.GetY(), 0);
    }
    next += gravity * dt;
    next += JPH::Vec3(velocity.x, 0, velocity.z);
    c.SetLinearVelocity(next);

    JPH::CharacterVirtual::ExtendedUpdateSettings update;
    c.ExtendedUpdate(dt, gravity, update, m.system->GetDefaultBroadPhaseLayerFilter(physics_layers::moving),
                     m.system->GetDefaultLayerFilter(physics_layers::moving), {}, {}, *m.temp);
}

CharacterState PhysicsWorld::character_state(CharacterHandle handle) const {
    const auto* r = impl_->characters.get(handle);
    if (!r) return {};
    const JPH::CharacterVirtual& c = *r->character;
    return {from_jph(JPH::Vec3(c.GetPosition())), from_jph(c.GetLinearVelocity()),
            c.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround};
}

void PhysicsWorld::set_character_position(CharacterHandle handle, Vec3 position) {
    auto* r = impl_->characters.get(handle);
    if (r) r->character->SetPosition(JPH::RVec3(to_jph(position)));
}

} // namespace astral
