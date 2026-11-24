#include "physics_internal.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <cfloat>
#include <optional>

namespace astral {

namespace {

JPH::EMotorState to_jph_motor(JointMotorMode mode) noexcept {
    switch (mode) {
    case JointMotorMode::Off: return JPH::EMotorState::Off;
    case JointMotorMode::Velocity: return JPH::EMotorState::Velocity;
    case JointMotorMode::Position: return JPH::EMotorState::Position;
    }
    return JPH::EMotorState::Off;
}

JPH::MotorSettings make_motor_settings(const JointMotorDesc& m) {
    JPH::MotorSettings s(JPH::ESpringMode::FrequencyAndDamping, std::max(m.frequency_hz, 0.0f), std::max(m.damping, 0.0f));
    if (m.max_force > 0.0f) s.SetForceLimit(m.max_force);
    if (m.max_torque > 0.0f) s.SetTorqueLimit(m.max_torque);
    return s;
}

JPH::Vec3 resolve_secondary_axis(const JPH::Vec3& primary, Vec3 requested) {
    JPH::Vec3 sec = (finite_vec(requested) && length(requested) > 1e-6f) ? to_jph(normalize(requested)) : primary.GetNormalizedPerpendicular();
    const JPH::Vec3 ortho = sec - primary * sec.Dot(primary);
    return ortho.LengthSq() > 1e-6f ? ortho.Normalized() : primary.GetNormalizedPerpendicular();
}

void configure_six_dof_axis(JPH::SixDOFConstraintSettings& s, JPH::SixDOFConstraintSettings::EAxis axis, float lo, float hi) {
    if (lo > hi) s.MakeFixedAxis(axis);
    else if (lo <= -JointLimitsDesc::free_limit && hi >= JointLimitsDesc::free_limit) s.MakeFreeAxis(axis);
    else s.SetLimitedAxis(axis, lo, hi);
}

JPH::Ref<JPH::TwoBodyConstraint> build_six_dof(const JointDesc& d, JPH::Body& body_a, JPH::Body& body_b,
                                               const JPH::RVec3& anchor, const JPH::Vec3& axis, const JPH::Vec3& normal,
                                               const JPH::SpringSettings& spring) {
    JPH::SixDOFConstraintSettings s;
    s.mSpace = JPH::EConstraintSpace::WorldSpace;
    s.mPosition1 = s.mPosition2 = anchor;
    s.mAxisX1 = s.mAxisX2 = axis;
    s.mAxisY1 = s.mAxisY2 = normal;
    s.mSwingType = d.limits.swing_type == JointSwingType::Pyramid ? JPH::ESwingType::Pyramid : JPH::ESwingType::Cone;
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::TranslationX, d.limits.linear_min.x, d.limits.linear_max.x);
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::TranslationY, d.limits.linear_min.y, d.limits.linear_max.y);
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::TranslationZ, d.limits.linear_min.z, d.limits.linear_max.z);
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::RotationX, d.limits.angular_min.x, d.limits.angular_max.x);
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::RotationY, d.limits.angular_min.y, d.limits.angular_max.y);
    configure_six_dof_axis(s, JPH::SixDOFConstraintSettings::RotationZ, d.limits.angular_min.z, d.limits.angular_max.z);
    for (int i = 0; i < 3; ++i) s.mLimitsSpringSettings[i] = spring;
    const JPH::MotorSettings ms = make_motor_settings(d.motor);
    for (int i = 0; i < JPH::SixDOFConstraintSettings::Num; ++i) {
        s.mMaxFriction[i] = d.max_friction;
        s.mMotorSettings[i] = ms;
    }
    return s.Create(body_a, body_b);
}

