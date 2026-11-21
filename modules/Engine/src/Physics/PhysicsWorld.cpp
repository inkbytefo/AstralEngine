#include "Astral/Engine/Physics/PhysicsWorld.hpp"
#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Engine/Physics/SdfMeshing.hpp"

#include "Astral/Core/Log.hpp"
#include "Astral/Render/SdfMath.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>


#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Astral {

namespace {

// ---- Jolt global state (shared by all PhysicsWorld instances) ----
std::mutex g_JoltMutex;
int g_JoltUsers = 0;

void AcquireJolt() {
    std::lock_guard lock(g_JoltMutex);
    if (g_JoltUsers++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

void ReleaseJolt() {
    std::lock_guard lock(g_JoltMutex);
    if (--g_JoltUsers == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

namespace Layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
} // namespace Layers

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override { return JPH::BroadPhaseLayer(JPH::uint8(layer)); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer.GetValue() == 0 ? "static" : "moving";
    }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        return layer == Layers::kMoving || bp.GetValue() == Layers::kMoving;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == Layers::kMoving || b == Layers::kMoving;
    }
};

JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
glm::vec3 FromJolt(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
glm::quat FromJolt(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

int64_t ChunkKey(const glm::ivec3& c) {
    return (int64_t(c.x & 0x1FFFFF) << 42) | (int64_t(c.y & 0x1FFFFF) << 21) | int64_t(c.z & 0x1FFFFF);
}

uint64_t HashShapes(const std::vector<const Render::SdfShapeDesc*>& shapes) {
    uint64_t h = 1469598103934665603ull;
    for (const auto* s : shapes) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(s);
        for (size_t i = 0; i < offsetof(Render::SdfShapeDesc, material); ++i) h = (h ^ bytes[i]) * 1099511628211ull;
    }
    return h;
}

} // namespace

struct PhysicsWorld::Impl {
    PhysicsSettings settings;
    BroadPhaseLayers broadPhase;
    ObjectVsBroadPhase objectVsBroadPhase;
    ObjectPairs objectPairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystemThreadPool> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    bool running = false;

    struct Chunk {
        JPH::BodyID body;
        uint64_t hash = 0;
        uint32_t triangles = 0;
    };
    std::unordered_map<int64_t, Chunk> chunks;
    std::unordered_map<uint32_t, Entity> owners; // body id -> entity, to notice destroyed entities

    JPH::BodyInterface& Bodies() { return system->GetBodyInterface(); }
    void CreateBody(Scene& scene, Entity e, const RigidBody& rb, const Collider& col);
    void SyncBodies(Scene& scene);

    JPH::RefConst<JPH::Shape> MakeShape(const Collider& c, const glm::vec3& scale) {
        const glm::vec3 s = glm::abs(scale);
        JPH::RefConst<JPH::Shape> shape;
        switch (c.shape) {
            case ColliderShape::Box:
                shape = new JPH::BoxShape(ToJolt(glm::max(c.size * s, glm::vec3(0.01f))), 0.0f);
                break;
            case ColliderShape::Sphere:
                shape = new JPH::SphereShape(std::max(c.size.x * std::max(s.x, std::max(s.y, s.z)), 0.01f));
                break;
            case ColliderShape::Capsule:
                shape = new JPH::CapsuleShape(std::max(c.size.y * s.y, 0.01f), std::max(c.size.x * std::max(s.x, s.z), 0.01f));
                break;
        }
        if (glm::length(c.offset) > 1e-5f)
            shape = new JPH::RotatedTranslatedShape(ToJolt(c.offset * s), JPH::Quat::sIdentity(), shape);
        return shape;
    }

    void UpdateSdfCollision(Scene& scene, bool force);
};

PhysicsWorld::PhysicsWorld(const PhysicsSettings& settings) : m_Impl(std::make_unique<Impl>()) {
    AcquireJolt();
    m_Impl->settings = settings;
    m_Impl->temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    int threads = std::max(1, int(std::thread::hardware_concurrency()) - 1);
    m_Impl->jobs = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);
}

PhysicsWorld::~PhysicsWorld() {
    m_Impl->system.reset();
    m_Impl->jobs.reset();
    m_Impl->temp.reset();
    ReleaseJolt();
}

bool PhysicsWorld::IsRunning() const { return m_Impl->running; }

void PhysicsWorld::Impl::CreateBody(Scene& scene, Entity e, const RigidBody& rb, const Collider& col) {
    Registry& reg = scene.GetRegistry();
    Transform t = Transform::FromMatrix(scene.GetWorldMatrix(e));
    JPH::EMotionType motion = rb.type == BodyType::Static    ? JPH::EMotionType::Static
                            : rb.type == BodyType::Kinematic ? JPH::EMotionType::Kinematic
                                                             : JPH::EMotionType::Dynamic;
    JPH::BodyCreationSettings bcs(MakeShape(col, t.scale), JPH::RVec3(t.position.x, t.position.y, t.position.z),
                                  ToJolt(t.rotation), motion,
                                  rb.type == BodyType::Static ? Layers::kStatic : Layers::kMoving);
    bcs.mFriction = rb.friction;
    bcs.mRestitution = rb.restitution;
    bcs.mLinearDamping = rb.linearDamping;
    bcs.mAngularDamping = rb.angularDamping;
    bcs.mUserData = Entity(e).Raw();
    if (rb.type == BodyType::Dynamic) {
        bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bcs.mMassPropertiesOverride.mMass = std::max(rb.mass, 0.001f);
        bcs.mLinearVelocity = ToJolt(rb.initialVelocity);
    }
    JPH::BodyID id = Bodies().CreateAndAddBody(
        bcs, rb.type == BodyType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    reg.Add<PhysicsBodyId>(e, PhysicsBodyId{id.GetIndexAndSequenceNumber()});
    owners[id.GetIndexAndSequenceNumber()] = e;
}

// Bodies follow the ECS: entities that gained RigidBody + Collider get a body, bodies whose
// entity was destroyed (or lost either component) are removed. Runs at the start of every step.
void PhysicsWorld::Impl::SyncBodies(Scene& scene) {
    Registry& reg = scene.GetRegistry();
    std::vector<uint32_t> stale;
    for (const auto& [id, e] : owners)
        if (!reg.Valid(e) || !reg.Has<RigidBody>(e) || !reg.Has<Collider>(e)) stale.push_back(id);
    for (uint32_t id : stale) {
        Entity e = owners[id];
        Bodies().RemoveBody(JPH::BodyID(id));
        Bodies().DestroyBody(JPH::BodyID(id));
        owners.erase(id);
        if (reg.Valid(e)) reg.Remove<PhysicsBodyId>(e);
    }

    std::vector<Entity> added;
    reg.Each<RigidBody, Collider>([&](Entity e, RigidBody&, Collider&) {
        if (!reg.Has<PhysicsBodyId>(e)) added.push_back(e);
    });
    for (Entity e : added) CreateBody(scene, e, reg.Get<RigidBody>(e), reg.Get<Collider>(e));
}

void PhysicsWorld::Start(Scene& scene) {
    if (m_Impl->running) Stop(scene);
    auto& impl = *m_Impl;
    impl.system = std::make_unique<JPH::PhysicsSystem>();
    impl.system->Init(65536, 0, 65536, 16384, impl.broadPhase, impl.objectVsBroadPhase, impl.objectPairs);
    impl.system->SetGravity(ToJolt(impl.settings.gravity));
    impl.running = true;

    scene.UpdateWorldTransforms();
    impl.SyncBodies(scene);

    impl.chunks.clear();
    impl.UpdateSdfCollision(scene, true);
    impl.system->OptimizeBroadPhase();
}

void PhysicsWorld::Stop(Scene& scene) {
    auto& impl = *m_Impl;
    if (!impl.running) return;
    Registry& reg = scene.GetRegistry();
    std::vector<Entity> withBodies;
    reg.Each<PhysicsBodyId>([&](Entity e, PhysicsBodyId&) { withBodies.push_back(e); });
    for (Entity e : withBodies) reg.Remove<PhysicsBodyId>(e);
    impl.chunks.clear();
    impl.owners.clear();
    impl.system.reset(); // destroys every body
    impl.running = false;
}

void PhysicsWorld::Impl::UpdateSdfCollision(Scene& scene, bool force) {
    // Gather collision shapes in evaluation order.
    struct Item { int32_t order; uint32_t index; Render::SdfShapeDesc desc; };
    std::vector<Item> items;
    Registry& reg = scene.GetRegistry();
    reg.Each<SdfShapeComponent>([&](Entity e, SdfShapeComponent& s) {
        if (!s.collision) return;
        Transform t = Transform::FromMatrix(scene.GetWorldMatrix(e));
        Render::SdfShapeDesc d;
        d.primitive = s.primitive;
        d.operation = s.operation;
        d.position = t.position;
        d.rotation = t.rotation;
        d.size = s.size * glm::vec4(t.scale, 1.0f);
        d.blend = s.blend;
        d.order = s.order;
        items.push_back({s.order, e.index, d});
    });
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.order != b.order ? a.order < b.order : a.index < b.index;
    });

