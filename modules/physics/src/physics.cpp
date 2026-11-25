#include "physics_internal.hpp"

#include <utility>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <thread>

namespace astral {

namespace {

// --- Process-wide Jolt registry (reference counted) ---------------------------------------
std::mutex registry_mutex;
int registry_users = 0;

void jolt_trace(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::fprintf(stderr, "[jolt] ");
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

#ifdef JPH_ENABLE_ASSERTS
bool jolt_assert(const char* expression, const char* message, const char* file, JPH::uint line) {
    std::fprintf(stderr, "[jolt] assertion failed: %s %s (%s:%u)\n", expression, message ? message : "", file, line);
    return false; // log and continue instead of trapping
}
#endif

constexpr int max_physics_jobs = 2048;
constexpr int max_physics_barriers = 8;

} // namespace

void acquire_jolt_registry() {
    std::scoped_lock lock(registry_mutex);
    if (registry_users++ == 0) {
        JPH::Trace = jolt_trace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = jolt_assert;)
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

void release_jolt_registry() {
    std::scoped_lock lock(registry_mutex);
    if (--registry_users == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

PhysicsWorld::PhysicsWorld(const PhysicsDesc& desc) : impl_(std::make_unique<Impl>()) {
    acquire_jolt_registry();
    auto& m = *impl_;
    m.collide.fill(0xFFFFFFFFu);
    m.pair_filter = new Impl::PairFilter();
    m.logger = desc.logger;
    unsigned threads = desc.threads;
    if (threads == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threads = hw > 1 ? hw - 1 : 1;
    }
    m.temp = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024);
    m.jobs = std::make_unique<JPH::JobSystemThreadPool>(max_physics_jobs, max_physics_barriers, static_cast<int>(threads));
    m.system = std::make_unique<JPH::PhysicsSystem>();
    m.system->Init(desc.max_bodies, 0, desc.max_body_pairs, desc.max_contact_constraints, m.broad_phase_layers,
                   m.object_vs_broad_phase, m.object_pairs);
    m.listener = std::make_unique<Impl::Contacts>(m);
    m.system->SetContactListener(m.listener.get());
    m.system->SetGravity(to_jph(desc.gravity));
    m.handle_by_index.resize(desc.max_bodies);
}

PhysicsWorld::~PhysicsWorld() {
    auto& m = *impl_;
    m.joints.for_each([&](JointHandle, Impl::JointRecord& j) {
        if (j.constraint) m.system->RemoveConstraint(j.constraint);
    });
    m.characters.for_each([&](CharacterHandle, Impl::CharacterRecord& c) {
        m.character_collision.Remove(c.character);
        c.character = nullptr; // also removes its inner body
    });
    m.bodies.for_each([&](BodyHandle, Impl::BodyRecord& r) {
        m.bi().RemoveBody(r.id);
        m.bi().DestroyBody(r.id);
    });
    m.shapes.for_each([](ShapeHandle, Impl::ShapeRecord& s) { s.shape = nullptr; });
    m.system.reset();
    m.jobs.reset();
    m.temp.reset();
    release_jolt_registry();
}

PhysicsCapabilities PhysicsWorld::capabilities() const noexcept {
    constexpr std::uint32_t all = static_cast<std::uint32_t>(PhysicsCapability::SixDOFJoints) |
                                  static_cast<std::uint32_t>(PhysicsCapability::SwingTwistJoints) |
                                  static_cast<std::uint32_t>(PhysicsCapability::JointMotors) |
                                  static_cast<std::uint32_t>(PhysicsCapability::BreakableJoints) |
                                  static_cast<std::uint32_t>(PhysicsCapability::CookedShapes) |
                                  static_cast<std::uint32_t>(PhysicsCapability::MaterialTable) |
                                  static_cast<std::uint32_t>(PhysicsCapability::BatchedQueries) |
                                  static_cast<std::uint32_t>(PhysicsCapability::OriginShift);
    return {all};
}

void PhysicsWorld::register_material(std::uint16_t surface, const PhysicsMaterialDesc& desc) {
    impl_->materials[surface] = desc;
}

std::optional<PhysicsMaterialDesc> PhysicsWorld::material(std::uint16_t surface) const {
    const auto* mat = impl_->material_of(surface);
    return mat ? std::optional<PhysicsMaterialDesc>(*mat) : std::nullopt;
}

void PhysicsWorld::clear_material(std::uint16_t surface) { impl_->materials.erase(surface); }

BodyHandle PhysicsWorld::create_body(const BodyDesc& desc) {
    auto& m = *impl_;
    JPH::Ref<JPH::Shape> shape;
    bool static_only = false;
    if (desc.cooked_shape.valid()) {
        const auto* rec = m.shapes.get(desc.cooked_shape);
        if (!rec) {
            m.error("create_body: invalid cooked_shape handle");
            return {};
        }
        shape = rec->shape;
        static_only = rec->static_only;
    } else {
        shape = make_jolt_shape(desc.shape);
        static_only = shape_needs_static(desc.shape);
    }
    if (!shape) {
        m.error("create_body: invalid shape (check dimensions, hull points, mesh indices, height field size)");
        return {};
    }
    if (desc.motion == MotionType::Dynamic && static_only) {
        m.error("create_body: meshes and height fields cannot be dynamic bodies (use a convex hull or a compound of convex shapes)");
        return {};
    }
    const bool is_static = desc.motion == MotionType::Static;
    const JPH::EMotionType motion = is_static ? JPH::EMotionType::Static
                                    : desc.motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic
                                                                           : JPH::EMotionType::Dynamic;
    if (desc.layer >= max_physics_layers) {
        m.error("create_body: layer must be below max_physics_layers (16)");
        return {};
    }
    const PhysicsMaterialDesc* mat = m.material_of(desc.surface);
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(to_jph(desc.position)), to_jph(desc.rotation), motion,
                                       physics_layers::object_layer(desc.layer, !is_static));
    settings.mFriction = mat ? mat->friction : desc.friction;
    settings.mRestitution = mat ? mat->restitution : desc.restitution;
    settings.mLinearDamping = desc.linear_damping;
    settings.mAngularDamping = desc.angular_damping;
    settings.mGravityFactor = desc.gravity_factor;
    settings.mIsSensor = desc.sensor;
    settings.mCollideKinematicVsNonDynamic = desc.sensor;
    settings.mLinearVelocity = to_jph(desc.linear_velocity);
    settings.mUserData = desc.user_data;
    if (desc.continuous) settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    if (motion == JPH::EMotionType::Dynamic) {
        if (desc.mass > 0.0f) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        } else if (mat && positive_scalar(mat->density)) {
            const float volume = shape->GetVolume();
            if (volume > 0.0f) {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = volume * mat->density;
            }
        }
    }
    const JPH::BodyID id = m.bi().CreateAndAddBody(
        settings, is_static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        m.error("create_body: max_bodies reached");
        return {};
    }
    {
        JPH::BodyLockWrite lock(m.system->GetBodyLockInterface(), id);
        if (lock.Succeeded()) {
            lock.GetBody().SetCollisionGroup(JPH::CollisionGroup(m.pair_filter, 1, id.GetIndex()));
        }
    }
    const BodyHandle handle = m.bodies.create({id, desc.user_data, desc.surface, desc.sensor});
    m.handle_by_index[id.GetIndex()] = {handle, id};
    m.info_by_id[id.GetIndexAndSequenceNumber()] = {desc.user_data, desc.surface, desc.sensor};
    return handle;
}

void PhysicsWorld::destroy(BodyHandle body) {
    auto& m = *impl_;
    const Impl::BodyRecord* r = m.bodies.get(body);
    if (!r) return;
    std::vector<JointHandle> doomed; // joints must not outlive the bodies they connect
    m.joints.for_each([&](JointHandle h, Impl::JointRecord& j) {
        if (j.a == body || j.b == body) doomed.push_back(h);
    });
    for (JointHandle h : doomed) destroy(h);
    const JPH::BodyID id = r->id;
    m.bi().RemoveBody(id);
    m.bi().DestroyBody(id);
    m.handle_by_index[id.GetIndex()] = {};
    m.retired_info.push_back(id.GetIndexAndSequenceNumber());
    m.bodies.destroy(body);
}

bool PhysicsWorld::valid(BodyHandle body) const noexcept { return impl_->bodies.get(body) != nullptr; }
std::size_t PhysicsWorld::body_count() const noexcept { return impl_->bodies.size(); }

Vec3 PhysicsWorld::position(BodyHandle body) const {
    const auto* r = impl_->record(body);
    return r ? from_jph(JPH::Vec3(impl_->bi().GetPosition(r->id))) : Vec3{};
}

Quat PhysicsWorld::rotation(BodyHandle body) const {
    const auto* r = impl_->record(body);
    return r ? from_jph(impl_->bi().GetRotation(r->id)) : Quat{};
}

Vec3 PhysicsWorld::linear_velocity(BodyHandle body) const {
    const auto* r = impl_->record(body);
    return r ? from_jph(impl_->bi().GetLinearVelocity(r->id)) : Vec3{};
}

float PhysicsWorld::mass(BodyHandle body) const {
    const auto* r = impl_->record(body);
    if (!r) return 0.0f;
    JPH::BodyLockRead lock(impl_->system->GetBodyLockInterface(), r->id);
    if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) return 0.0f;
    const float inverse = lock.GetBody().GetMotionProperties()->GetInverseMass();
    return inverse > 0.0f ? 1.0f / inverse : 0.0f;
}

bool PhysicsWorld::active(BodyHandle body) const {
    const auto* r = impl_->record(body);
    return r && impl_->bi().IsActive(r->id);
}

std::uint64_t PhysicsWorld::user_data(BodyHandle body) const { return impl_->user_data_of(body); }

void PhysicsWorld::set_transform(BodyHandle body, Vec3 position, Quat rotation) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().SetPositionAndRotation(r->id, JPH::RVec3(to_jph(position)), to_jph(rotation), JPH::EActivation::Activate);
}