JPH::Ref<JPH::TwoBodyConstraint> build_swing_twist(const JointDesc& d, JPH::Body& body_a, JPH::Body& body_b,
                                                   const JPH::RVec3& anchor, const JPH::Vec3& axis, const JPH::Vec3& normal) {
    JPH::SwingTwistConstraintSettings s;
    s.mSpace = JPH::EConstraintSpace::WorldSpace;
    s.mPosition1 = s.mPosition2 = anchor;
    s.mTwistAxis1 = s.mTwistAxis2 = axis;
    s.mPlaneAxis1 = s.mPlaneAxis2 = normal;
    s.mSwingType = d.limits.swing_type == JointSwingType::Pyramid ? JPH::ESwingType::Pyramid : JPH::ESwingType::Cone;
    s.mNormalHalfConeAngle = std::clamp(d.limits.swing_normal_cone, 0.0f, JPH::JPH_PI);
    s.mPlaneHalfConeAngle = std::clamp(d.limits.swing_plane_cone, 0.0f, JPH::JPH_PI);
    s.mTwistMinAngle = std::clamp(d.limits.twist_min, -JPH::JPH_PI, JPH::JPH_PI);
    s.mTwistMaxAngle = std::clamp(d.limits.twist_max, s.mTwistMinAngle, JPH::JPH_PI);
    s.mMaxFrictionTorque = d.max_friction;
    const JPH::MotorSettings ms = make_motor_settings(d.motor);
    s.mSwingMotorSettings = ms;
    s.mTwistMotorSettings = ms;
    return s.Create(body_a, body_b);
}

void apply_motor_state(JPH::TwoBodyConstraint& constraint, const JointDesc& d) {
    const JPH::EMotorState state = to_jph_motor(d.motor.mode);
    switch (d.type) {
    case JointType::Hinge: {
        auto& h = static_cast<JPH::HingeConstraint&>(constraint);
        h.SetMotorState(state);
        h.SetTargetAngularVelocity(d.motor.target_velocity);
        h.SetTargetAngle(d.motor.target_position);
        break;
    }
    case JointType::Slider: {
        auto& s = static_cast<JPH::SliderConstraint&>(constraint);
        s.SetMotorState(state);
        s.SetTargetVelocity(d.motor.target_velocity);
        s.SetTargetPosition(d.motor.target_position);
        break;
    }
    case JointType::SixDOF: {
        auto& c = static_cast<JPH::SixDOFConstraint&>(constraint);
        for (int i = JPH::SixDOFConstraintSettings::RotationX; i <= JPH::SixDOFConstraintSettings::RotationZ; ++i) {
            const auto ax = static_cast<JPH::SixDOFConstraintSettings::EAxis>(i);
            if (!c.IsFixedAxis(ax)) c.SetMotorState(ax, state);
        }
        c.SetTargetAngularVelocityCS(to_jph(d.motor.target_angular_velocity));
        c.SetTargetOrientationCS(to_jph(d.motor.target_rotation));
        break;
    }
    case JointType::SwingTwist: {
        auto& c = static_cast<JPH::SwingTwistConstraint&>(constraint);
        c.SetSwingMotorState(state);
        c.SetTwistMotorState(state);
        c.SetTargetAngularVelocityCS(to_jph(d.motor.target_angular_velocity));
        c.SetTargetOrientationCS(to_jph(d.motor.target_rotation));
        break;
    }
    default: break;
    }
}

JPH::Ref<JPH::TwoBodyConstraint> build_jolt_constraint(const JointDesc& d, JPH::Body& body_a, JPH::Body& body_b) {
    const JPH::RVec3 anchor(to_jph(d.anchor));
    const JPH::Vec3 axis = (finite_vec(d.axis) && length(d.axis) > 1e-6f) ? to_jph(normalize(d.axis)) : JPH::Vec3::sAxisY();
    const JPH::Vec3 normal = resolve_secondary_axis(axis, d.secondary_axis);
    const JPH::SpringSettings spring(JPH::ESpringMode::FrequencyAndDamping, d.spring_frequency, d.spring_damping);
    JPH::Ref<JPH::TwoBodyConstraint> out;
    switch (d.type) {
    case JointType::Fixed: {
        JPH::FixedConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mAutoDetectPoint = true;
        out = s.Create(body_a, body_b);
        break;
    }
    case JointType::Point: {
        JPH::PointConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = s.mPoint2 = anchor;
        out = s.Create(body_a, body_b);
        break;
    }
    case JointType::Hinge: {
        JPH::HingeConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = s.mPoint2 = anchor;
        s.mHingeAxis1 = s.mHingeAxis2 = axis;
        s.mNormalAxis1 = s.mNormalAxis2 = normal;
        s.mMaxFrictionTorque = d.max_friction;
        s.mMotorSettings = make_motor_settings(d.motor);
        if (d.limited) {
            s.mLimitsMin = d.min_limit;
            s.mLimitsMax = d.max_limit;
            s.mLimitsSpringSettings = spring;
        }
        out = s.Create(body_a, body_b);
        break;
    }
    case JointType::Slider: {
        JPH::SliderConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mAutoDetectPoint = false;
        s.mPoint1 = s.mPoint2 = anchor;
        s.mSliderAxis1 = s.mSliderAxis2 = axis;
        s.mNormalAxis1 = s.mNormalAxis2 = normal;
        s.mMaxFrictionForce = d.max_friction;
        s.mMotorSettings = make_motor_settings(d.motor);
        if (d.limited) {
            s.mLimitsMin = d.min_limit;
            s.mLimitsMax = d.max_limit;
            s.mLimitsSpringSettings = spring;
        }
        out = s.Create(body_a, body_b);
        break;
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
            s.mMinDistance = s.mMaxDistance = -1.0f;
        }
        s.mLimitsSpringSettings = spring;
        out = s.Create(body_a, body_b);
        break;
    }
    case JointType::SixDOF: out = build_six_dof(d, body_a, body_b, anchor, axis, normal, spring); break;
    case JointType::SwingTwist: out = build_swing_twist(d, body_a, body_b, anchor, axis, normal); break;
    }
    if (out && d.motor.mode != JointMotorMode::Off) apply_motor_state(*out, d);
    return out;
}

