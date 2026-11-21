#include <astral/core/log.hpp>
#include <astral/physics/physics.hpp>

// Jolt is a private dependency: none of these types appear in public headers.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

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

void acquire_jolt() {
    std::scoped_lock lock(registry_mutex);
    if (registry_users++ == 0) {
        JPH::Trace = jolt_trace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = jolt_assert;)
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

void release_jolt() {
    std::scoped_lock lock(registry_mutex);
    if (--registry_users == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

// --- Layers: static things never test against each other ----------------------------------
namespace layers {
constexpr JPH::ObjectLayer non_moving = 0;
constexpr JPH::ObjectLayer moving = 1;
constexpr JPH::BroadPhaseLayer bp_non_moving(0);
constexpr JPH::BroadPhaseLayer bp_moving(1);
} // namespace layers

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == layers::non_moving ? layers::bp_non_moving : layers::bp_moving;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == layers::bp_non_moving ? "non_moving" : "moving";
    }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        return layer == layers::moving || bp == layers::bp_moving;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == layers::moving || b == layers::moving;
    }
};

// --- Conversions ----------------------------------------------------------------------------
JPH::Vec3 to_jph(Vec3 v) { return {v.x, v.y, v.z}; }
JPH::Quat to_jph(Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
Vec3 from_jph(JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
Quat from_jph(JPH::Quat q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }

bool positive(float v) { return std::isfinite(v) && v > 0.0f; }

bool valid_shape(const ShapeDesc& d) {
    switch (d.type) {
    case ShapeType::Box: return positive(d.half_extents.x) && positive(d.half_extents.y) && positive(d.half_extents.z);
    case ShapeType::Sphere: return positive(d.radius);
    case ShapeType::Capsule:
    case ShapeType::Cylinder: return positive(d.radius) && positive(d.half_height);
    }
    return false;
}

JPH::Ref<JPH::Shape> make_shape(const ShapeDesc& d) {
    if (!valid_shape(d)) return nullptr;
    JPH::ShapeSettings::ShapeResult result;
    switch (d.type) {
    case ShapeType::Box: {
        const float convex = std::min({0.05f, d.half_extents.x * 0.5f, d.half_extents.y * 0.5f, d.half_extents.z * 0.5f});
        result = JPH::BoxShapeSettings(to_jph(d.half_extents), convex).Create();
        break;
    }
    case ShapeType::Sphere: result = JPH::SphereShapeSettings(d.radius).Create(); break;
    case ShapeType::Capsule: result = JPH::CapsuleShapeSettings(d.half_height, d.radius).Create(); break;
    case ShapeType::Cylinder: {
        const float convex = std::min({0.05f, d.radius * 0.5f, d.half_height * 0.5f});
        result = JPH::CylinderShapeSettings(d.half_height, d.radius, convex).Create();
        break;
    }
    }
    return result.IsValid() ? result.Get() : nullptr;
}

/// Ray filter: skips sensors and one optional body.
class RayBodyFilter final : public JPH::BodyFilter {
public:
    explicit RayBodyFilter(JPH::BodyID ignore) : ignore_(ignore) {}
    bool ShouldCollide(const JPH::BodyID& id) const override { return id != ignore_; }
    bool ShouldCollideLocked(const JPH::Body& body) const override { return !body.IsSensor(); }

private:
    JPH::BodyID ignore_;
};

constexpr int max_physics_jobs = 2048;
constexpr int max_physics_barriers = 8;

} // namespace

struct PhysicsWorld::Impl {
    struct BodyRecord {
        JPH::BodyID id;
        std::uint64_t user_data = 0;
    };
    struct CharacterRecord {
        JPH::Ref<JPH::CharacterVirtual> character;
    };
    /// Jolt body index -> our handle plus the full Jolt id (index + sequence), so a recycled
    /// index never resolves to a newer body.
    struct IndexEntry {
        BodyHandle handle;
        JPH::BodyID id;
    };

    /// Collects contacts from Jolt's worker threads.
    class Contacts final : public JPH::ContactListener {
    public:
        explicit Contacts(Impl& owner) : owner_(owner) {}
        void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold,
                            JPH::ContactSettings&) override {
            ContactEvent e;
            e.type = ContactEvent::Type::Begin;
            e.a = owner_.handle_of(a.GetID());
            e.b = owner_.handle_of(b.GetID());
            e.point = from_jph(JPH::Vec3(manifold.GetWorldSpaceContactPointOn1(0)));
            e.normal = from_jph(manifold.mWorldSpaceNormal);
            e.user_data_a = a.GetUserData();
            e.user_data_b = b.GetUserData();
            std::scoped_lock lock(mutex_);
            owner_.contacts.push_back(e);
        }
        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
            ContactEvent e;
            e.type = ContactEvent::Type::End;
            e.a = owner_.handle_of(pair.GetBody1ID());
            e.b = owner_.handle_of(pair.GetBody2ID());
            e.user_data_a = owner_.user_data_of(e.a); // 0 if that body is already gone
            e.user_data_b = owner_.user_data_of(e.b);
            std::scoped_lock lock(mutex_);
            owner_.contacts.push_back(e);
        }

    private:
        Impl& owner_;
        std::mutex mutex_;
    };

    BroadPhaseLayers broad_phase_layers;
    ObjectVsBroadPhase object_vs_broad_phase;
    ObjectPairs object_pairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystemThreadPool> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    std::unique_ptr<Contacts> listener;

    HandlePool<BodyRecord, BodyTag> bodies;
    std::vector<IndexEntry> handle_by_index; // read during steps, written only between them
    HandlePool<CharacterRecord, CharacterTag> characters;
    JPH::CharacterVsCharacterCollisionSimple character_collision;
    std::vector<ContactEvent> contacts;
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
    std::uint64_t user_data_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r ? r->user_data : 0;
    }
    JPH::BodyInterface& bi() { return system->GetBodyInterface(); }
    const JPH::BodyInterface& bi() const { return system->GetBodyInterface(); }
    const BodyRecord* record(BodyHandle h) const { return bodies.get(h); }
};