    // Chunks touched by any additive shape.
    const float chunk = settings.sdfChunkSize;
    std::unordered_map<int64_t, glm::ivec3> wanted;
    for (const Item& it : items) {
        if (it.desc.operation == Render::SdfOperation::Subtract || it.desc.operation == Render::SdfOperation::SmoothSubtract) continue;
        float r = Render::Sdf::BoundRadius(it.desc) + it.desc.blend;
        glm::ivec3 lo(glm::floor((it.desc.position - r) / chunk)), hi(glm::floor((it.desc.position + r) / chunk));
        for (int z = lo.z; z <= hi.z; ++z)
            for (int y = lo.y; y <= hi.y; ++y)
                for (int x = lo.x; x <= hi.x; ++x) wanted[ChunkKey({x, y, z})] = {x, y, z};
    }

    // Drop chunks that are no longer needed.
    for (auto it = chunks.begin(); it != chunks.end();) {
        if (!wanted.count(it->first)) {
            if (!it->second.body.IsInvalid()) {
                Bodies().RemoveBody(it->second.body);
                Bodies().DestroyBody(it->second.body);
            }
            it = chunks.erase(it);
        } else {
            ++it;
        }
    }

    int remeshBudget = force ? INT32_MAX : 2; // limit hitches while editing at runtime
    const float cell = settings.sdfCollisionCellSize;
    for (const auto& [key, coord] : wanted) {
        glm::vec3 lo = glm::vec3(coord) * chunk, hi = lo + chunk;
        std::vector<const Render::SdfShapeDesc*> relevant;
        for (const Item& it : items) {
            float r = Render::Sdf::BoundRadius(it.desc) + it.desc.blend + cell * 2.0f;
            glm::vec3 closest = glm::clamp(it.desc.position, lo, hi);
            if (it.desc.operation == Render::SdfOperation::Intersect || glm::distance(closest, it.desc.position) <= r)
                relevant.push_back(&it.desc);
        }
        uint64_t hash = HashShapes(relevant);
        Chunk& state = chunks[key];
        if (state.hash == hash && !force) continue;
        if (remeshBudget-- <= 0) break;
        state.hash = hash;

        if (!state.body.IsInvalid()) {
            Bodies().RemoveBody(state.body);
            Bodies().DestroyBody(state.body);
            state.body = JPH::BodyID();
        }
        std::vector<Render::SdfShapeDesc> shapes;
        for (const auto* s : relevant) shapes.push_back(*s);
        // One cell of overlap so neighbouring chunk meshes meet.
        TriangleMesh mesh = MeshSdfRegion(shapes, lo - cell, hi + cell, cell);
        state.triangles = uint32_t(mesh.indices.size() / 3);
        if (mesh.indices.empty()) continue;

        JPH::VertexList vertices;
        vertices.reserve(mesh.vertices.size());
        for (const auto& v : mesh.vertices) vertices.push_back(JPH::Float3(v.x, v.y, v.z));
        JPH::IndexedTriangleList triangles;
        triangles.reserve(mesh.indices.size() / 3);
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            triangles.push_back(JPH::IndexedTriangle(mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]));
        JPH::MeshShapeSettings meshSettings(std::move(vertices), std::move(triangles));
        auto result = meshSettings.Create();
        if (result.HasError()) {
            Log::Warn("physics", "SDF collision mesh failed: {}", result.GetError().c_str());
            continue;
        }
        JPH::BodyCreationSettings bcs(result.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                      Layers::kStatic);
        bcs.mUserData = 0;
        state.body = Bodies().CreateAndAddBody(bcs, JPH::EActivation::DontActivate);
    }
}