void PhysicsWorld::move_kinematic(BodyHandle body, Vec3 position, Quat rotation, float dt) {
    const auto* r = impl_->record(body);
    if (r && dt > 0.0f) impl_->bi().MoveKinematic(r->id, JPH::RVec3(to_jph(position)), to_jph(rotation), dt);
}

void PhysicsWorld::set_linear_velocity(BodyHandle body, Vec3 velocity) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().SetLinearVelocity(r->id, to_jph(velocity));
}

void PhysicsWorld::add_impulse(BodyHandle body, Vec3 impulse) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddImpulse(r->id, to_jph(impulse));
}

void PhysicsWorld::add_force(BodyHandle body, Vec3 force) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddForce(r->id, to_jph(force));
}

void PhysicsWorld::set_gravity(Vec3 gravity) { impl_->system->SetGravity(to_jph(gravity)); }
Vec3 PhysicsWorld::gravity() const { return from_jph(impl_->system->GetGravity()); }

Vec3 PhysicsWorld::angular_velocity(BodyHandle body) const {
    const auto* r = impl_->record(body);
    return r ? from_jph(impl_->bi().GetAngularVelocity(r->id)) : Vec3{};
}

void PhysicsWorld::set_angular_velocity(BodyHandle body, Vec3 velocity) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().SetAngularVelocity(r->id, to_jph(velocity));
}