PhysicsWorld::PhysicsWorld(const PhysicsDesc& desc) : impl_(std::make_unique<Impl>()) {
    acquire_jolt();
    auto& m = *impl_;
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
    m.characters.for_each([&](CharacterHandle, Impl::CharacterRecord& c) {
        m.character_collision.Remove(c.character);
        c.character = nullptr; // also removes its inner body
    });
    m.bodies.for_each([&](BodyHandle, Impl::BodyRecord& r) {
        m.bi().RemoveBody(r.id);
        m.bi().DestroyBody(r.id);
    });
    m.system.reset();
    m.jobs.reset();
    m.temp.reset();
    release_jolt();
}

BodyHandle PhysicsWorld::create_body(const BodyDesc& desc) {
    auto& m = *impl_;
    JPH::Ref<JPH::Shape> shape = make_shape(desc.shape);
    if (!shape) {
        m.error("create_body: invalid shape dimensions");
        return {};
    }
    const bool is_static = desc.motion == MotionType::Static;
    const JPH::EMotionType motion = is_static ? JPH::EMotionType::Static
                                    : desc.motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic
                                                                           : JPH::EMotionType::Dynamic;
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(to_jph(desc.position)), to_jph(desc.rotation), motion,
                                       is_static ? layers::non_moving : layers::moving);
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    settings.mLinearDamping = desc.linear_damping;
    settings.mAngularDamping = desc.angular_damping;
    settings.mGravityFactor = desc.gravity_factor;
    settings.mIsSensor = desc.sensor;
    settings.mCollideKinematicVsNonDynamic = desc.sensor; // sensors also see static/kinematic bodies
    settings.mLinearVelocity = to_jph(desc.linear_velocity);
    settings.mUserData = desc.user_data;
    if (desc.continuous) settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    if (desc.mass > 0.0f && motion == JPH::EMotionType::Dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass;
    }
    const JPH::BodyID id = m.bi().CreateAndAddBody(
        settings, is_static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        m.error("create_body: max_bodies reached");
        return {};
    }
    const BodyHandle handle = m.bodies.create({id, desc.user_data});
    m.handle_by_index[id.GetIndex()] = {handle, id};
    return handle;
}

void PhysicsWorld::destroy(BodyHandle body) {
    auto& m = *impl_;
    const Impl::BodyRecord* r = m.bodies.get(body);
    if (!r) return;
    const JPH::BodyID id = r->id;
    m.bi().RemoveBody(id);
    m.bi().DestroyBody(id);
    m.handle_by_index[id.GetIndex()] = {};
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

void PhysicsWorld::step(float dt, int collision_steps) {
    auto& m = *impl_;
    m.contacts.clear();
    if (dt <= 0.0f) return;
    const JPH::EPhysicsUpdateError error = m.system->Update(dt, std::max(collision_steps, 1), m.temp.get(), m.jobs.get());
    if (error != JPH::EPhysicsUpdateError::None) {
        // Overflowing these buffers drops contacts: objects can fall through floors.
        m.error("physics step overflow: raise PhysicsDesc::max_body_pairs / max_contact_constraints");
    }
}

std::span<const ContactEvent> PhysicsWorld::contacts() const noexcept { return impl_->contacts; }

std::optional<RayHit> PhysicsWorld::raycast(Vec3 origin, Vec3 direction, float max_distance, BodyHandle ignore) const {
    const auto& m = *impl_;
    const Vec3 dir = normalize(direction);
    if (max_distance <= 0.0f || length(dir) == 0.0f) return std::nullopt;
    const JPH::RRayCast ray{JPH::RVec3(to_jph(origin)), to_jph(dir * max_distance)};
    JPH::RayCastResult result;
    const auto* ignored = m.record(ignore);
    const RayBodyFilter filter(ignored ? ignored->id : JPH::BodyID());
    if (!m.system->GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, filter)) return std::nullopt;

    RayHit hit;
    hit.body = m.handle_of(result.mBodyID);
    hit.distance = result.mFraction * max_distance;
    hit.position = origin + dir * hit.distance;
    JPH::BodyLockRead lock(m.system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        hit.normal = from_jph(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, JPH::RVec3(to_jph(hit.position))));
        hit.user_data = body.GetUserData();
    }
    return hit;
}

// --- Characters ------------------------------------------------------------------------------

CharacterHandle PhysicsWorld::create_character(const CharacterDesc& desc) {
    auto& m = *impl_;
    if (!positive(desc.radius) || !positive(desc.height)) {
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
    settings->mInnerBodyLayer = layers::moving;
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
    c.ExtendedUpdate(dt, gravity, update, m.system->GetDefaultBroadPhaseLayerFilter(layers::moving),
                     m.system->GetDefaultLayerFilter(layers::moving), {}, {}, *m.temp);
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