void PhysicsWorld::Step(Scene& scene, float dt) {
    auto& impl = *m_Impl;
    if (!impl.running || dt <= 0.0f) return;
    Registry& reg = scene.GetRegistry();
    JPH::BodyInterface& bodies = impl.Bodies();
    impl.SyncBodies(scene);

    reg.Each<RigidBody, PhysicsBodyId>([&](Entity e, RigidBody& rb, PhysicsBodyId& id) {
        if (rb.type != BodyType::Kinematic) return;
        Transform t = Transform::FromMatrix(scene.GetWorldMatrix(e));
        bodies.MoveKinematic(JPH::BodyID(id.id), JPH::RVec3(t.position.x, t.position.y, t.position.z), ToJolt(t.rotation), dt);
    });

    impl.UpdateSdfCollision(scene, false);
    impl.system->Update(dt, 1, impl.temp.get(), impl.jobs.get());

    reg.Each<RigidBody, PhysicsBodyId>([&](Entity e, RigidBody& rb, PhysicsBodyId& id) {
        if (rb.type != BodyType::Dynamic) return;
        JPH::BodyID body(id.id);
        JPH::RVec3 p;
        JPH::Quat q;
        bodies.GetPositionAndRotation(body, p, q);
        id.linearVelocity = FromJolt(bodies.GetLinearVelocity(body));
        auto* t = reg.TryGet<Transform>(e);
        if (!t) return;
        glm::mat4 world = glm::translate(glm::mat4(1.0f), glm::vec3(p.GetX(), p.GetY(), p.GetZ())) * glm::mat4_cast(FromJolt(q));
        if (Entity parent = scene.GetParent(e)) world = glm::inverse(scene.GetWorldMatrix(parent)) * world;
        Transform local = Transform::FromMatrix(world);
        t->position = local.position;
        t->rotation = local.rotation;
    });
}