JointReaction compute_joint_reaction(const JPH::TwoBodyConstraint& c, JointType type, float dt) {
    if (dt <= 0.0f) return {};
    float lin = 0.0f;
    float ang = 0.0f;
    switch (type) {
    case JointType::Fixed: {
        const auto& f = static_cast<const JPH::FixedConstraint&>(c);
        lin = f.GetTotalLambdaPosition().Length();
        ang = f.GetTotalLambdaRotation().Length();
        break;
    }
    case JointType::Point: {
        lin = static_cast<const JPH::PointConstraint&>(c).GetTotalLambdaPosition().Length();
        break;
    }
    case JointType::Hinge: {
        const auto& h = static_cast<const JPH::HingeConstraint&>(c);
        lin = h.GetTotalLambdaPosition().Length();
        const auto rot = h.GetTotalLambdaRotation();
        const float lim = h.GetTotalLambdaRotationLimits();
        const float mot = h.GetTotalLambdaMotor();
        ang = std::sqrt(rot[0] * rot[0] + rot[1] * rot[1] + lim * lim + mot * mot);
        break;
    }
    case JointType::Slider: {
        const auto& s = static_cast<const JPH::SliderConstraint&>(c);
        const auto pos = s.GetTotalLambdaPosition();
        const float lim = s.GetTotalLambdaPositionLimits();
        const float mot = s.GetTotalLambdaMotor();
        lin = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + lim * lim + mot * mot);
        ang = s.GetTotalLambdaRotation().Length();
        break;
    }
    case JointType::Distance: {
        lin = std::abs(static_cast<const JPH::DistanceConstraint&>(c).GetTotalLambdaPosition());
        break;
    }
    case JointType::SixDOF: {
        const auto& s = static_cast<const JPH::SixDOFConstraint&>(c);
        lin = s.GetTotalLambdaPosition().Length() + s.GetTotalLambdaMotorTranslation().Length();
        ang = s.GetTotalLambdaRotation().Length() + s.GetTotalLambdaMotorRotation().Length();
        break;
    }
    case JointType::SwingTwist: {
        const auto& s = static_cast<const JPH::SwingTwistConstraint&>(c);
        lin = s.GetTotalLambdaPosition().Length();
        const float tw = s.GetTotalLambdaTwist(), sy = s.GetTotalLambdaSwingY(), sz = s.GetTotalLambdaSwingZ();
        ang = std::sqrt(tw * tw + sy * sy + sz * sz) + s.GetTotalLambdaMotor().Length();
        break;
    }
    }
    return {lin / dt, ang / dt};
}

void release_joint_constraint(PhysicsWorld::Impl& m, PhysicsWorld::Impl::JointRecord& r) {
    if (r.pair_key != 0) {
        auto it = m.pair_filter->disabled.find(r.pair_key);
        if (it != m.pair_filter->disabled.end() && --it->second <= 0) m.pair_filter->disabled.erase(it);
        r.pair_key = 0;
    }
    if (r.constraint) {
        m.system->RemoveConstraint(r.constraint);
        r.constraint = nullptr;
    }
}

