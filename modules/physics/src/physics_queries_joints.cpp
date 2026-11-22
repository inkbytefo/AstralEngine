#include "physics_internal.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>

#include <optional>

namespace astral {

namespace {

JPH::Ref<JPH::TwoBodyConstraint> build_jolt_constraint(const JointDesc& d, JPH::Body& body_a, JPH::Body& body_b) {
    const JPH::RVec3 anchor(to_jph(d.anchor));
    const JPH::Vec3 axis = to_jph(normalize(d.axis));
    const JPH::Vec3 normal = axis.GetNormalizedPerpendicular();
    const JPH::SpringSettings spring(JPH::ESpringMode::FrequencyAndDamping, d.spring_frequency, d.spring_damping);
    switch (d.type) {
    case JointType::Fixed: {
        JPH::FixedConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mAutoDetectPoint = true;
        return s.Create(body_a, body_b);
    }
    case JointType::Point: {
        JPH::PointConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = s.mPoint2 = anchor;
        return s.Create(body_a, body_b);
    }
    case JointType::Hinge: {
        JPH::HingeConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = s.mPoint2 = anchor;
        s.mHingeAxis1 = s.mHingeAxis2 = axis;
        s.mNormalAxis1 = s.mNormalAxis2 = normal;
        s.mMaxFrictionTorque = d.max_friction;
        if (d.limited) {
            s.mLimitsMin = d.min_limit;
            s.mLimitsMax = d.max_limit;
            s.mLimitsSpringSettings = spring;
        }
        return s.Create(body_a, body_b);
    }
    case JointType::Slider: {
        JPH::SliderConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mAutoDetectPoint = false;
        s.mPoint1 = s.mPoint2 = anchor;
        s.mSliderAxis1 = s.mSliderAxis2 = axis;
        s.mNormalAxis1 = s.mNormalAxis2 = normal;
        s.mMaxFrictionForce = d.max_friction;
        if (d.limited) {
            s.mLimitsMin = d.min_limit;
            s.mLimitsMax = d.max_limit;
            s.mLimitsSpringSettings = spring;
        }
        return s.Create(body_a, body_b);
    }
    case JointType::Distance: {
        JPH::DistanceConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = anchor;
        s.mPoint2 = JPH::RVec3(to_jph(d.anchor_b));
        if (d.limited) {
            s.mMinDistance = d.min_limit;
            s.mMaxDistance = d.max_limit;
        } else {
            s.mMinDistance = s.mMaxDistance = -1.0f; // the distance they have now
        }
        s.mLimitsSpringSettings = spring;
        return s.Create(body_a, body_b);
    }
    }
    return nullptr;
}

} // namespace

// --- Queries ---------------------------------------------------------------------------------

std::optional<RayHit> PhysicsWorld::raycast(Vec3 origin, Vec3 direction, float max_distance, const QueryFilter& filter) const {
    const auto& m = *impl_;
    const Vec3 dir = normalize(direction);
    if (max_distance <= 0.0f || length(dir) == 0.0f) return std::nullopt;
    const JPH::RRayCast ray{JPH::RVec3(to_jph(origin)), to_jph(dir * max_distance)};
    JPH::RayCastResult result;
    const auto* ignored = m.record(filter.ignore);
    const RayBodyFilter body_filter(ignored ? ignored->id : JPH::BodyID(), filter.include_sensors);
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
    const auto& m = *impl_;
    if (shape_needs_static(shape_desc)) return std::nullopt; // static geometry cannot be swept
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(shape_desc);
    if (!shape || length(displacement) <= 0.0f) return std::nullopt;
    const JPH::RMat44 start = JPH::RMat44::sRotationTranslation(to_jph(rotation), JPH::RVec3(to_jph(position)));
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(shape, JPH::Vec3::sReplicate(1.0f), start, to_jph(displacement));
    JPH::ShapeCastSettings settings;
    settings.mReturnDeepestPoint = false;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const auto* ignored = m.record(filter.ignore);
    const RayBodyFilter body_filter(ignored ? ignored->id : JPH::BodyID(), filter.include_sensors);
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

std::vector<OverlapHit> PhysicsWorld::overlap(const ShapeDesc& shape_desc, Vec3 position, Quat rotation, const QueryFilter& filter) const {
    const auto& m = *impl_;
    std::vector<OverlapHit> out;
    if (shape_needs_static(shape_desc)) return out;
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(shape_desc);
    if (!shape) return out;
    const JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(to_jph(rotation), JPH::RVec3(to_jph(position)));
    JPH::CollideShapeSettings settings;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    const auto* ignored = m.record(filter.ignore);
    const RayBodyFilter body_filter(ignored ? ignored->id : JPH::BodyID(), filter.include_sensors);
    const LayerMask layer_filter(filter.layers);
    m.system->GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(), collector, {}, layer_filter, body_filter);
    for (const JPH::CollideShapeResult& r : collector.mHits) {
        const BodyHandle h = m.handle_of(r.mBodyID2);
        if (!h.valid()) continue;
        if (std::any_of(out.begin(), out.end(), [&](const OverlapHit& o) { return o.body == h; })) continue; // one entry per body
        out.push_back({h, m.user_data_of(h)});
    }
    return out;
}

// --- Joints ----------------------------------------------------------------------------------

JointHandle PhysicsWorld::create_joint(const JointDesc& d) {
    auto& m = *impl_;
    const auto fail = [&](std::string_view why) {
        m.error(std::string("create_joint: ") + std::string(why));
        return JointHandle{};
    };
    const Impl::BodyRecord* ra = d.body_a.valid() ? m.record(d.body_a) : nullptr;
    const Impl::BodyRecord* rb = m.record(d.body_b);
    if (d.body_a.valid() && !ra) return fail("body_a is not a live body");
    if (!rb) return fail("body_b is not a live body");
    if (d.body_a == d.body_b) return fail("a joint needs two different bodies");
    if (!finite_vec(d.anchor) || !finite_vec(d.anchor_b)) return fail("the anchor must be finite");
    const bool axis_needed = d.type == JointType::Hinge || d.type == JointType::Slider;
    if (axis_needed && (!finite_vec(d.axis) || length(d.axis) < 1e-6f)) return fail("hinge and slider need a non-zero axis");

    // Lock the bodies for writing while the constraint is created.
    JPH::BodyID ids[2];
    int id_count = 0;
    if (ra) ids[id_count++] = ra->id;
    ids[id_count++] = rb->id;
    std::optional<JPH::BodyLockMultiWrite> lock;
    lock.emplace(m.system->GetBodyLockInterface(), ids, id_count);
    JPH::Body* body_b = lock->GetBody(id_count - 1);
    JPH::Body* body_a = ra ? lock->GetBody(0) : &JPH::Body::sFixedToWorld;
    if (!body_a || !body_b) return fail("a body could not be locked");
    if (!body_b->IsDynamic() && !body_a->IsDynamic()) return fail("at least one body must be dynamic");

    JPH::Ref<JPH::TwoBodyConstraint> constraint = build_jolt_constraint(d, *body_a, *body_b);
    if (!constraint) return fail("Jolt refused the joint");
    lock.reset(); // unlock the bodies before the world sees the constraint
    m.system->AddConstraint(constraint);
    std::uint64_t pair_key = 0;
    if (!d.collide_connected && ra) { // bodies of a joint do not collide with each other (unless asked)
        pair_key = Impl::pair_key(ra->id, rb->id);
        ++m.pair_filter->disabled[pair_key];
    }
    return m.joints.create({constraint, d.body_a, d.body_b, pair_key});
}

void PhysicsWorld::destroy(JointHandle joint) {
    auto& m = *impl_;
    if (auto* r = m.joints.get(joint)) {
        if (r->pair_key != 0) {
            auto it = m.pair_filter->disabled.find(r->pair_key);
            if (it != m.pair_filter->disabled.end() && --it->second <= 0) m.pair_filter->disabled.erase(it);
        }
        m.system->RemoveConstraint(r->constraint);
        m.joints.destroy(joint);
    }
}

bool PhysicsWorld::valid(JointHandle joint) const noexcept { return impl_->joints.get(joint) != nullptr; }

// --- Characters ------------------------------------------------------------------------------

CharacterHandle PhysicsWorld::create_character(const CharacterDesc& desc) {
    auto& m = *impl_;
    if (!positive_scalar(desc.radius) || !positive_scalar(desc.height)) {
        m.error("create_character: radius and height must be positive");
        return {};
    }
    // The capsule is at least a sphere: height below 2 * radius is clamped.
    const float half_cylinder = std::max(0.01f, desc.height * 0.5f - desc.radius);
    auto capsule_shape = JPH::CapsuleShapeSettings(half_cylinder, desc.radius).Create();
    if (!capsule_shape.IsValid()) return {};
    // Capsule standing on the character's position (feet at the origin of the shape).
    JPH::RotatedTranslatedShapeSettings capsule(JPH::Vec3(0, half_cylinder + desc.radius, 0), JPH::Quat::sIdentity(),
                                                capsule_shape.Get());
    auto shape = capsule.Create();
    if (!shape.IsValid()) return {};
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = shape.Get();
    settings->mMaxSlopeAngle = desc.max_slope;
    settings->mMass = desc.mass;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.radius); // contacts below the sphere center support
    // Inner body: lets raycasts and sensors see the character (user data = the character's).
    settings->mInnerBodyShape = shape.Get();
    settings->mInnerBodyLayer = physics_layers::moving;
    JPH::Ref<JPH::CharacterVirtual> character =
        new JPH::CharacterVirtual(settings, JPH::RVec3(to_jph(desc.position)), JPH::Quat::sIdentity(), desc.user_data, m.system.get());
    character->SetCharacterVsCharacterCollision(&m.character_collision);
    m.character_collision.Add(character);
    return m.characters.create({character});
}

void PhysicsWorld::destroy(CharacterHandle character) {
    auto& m = *impl_;
    if (auto* r = m.characters.get(character)) {
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
        next = c.GetGroundVelocity(); // stick to (moving) ground
        if (jump) next += JPH::Vec3(0, jump_speed, 0);
    } else {
        next = JPH::Vec3(0, current.GetY(), 0); // keep vertical motion in the air
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