std::optional<RaycastHit> PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const {
    auto& impl = *m_Impl;
    if (!impl.running) return std::nullopt;
    glm::vec3 dir = glm::normalize(direction) * maxDistance;
    JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(dir));
    JPH::RayCastResult result;
    if (!impl.system->GetNarrowPhaseQuery().CastRay(ray, result)) return std::nullopt;

    RaycastHit hit;
    hit.distance = result.mFraction * maxDistance;
    hit.position = origin + glm::normalize(direction) * hit.distance;
    JPH::BodyLockRead lock(impl.system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        hit.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
        if (body.GetUserData() != 0) hit.entity = Entity::FromRaw(body.GetUserData());
    }
    return hit;
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
    m_Impl->settings.gravity = gravity;
    if (m_Impl->system) m_Impl->system->SetGravity(ToJolt(gravity));
}

void PhysicsWorld::AddImpulse(Scene& scene, Entity e, const glm::vec3& impulse) {
    if (!m_Impl->running) return;
    if (auto* id = scene.GetRegistry().TryGet<PhysicsBodyId>(e)) m_Impl->Bodies().AddImpulse(JPH::BodyID(id->id), ToJolt(impulse));
}

void PhysicsWorld::SetLinearVelocity(Scene& scene, Entity e, const glm::vec3& v) {
    if (!m_Impl->running) return;
    if (auto* id = scene.GetRegistry().TryGet<PhysicsBodyId>(e)) m_Impl->Bodies().SetLinearVelocity(JPH::BodyID(id->id), ToJolt(v));
}

glm::vec3 PhysicsWorld::GetLinearVelocity(Scene& scene, Entity e) const {
    if (!m_Impl->running) return glm::vec3(0.0f);
    if (auto* id = scene.GetRegistry().TryGet<PhysicsBodyId>(e)) return FromJolt(m_Impl->Bodies().GetLinearVelocity(JPH::BodyID(id->id)));
    return glm::vec3(0.0f);
}

uint32_t PhysicsWorld::BodyCount() const { return m_Impl->system ? m_Impl->system->GetNumBodies() : 0; }

uint32_t PhysicsWorld::SdfCollisionTriangles() const {
    uint32_t n = 0;
    for (const auto& [key, c] : m_Impl->chunks) n += c.triangles;
    return n;
}

} // namespace Astral