void wake_joint_bodies(PhysicsWorld::Impl& m, const PhysicsWorld::Impl::JointRecord& r) {
    if (const auto* ra = m.record(r.a)) m.bi().ActivateBody(ra->id);
    if (const auto* rb = m.record(r.b)) m.bi().ActivateBody(rb->id);
}

} // namespace

void check_breakable_joints(PhysicsWorld::Impl& m, float dt) {
    m.joints.for_each([&](JointHandle handle, PhysicsWorld::Impl::JointRecord& r) {
        if (r.broken || !r.constraint) return;
        r.last_reaction = compute_joint_reaction(*r.constraint, r.desc.type, dt);
        const bool over_force = r.desc.break_force > 0.0f && r.last_reaction.force >= r.desc.break_force;
        const bool over_torque = r.desc.break_torque > 0.0f && r.last_reaction.torque >= r.desc.break_torque;
        if (!over_force && !over_torque) return;
        r.broken = true;
        release_joint_constraint(m, r);
        wake_joint_bodies(m, r);
        m.broken_joints.push_back({handle, r.a, r.b, r.last_reaction.force, r.last_reaction.torque});
    });
}

void shift_world_anchored_joints(PhysicsWorld::Impl& m, Vec3 offset) {
    m.joints.for_each([&](JointHandle, PhysicsWorld::Impl::JointRecord& r) {
        if (r.broken || !r.constraint || r.a.valid()) return; // 2-body joints stay valid in body-local space
        r.desc.anchor = r.desc.anchor - offset;
        r.desc.anchor_b = r.desc.anchor_b - offset;
        const auto* rb = m.record(r.b);
        if (!rb) return;
        m.system->RemoveConstraint(r.constraint);
        JPH::BodyLockWrite lock(m.system->GetBodyLockInterface(), rb->id);
        if (!lock.Succeeded()) {
            r.constraint = nullptr;
            return;
        }
        r.constraint = build_jolt_constraint(r.desc, JPH::Body::sFixedToWorld, lock.GetBody());
        lock.ReleaseLock();
        if (r.constraint) m.system->AddConstraint(r.constraint);
    });
}

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
    const bool axis_needed = d.type == JointType::Hinge || d.type == JointType::Slider ||
                             d.type == JointType::SixDOF || d.type == JointType::SwingTwist;
    if (axis_needed && (!finite_vec(d.axis) || length(d.axis) < 1e-6f)) return fail("joint needs a non-zero axis");

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
    lock.reset();
    m.system->AddConstraint(constraint);
    std::uint64_t pair_key = 0;
    if (!d.collide_connected && ra) {
        pair_key = Impl::pair_key(ra->id, rb->id);
        ++m.pair_filter->disabled[pair_key];
    }
    return m.joints.create({constraint, d.body_a, d.body_b, pair_key, d, false, {}});
}

void PhysicsWorld::destroy(JointHandle joint) {
    auto& m = *impl_;
    if (auto* r = m.joints.get(joint)) {
        release_joint_constraint(m, *r);
        m.joints.destroy(joint);
    }
}

bool PhysicsWorld::valid(JointHandle joint) const noexcept { return impl_->joints.get(joint) != nullptr; }