void PhysicsWorld::add_torque(BodyHandle body, Vec3 torque) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddTorque(r->id, to_jph(torque));
}

void PhysicsWorld::add_angular_impulse(BodyHandle body, Vec3 impulse) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddAngularImpulse(r->id, to_jph(impulse));
}

void PhysicsWorld::add_force_at(BodyHandle body, Vec3 force, Vec3 point) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddForce(r->id, to_jph(force), JPH::RVec3(to_jph(point)));
}

void PhysicsWorld::wake(BodyHandle body) {
    if (const auto* r = impl_->record(body)) impl_->bi().ActivateBody(r->id);
}

void PhysicsWorld::sleep(BodyHandle body) {
    if (const auto* r = impl_->record(body)) impl_->bi().DeactivateBody(r->id);
}

void PhysicsWorld::step(float dt, int collision_steps) {
    auto& m = *impl_;
    m.contacts.clear();
    m.broken_joints.clear();
    if (dt <= 0.0f) return;
    // Bodies destroyed before this step: their removed contacts are reported during it.
    const std::vector<std::uint32_t> expiring = std::exchange(m.retired_info, {});
    const JPH::EPhysicsUpdateError error = m.system->Update(dt, std::max(collision_steps, 1), m.temp.get(), m.jobs.get());
    for (std::uint32_t key : expiring) m.info_by_id.erase(key);
    if (error != JPH::EPhysicsUpdateError::None) {
        m.error("physics step overflow: raise PhysicsDesc::max_body_pairs / max_contact_constraints");
    }
    check_breakable_joints(m, dt);
}

std::span<const ContactEvent> PhysicsWorld::contacts() const noexcept { return impl_->contacts; }

void PhysicsWorld::set_layer_collision(unsigned a, unsigned b, bool collide) {
    if (a >= max_physics_layers || b >= max_physics_layers) return;
    auto& matrix = impl_->collide;
    const auto set = [&](unsigned x, unsigned y) { matrix[x] = collide ? (matrix[x] | (1u << y)) : (matrix[x] & ~(1u << y)); };
    set(a, b);
    set(b, a);
}

bool PhysicsWorld::layers_collide(unsigned a, unsigned b) const noexcept {
    return a < max_physics_layers && b < max_physics_layers && ((impl_->collide[a] >> b) & 1u) != 0;
}

void PhysicsWorld::shift_origin(Vec3 offset) {
    const JPH::RVec3 delta(to_jph(offset));
    JPH::BodyInterface& bi = impl_->bi();
    impl_->bodies.for_each([&](BodyHandle, Impl::BodyRecord& r) {
        bi.SetPosition(r.id, bi.GetPosition(r.id) - delta, JPH::EActivation::DontActivate);
    });
    impl_->characters.for_each([&](CharacterHandle, Impl::CharacterRecord& r) {
        r.character->SetPosition(r.character->GetPosition() - delta);
    });
    shift_world_anchored_joints(*impl_, offset);
    impl_->system->OptimizeBroadPhase();
}

} // namespace astral
