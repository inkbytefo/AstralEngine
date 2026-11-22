#include <astral/core/log.hpp>
#include <astral/physics/physics.hpp>

// Jolt is a private dependency: none of these types appear in public headers.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
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
constexpr JPH::ObjectLayer non_moving = 0; // layer 0, static
constexpr JPH::ObjectLayer moving = 1;     // layer 0, moving
constexpr JPH::BroadPhaseLayer bp_non_moving(0);
constexpr JPH::BroadPhaseLayer bp_moving(1);
/// Jolt object layer = astral layer * 2 + (moving ? 1 : 0).
constexpr JPH::ObjectLayer object_layer(unsigned layer, bool is_moving) { return static_cast<JPH::ObjectLayer>(layer * 2 + (is_moving ? 1 : 0)); }
constexpr unsigned astral_layer(JPH::ObjectLayer l) { return l >> 1; }
constexpr bool is_moving(JPH::ObjectLayer l) { return (l & 1) != 0; }
} // namespace layers

/// collide[a] bit b: layers a and b collide.
using CollisionMatrix = std::array<std::uint32_t, max_physics_layers>;

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layers::is_moving(layer) ? layers::bp_moving : layers::bp_non_moving;
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
        return layers::is_moving(layer) || bp == layers::bp_moving;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    explicit ObjectPairs(const CollisionMatrix& matrix) : matrix_(matrix) {}
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (!layers::is_moving(a) && !layers::is_moving(b)) return false; // static things never test against each other
        return ((matrix_[layers::astral_layer(a)] >> layers::astral_layer(b)) & 1u) != 0;
    }

private:
    const CollisionMatrix& matrix_;
};

/// A query sees these layers only.
class LayerMask final : public JPH::ObjectLayerFilter {
public:
    explicit LayerMask(std::uint32_t mask) : mask_(mask) {}
    bool ShouldCollide(JPH::ObjectLayer layer) const override { return ((mask_ >> layers::astral_layer(layer)) & 1u) != 0; }

private:
    std::uint32_t mask_;
};