void PhysicsWorld::set_joint_motor_tuning(JointHandle joint, float frequency_hz, float damping, float max_force,
                                         float max_torque) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint || !std::isfinite(frequency_hz) || !std::isfinite(damping) ||
        !std::isfinite(max_force) || !std::isfinite(max_torque)) return;
    JointMotorDesc& motor = r->desc.motor;
    if (motor.frequency_hz == frequency_hz && motor.damping == damping &&
        motor.max_force == max_force && motor.max_torque == max_torque) return;
    motor.frequency_hz = frequency_hz;
    motor.damping = damping;
    motor.max_force = max_force;
    motor.max_torque = max_torque;
    const auto settings = make_motor_settings(motor);
    switch (r->desc.type) {
    case JointType::Hinge:
        static_cast<JPH::HingeConstraint&>(*r->constraint).GetMotorSettings() = settings;
        break;
    case JointType::Slider:
        static_cast<JPH::SliderConstraint&>(*r->constraint).GetMotorSettings() = settings;
        break;
    case JointType::SixDOF: {
        auto& constraint = static_cast<JPH::SixDOFConstraint&>(*r->constraint);
        for (int axis = 0; axis < JPH::SixDOFConstraintSettings::Num; ++axis) {
            constraint.GetMotorSettings(static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis)) = settings;
        }
        break;
    }
    case JointType::SwingTwist: {
        auto& constraint = static_cast<JPH::SwingTwistConstraint&>(*r->constraint);
        constraint.GetSwingMotorSettings() = settings;
        constraint.GetTwistMotorSettings() = settings;
        break;
    }
    default: return;
    }
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_motor_mode(JointHandle joint, JointMotorMode mode) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint) return;
    r->desc.motor.mode = mode;
    apply_motor_state(*r->constraint, r->desc);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_motor_velocity(JointHandle joint, float velocity) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint || !std::isfinite(velocity)) return;
    r->desc.motor.target_velocity = velocity;
    apply_motor_state(*r->constraint, r->desc);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_motor_position(JointHandle joint, float position) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint || !std::isfinite(position)) return;
    r->desc.motor.target_position = position;
    apply_motor_state(*r->constraint, r->desc);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_motor_angular_velocity(JointHandle joint, Vec3 angular_velocity) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint || !finite_vec(angular_velocity)) return;
    r->desc.motor.target_angular_velocity = angular_velocity;
    apply_motor_state(*r->constraint, r->desc);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_motor_rotation(JointHandle joint, Quat target_rotation) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint) return;
    r->desc.motor.target_rotation = target_rotation;
    apply_motor_state(*r->constraint, r->desc);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_limits(JointHandle joint, float min_limit, float max_limit) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint || !std::isfinite(min_limit) || !std::isfinite(max_limit)) return;
    r->desc.limited = true;
    r->desc.min_limit = min_limit;
    r->desc.max_limit = max_limit;
    if (r->desc.type == JointType::Hinge) static_cast<JPH::HingeConstraint&>(*r->constraint).SetLimits(min_limit, max_limit);
    else if (r->desc.type == JointType::Slider) static_cast<JPH::SliderConstraint&>(*r->constraint).SetLimits(min_limit, max_limit);
    else if (r->desc.type == JointType::Distance) static_cast<JPH::DistanceConstraint&>(*r->constraint).SetDistance(min_limit, max_limit);
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::set_joint_limits(JointHandle joint, const JointLimitsDesc& limits) {
    auto* r = impl_->joints.get(joint);
    if (!r || r->broken || !r->constraint) return;
    r->desc.limits = limits;
    if (r->desc.type == JointType::SixDOF) {
        auto& c = static_cast<JPH::SixDOFConstraint&>(*r->constraint);
        c.SetTranslationLimits(to_jph(limits.linear_min), to_jph(limits.linear_max));
        c.SetRotationLimits(to_jph(limits.angular_min), to_jph(limits.angular_max));
    } else if (r->desc.type == JointType::SwingTwist) {
        auto& c = static_cast<JPH::SwingTwistConstraint&>(*r->constraint);
        c.SetNormalHalfConeAngle(std::clamp(limits.swing_normal_cone, 0.0f, JPH::JPH_PI));
        c.SetPlaneHalfConeAngle(std::clamp(limits.swing_plane_cone, 0.0f, JPH::JPH_PI));
        c.SetTwistMinAngle(std::clamp(limits.twist_min, -JPH::JPH_PI, JPH::JPH_PI));
        c.SetTwistMaxAngle(std::clamp(limits.twist_max, std::clamp(limits.twist_min, -JPH::JPH_PI, JPH::JPH_PI), JPH::JPH_PI));
    }
    wake_joint_bodies(*impl_, *r);
}

void PhysicsWorld::break_joint(JointHandle joint) {
    auto& m = *impl_;
    auto* r = m.joints.get(joint);
    if (!r || r->broken) return;
    r->broken = true;
    release_joint_constraint(m, *r);
    wake_joint_bodies(m, *r);
    m.broken_joints.push_back({joint, r->a, r->b, r->last_reaction.force, r->last_reaction.torque});
}

bool PhysicsWorld::joint_broken(JointHandle joint) const noexcept {
    const auto* r = impl_->joints.get(joint);
    return r && r->broken;
}

JointReaction PhysicsWorld::joint_reaction(JointHandle joint) const noexcept {
    const auto* r = impl_->joints.get(joint);
    return r ? r->last_reaction : JointReaction{};
}

std::span<const JointBrokenEvent> PhysicsWorld::broken_joints() const noexcept { return impl_->broken_joints; }

} // namespace astral
