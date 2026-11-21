#include "Detail/WorldImpl.hpp"

#include "Astral/Render/SdfMath.hpp"

#include <algorithm>

namespace Astral::Render {

const std::vector<SdfShapeDesc>& World::Impl::SortedShapes() const {
    if (sortedVersion == sdfVersion) return sortedShapes;
    std::vector<const SdfShapeData*> list;
    sdfShapes.ForEach([&](SdfShapeHandle, const SdfShapeData& s) { list.push_back(&s); });
    std::sort(list.begin(), list.end(), [](const SdfShapeData* a, const SdfShapeData* b) {
        return a->desc.order != b->desc.order ? a->desc.order < b->desc.order : a->sequence < b->sequence;
    });
    sortedShapes.clear();
    for (const SdfShapeData* s : list) sortedShapes.push_back(s->desc);
    sortedVersion = sdfVersion;
    return sortedShapes;
}

World::World(std::unique_ptr<Impl> impl) : m_Impl(std::move(impl)) {}
World::~World() = default;

// ----------------------------------------------------------------------------- instances

InstanceHandle World::CreateInstance(MeshHandle mesh, MaterialHandle material, const glm::mat4& transform, uint32_t objectId) {
    InstanceData data;
    data.mesh = mesh;
    data.material = material;
    data.transform = transform;
    data.objectId = objectId;
    return m_Impl->instances.Create(data);
}

void World::SetInstanceTransform(InstanceHandle h, const glm::mat4& t) {
    if (auto* i = m_Impl->instances.TryGet(h)) i->transform = t;
}
void World::SetInstanceMaterial(InstanceHandle h, MaterialHandle m) {
    if (auto* i = m_Impl->instances.TryGet(h)) i->material = m;
}
void World::SetInstanceVisible(InstanceHandle h, bool visible) {
    if (auto* i = m_Impl->instances.TryGet(h)) i->visible = visible;
}
void World::SetInstanceCastsShadows(InstanceHandle h, bool cast) {
    if (auto* i = m_Impl->instances.TryGet(h)) i->castsShadows = cast;
}
void World::DestroyInstance(InstanceHandle h) { m_Impl->instances.Destroy(h); }

// ----------------------------------------------------------------------------- SDF shapes

SdfShapeHandle World::CreateSdfShape(const SdfShapeDesc& desc) {
    ++m_Impl->sdfVersion;
    return m_Impl->sdfShapes.Create(SdfShapeData{desc, m_Impl->nextSequence++});
}

void World::UpdateSdfShape(SdfShapeHandle h, const SdfShapeDesc& desc) {
    if (auto* s = m_Impl->sdfShapes.TryGet(h)) {
        s->desc = desc;
        ++m_Impl->sdfVersion;
    }
}

void World::DestroySdfShape(SdfShapeHandle h) {
    if (m_Impl->sdfShapes.Destroy(h)) ++m_Impl->sdfVersion;
}

const SdfShapeDesc* World::GetSdfShape(SdfShapeHandle h) const {
    const auto* s = m_Impl->sdfShapes.TryGet(h);
    return s ? &s->desc : nullptr;
}

float World::SdfDistance(const glm::vec3& point) const { return Sdf::Evaluate(m_Impl->SortedShapes(), point); }

std::optional<SdfRayHit> World::RaycastSdf(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const {
    const auto& shapes = m_Impl->SortedShapes();
    if (shapes.empty()) return std::nullopt;
    glm::vec3 dir = glm::normalize(direction);
    float t = 0.0f;
    for (int i = 0; i < 256 && t < maxDistance; ++i) {
        glm::vec3 p = origin + dir * t;
        float d = Sdf::Evaluate(shapes, p);
        if (d < 1e-3f) {
            SdfRayHit hit;
            hit.distance = t;
            hit.position = p;
            hit.normal = Sdf::Normal(shapes, p);
            int idx = Sdf::DominantShape(shapes, p);
            hit.objectId = idx >= 0 ? shapes[idx].objectId : 0;
            return hit;
        }
        t += std::max(d, 1e-3f);
    }
    return std::nullopt;
}

glm::vec3 World::SdfVolumeMin() const { return m_Impl->volume.origin; }
glm::vec3 World::SdfVolumeMax() const {
    const auto& v = m_Impl->volume;
    return v.origin + glm::vec3(v.cells) * (v.voxelSize * 7.0f);
}

// ----------------------------------------------------------------------------- lights

LightHandle World::CreateLight(const LightDesc& desc) {
    if (desc.type == LightType::Directional) ++m_Impl->environmentVersion;
    return m_Impl->lights.Create(desc);
}

void World::UpdateLight(LightHandle h, const LightDesc& desc) {
    if (auto* l = m_Impl->lights.TryGet(h)) {
        if (l->type == LightType::Directional || desc.type == LightType::Directional) ++m_Impl->environmentVersion;
        *l = desc;
    }
}

void World::DestroyLight(LightHandle h) {
    if (auto* l = m_Impl->lights.TryGet(h); l && l->type == LightType::Directional) ++m_Impl->environmentVersion;
    m_Impl->lights.Destroy(h);
}

void World::SetEnvironment(const EnvironmentDesc& env) {
    m_Impl->environment = env;
    ++m_Impl->environmentVersion;
}

const EnvironmentDesc& World::GetEnvironment() const { return m_Impl->environment; }

uint32_t World::InstanceCount() const { return m_Impl->instances.AliveCount(); }
uint32_t World::SdfShapeCount() const { return m_Impl->sdfShapes.AliveCount(); }
uint32_t World::LightCount() const { return m_Impl->lights.AliveCount(); }

} // namespace Astral::Render