// --- Conversions ----------------------------------------------------------------------------
JPH::Vec3 to_jph(Vec3 v) { return {v.x, v.y, v.z}; }
JPH::Quat to_jph(Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
Vec3 from_jph(JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
Quat from_jph(JPH::Quat q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }

bool positive(float v) { return std::isfinite(v) && v > 0.0f; }

bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

/// Meshes and height fields are static geometry: they cannot be a moving body or be swept in a query.
bool needs_static(const ShapeDesc& d) {
    if (d.type == ShapeType::Mesh || d.type == ShapeType::HeightField) return true;
    for (const ShapeChild& c : d.children) {
        if (needs_static(c.shape)) return true;
    }
    return false;
}

/// A hull needs volume: some point must sit off the plane through the first three non-collinear points.
bool hull_has_volume(const std::vector<Vec3>& pts) {
    const Vec3 a = pts[0];
    Vec3 normal{};
    std::size_t used = 0;
    for (std::size_t i = 1; i + 1 < pts.size() && length(normal) < 1e-8f; ++i) {
        normal = cross(pts[i] - a, pts[i + 1] - a);
        used = i + 1;
    }
    if (length(normal) < 1e-8f) return false; // all on one line
    normal = normalize(normal);
    float extent = 0.0f;
    for (const Vec3& p : pts) extent = std::max(extent, length(p - a));
    for (std::size_t i = used + 1; i < pts.size(); ++i) {
        if (std::abs(dot(pts[i] - a, normal)) > 1e-4f * std::max(extent, 1.0f)) return true;
    }
    return false;
}

bool valid_shape(const ShapeDesc& d) {
    switch (d.type) {
    case ShapeType::Box: return positive(d.half_extents.x) && positive(d.half_extents.y) && positive(d.half_extents.z);
    case ShapeType::Sphere: return positive(d.radius);
    case ShapeType::Capsule:
    case ShapeType::Cylinder: return positive(d.radius) && positive(d.half_height);
    case ShapeType::ConvexHull:
        return d.points.size() >= 4 && std::all_of(d.points.begin(), d.points.end(), finite) && hull_has_volume(d.points);
    case ShapeType::Mesh:
        return d.vertices.size() >= 3 && !d.indices.empty() && d.indices.size() % 3 == 0 &&
               std::all_of(d.vertices.begin(), d.vertices.end(), finite) &&
               std::all_of(d.indices.begin(), d.indices.end(), [&](std::uint32_t i) { return i < d.vertices.size(); });
    case ShapeType::HeightField:
        return d.samples >= 4 && positive(d.cell_size) && d.heights.size() == std::size_t{d.samples} * d.samples &&
               std::all_of(d.heights.begin(), d.heights.end(), [](float h) { return std::isfinite(h); });
    case ShapeType::Compound:
        return !d.children.empty() && std::all_of(d.children.begin(), d.children.end(), [](const ShapeChild& c) {
            return finite(c.position) && valid_shape(c.shape);
        });
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
    case ShapeType::ConvexHull: {
        JPH::Array<JPH::Vec3> points;
        points.reserve(d.points.size());
        for (const Vec3& p : d.points) points.push_back(to_jph(p));
        result = JPH::ConvexHullShapeSettings(points).Create();
        break;
    }
    case ShapeType::Mesh: {
        JPH::VertexList vertices;
        vertices.reserve(d.vertices.size());
        for (const Vec3& v : d.vertices) vertices.push_back(JPH::Float3(v.x, v.y, v.z));
        JPH::IndexedTriangleList triangles;
        for (std::size_t i = 0; i + 2 < d.indices.size(); i += 3) triangles.push_back(JPH::IndexedTriangle(d.indices[i], d.indices[i + 1], d.indices[i + 2]));
        result = JPH::MeshShapeSettings(vertices, triangles).Create();
        break;
    }
    case ShapeType::HeightField: {
        const float half = 0.5f * static_cast<float>(d.samples - 1) * d.cell_size; // centred on the body
        result = JPH::HeightFieldShapeSettings(d.heights.data(), JPH::Vec3(-half, 0, -half), JPH::Vec3(d.cell_size, 1, d.cell_size), d.samples).Create();
        break;
    }
    case ShapeType::Compound: {
        JPH::StaticCompoundShapeSettings compound;
        for (const ShapeChild& c : d.children) {
            JPH::Ref<JPH::Shape> child = make_shape(c.shape);
            if (!child) return nullptr;
            compound.AddShape(to_jph(c.position), to_jph(c.rotation), child);
        }
        result = compound.Create();
        break;
    }
    }
    return result.IsValid() ? result.Get() : nullptr;
}

/// Query filter: skips one optional body, and sensors unless asked for.
class RayBodyFilter final : public JPH::BodyFilter {
public:
    RayBodyFilter(JPH::BodyID ignore, bool sensors) : ignore_(ignore), sensors_(sensors) {}
    bool ShouldCollide(const JPH::BodyID& id) const override { return id != ignore_; }
    bool ShouldCollideLocked(const JPH::Body& body) const override { return sensors_ || !body.IsSensor(); }

private:
    JPH::BodyID ignore_;
    bool sensors_;
};

constexpr int max_physics_jobs = 2048;
constexpr int max_physics_barriers = 8;

} // namespace

struct PhysicsWorld::Impl {
    struct BodyRecord {
        JPH::BodyID id;
        std::uint64_t user_data = 0;
        std::uint16_t surface = 0;
        bool sensor = false;
    };
    struct CharacterRecord {
        JPH::Ref<JPH::CharacterVirtual> character;
    };
    struct JointRecord {
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        BodyHandle a, b;
        std::uint64_t pair_key = 0; ///< Non-zero: this joint disables collisions between its bodies.
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
            e.surface_a = owner_.surface_of(e.a);
            e.surface_b = owner_.surface_of(e.b);
            e.sensor = a.IsSensor() || b.IsSensor();
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
            e.surface_a = owner_.surface_of(e.a);
            e.surface_b = owner_.surface_of(e.b);
            e.sensor = owner_.sensor_of(e.a) || owner_.sensor_of(e.b);
            std::scoped_lock lock(mutex_);
            owner_.contacts.push_back(e);
        }

    private:
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
    std::vector<IndexEntry> handle_by_index; // read during steps, written only between them
    HandlePool<CharacterRecord, CharacterTag> characters;
    HandlePool<JointRecord, JointTag> joints;
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
    std::uint16_t surface_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r ? r->surface : 0;
    }
    bool sensor_of(BodyHandle h) const {
        const BodyRecord* r = bodies.get(h);
        return r && r->sensor;
    }
    JPH::BodyInterface& bi() { return system->GetBodyInterface(); }
    const JPH::BodyInterface& bi() const { return system->GetBodyInterface(); }
    const BodyRecord* record(BodyHandle h) const { return bodies.get(h); }
};

PhysicsWorld::PhysicsWorld(const PhysicsDesc& desc) : impl_(std::make_unique<Impl>()) {
    acquire_jolt();
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
    m.joints.for_each([&](JointHandle, Impl::JointRecord& j) { m.system->RemoveConstraint(j.constraint); });
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
        m.error("create_body: invalid shape (check dimensions, hull points, mesh indices, height field size)");
        return {};
    }
    if (desc.motion == MotionType::Dynamic && needs_static(desc.shape)) {
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
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(to_jph(desc.position)), to_jph(desc.rotation), motion,
                                       layers::object_layer(desc.layer, !is_static));
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
    {   // every body can be told not to collide with the bodies it is joined to: its group is its own index
        JPH::BodyLockWrite lock(m.system->GetBodyLockInterface(), id);
        if (lock.Succeeded()) {
            lock.GetBody().SetCollisionGroup(JPH::CollisionGroup(m.pair_filter, 1, id.GetIndex()));
        }
    }
    const BodyHandle handle = m.bodies.create({id, desc.user_data, desc.surface, desc.sensor});
    m.handle_by_index[id.GetIndex()] = {handle, id};
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

void PhysicsWorld::add_force_at(BodyHandle body, Vec3 force, Vec3 point) {
    const auto* r = impl_->record(body);
    if (r) impl_->bi().AddForce(r->id, to_jph(force), JPH::RVec3(to_jph(point)));
}

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
    if (needs_static(shape_desc)) return std::nullopt; // static geometry cannot be swept
    const JPH::Ref<JPH::Shape> shape = make_shape(shape_desc);
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
    if (needs_static(shape_desc)) return out;
    const JPH::Ref<JPH::Shape> shape = make_shape(shape_desc);
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
    if (!finite(d.anchor) || !finite(d.anchor_b)) return fail("the anchor must be finite");
    const bool axis_needed = d.type == JointType::Hinge || d.type == JointType::Slider;
    if (axis_needed && (!finite(d.axis) || length(d.axis) < 1e-6f)) return fail("hinge and slider need a non-zero axis");

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
    if (!body_b->IsDynamic() && !(body_a->IsDynamic())) return fail("at least one body must be dynamic");

    const JPH::RVec3 anchor(to_jph(d.anchor));
    const JPH::Vec3 axis = to_jph(normalize(d.axis));
    const JPH::Vec3 normal = axis.GetNormalizedPerpendicular();
    const JPH::SpringSettings spring(JPH::ESpringMode::FrequencyAndDamping, d.spring_frequency, d.spring_damping);
    JPH::Ref<JPH::TwoBodyConstraint> constraint;
    switch (d.type) {
    case JointType::Fixed: {
        JPH::FixedConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mAutoDetectPoint = true;
        constraint = s.Create(*body_a, *body_b);
        break;
    }
    case JointType::Point: {
        JPH::PointConstraintSettings s;
        s.mSpace = JPH::EConstraintSpace::WorldSpace;
        s.mPoint1 = s.mPoint2 = anchor;
        constraint = s.Create(*body_a, *body_b);
        break;
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
        constraint = s.Create(*body_a, *body_b);
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
        if (d.limited) {
            s.mLimitsMin = d.min_limit;
            s.mLimitsMax = d.max_limit;
            s.mLimitsSpringSettings = spring;
        }
        constraint = s.Create(*body_a, *body_b);
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
            s.mMinDistance = s.mMaxDistance = -1.0f; // the distance they have now
        }
        s.mLimitsSpringSettings = spring;
        constraint = s.Create(*body_a, *body_b);
        break;
    }
    }
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

void PhysicsWorld::shift_origin(Vec3 offset) {
    const JPH::RVec3 delta(to_jph(offset));
    JPH::BodyInterface& bi = impl_->bi();
    impl_->bodies.for_each([&](BodyHandle, Impl::BodyRecord& r) {
        bi.SetPosition(r.id, bi.GetPosition(r.id) - delta, JPH::EActivation::DontActivate);
    });
    impl_->characters.for_each([&](CharacterHandle, Impl::CharacterRecord& r) {
        r.character->SetPosition(r.character->GetPosition() - delta);
    });
    impl_->system->OptimizeBroadPhase();
}

void PhysicsWorld::set_character_position(CharacterHandle handle, Vec3 position) {
    auto* r = impl_->characters.get(handle);
    if (r) r->character->SetPosition(JPH::RVec3(to_jph(position)));
}

} // namespace astral
