#include "Runtime/RenderSync.hpp"

#include <cstring>

namespace Astral {

namespace {

bool SameMaterial(const MaterialParams& a, const MaterialParams& b) {
    return a.baseColor == b.baseColor && a.metallic == b.metallic && a.roughness == b.roughness && a.emissive == b.emissive;
}

bool SameShape(const Render::SdfShapeDesc& a, const Render::SdfShapeDesc& b) {
    return a.primitive == b.primitive && a.operation == b.operation && a.position == b.position && a.rotation == b.rotation &&
           a.size == b.size && a.blend == b.blend && a.order == b.order && a.objectId == b.objectId &&
           a.material.albedo == b.material.albedo && a.material.roughness == b.material.roughness &&
           a.material.metallic == b.material.metallic && a.material.emissive == b.material.emissive;
}

bool SameLight(const Render::LightDesc& a, const Render::LightDesc& b) {
    return a.type == b.type && a.color == b.color && a.intensity == b.intensity && a.position == b.position &&
           a.direction == b.direction && a.range == b.range && a.castShadows == b.castShadows;
}

Render::MaterialDesc ToMaterial(const MaterialParams& m) {
    Render::MaterialDesc d;
    d.baseColor = m.baseColor;
    d.metallic = m.metallic;
    d.roughness = m.roughness;
    d.emissive = m.emissive;
    return d;
}

} // namespace

RenderSync::RenderSync(Render::Renderer& renderer, AssetManager& assets) : m_Renderer(renderer), m_Assets(assets) {}
RenderSync::~RenderSync() { Clear(); }

void RenderSync::Clear() {
    Render::World& world = m_Renderer.GetWorld();
    for (auto& [key, e] : m_Meshes) DestroyMesh(e);
    for (auto& [key, e] : m_Sdfs) world.DestroySdfShape(e.handle);
    for (auto& [key, e] : m_Lights) world.DestroyLight(e.handle);
    m_Meshes.clear();
    m_Sdfs.clear();
    m_Lights.clear();
    m_EnvironmentSet = false;
}

void RenderSync::DestroyMesh(MeshEntry& e) {
    for (auto h : e.instances) m_Renderer.GetWorld().DestroyInstance(h);
    e.instances.clear();
    if (e.hasOverride) m_Renderer.DestroyMaterial(e.overrideMaterial);
    e.hasOverride = false;
}

void RenderSync::Sync(Scene& scene) {
    SyncMeshes(scene);
    SyncSdfShapes(scene);
    SyncLights(scene);
    const auto& env = scene.environment;
    if (!m_EnvironmentSet || std::memcmp(&env, &m_LastEnvironment, sizeof(env)) != 0) {
        m_Renderer.GetWorld().SetEnvironment(env);
        m_LastEnvironment = env;
        m_EnvironmentSet = true;
    }
}

void RenderSync::SyncMeshes(Scene& scene) {
    Render::World& world = m_Renderer.GetWorld();
    Registry& reg = scene.GetRegistry();
    for (auto& [key, e] : m_Meshes) e.seen = false;

    reg.Each<MeshRenderer, WorldTransform>([&](Entity entity, MeshRenderer& mr, WorldTransform& wt) {
        MeshEntry& e = m_Meshes[entity.Raw()];
        e.seen = true;
        const Model* model = m_Assets.GetModel(mr.mesh);

        bool rebuild = e.mesh != mr.mesh || (model && e.instances.size() != model->primitives.size()) ||
                       e.hasOverride != !mr.useAssetMaterials;
        if (rebuild) {
            DestroyMesh(e);
            e.mesh = mr.mesh;
            if (!model) return;
            if (!mr.useAssetMaterials) {
                e.overrideMaterial = m_Renderer.CreateMaterial(ToMaterial(mr.material));
                e.lastMaterial = mr.material;
                e.hasOverride = true;
            }
            for (const auto& prim : model->primitives) {
                auto material = e.hasOverride ? e.overrideMaterial : prim.material;
                e.instances.push_back(world.CreateInstance(prim.mesh, material, wt.matrix * prim.localTransform,
                                                           Scene::ObjectIdOf(entity)));
            }
        }
        if (!model) return;
        if (e.hasOverride && !SameMaterial(e.lastMaterial, mr.material)) {
            m_Renderer.UpdateMaterial(e.overrideMaterial, ToMaterial(mr.material));
            e.lastMaterial = mr.material;
        }
        for (size_t i = 0; i < e.instances.size(); ++i) {
            world.SetInstanceTransform(e.instances[i], wt.matrix * model->primitives[i].localTransform);
            world.SetInstanceCastsShadows(e.instances[i], mr.castShadows);
        }
    });

    for (auto it = m_Meshes.begin(); it != m_Meshes.end();) {
        if (!it->second.seen) {
            DestroyMesh(it->second);
            it = m_Meshes.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderSync::SyncSdfShapes(Scene& scene) {
    Render::World& world = m_Renderer.GetWorld();
    Registry& reg = scene.GetRegistry();
    for (auto& [key, e] : m_Sdfs) e.seen = false;

    reg.Each<SdfShapeComponent, WorldTransform>([&](Entity entity, SdfShapeComponent& s, WorldTransform& wt) {
        const Transform placed = Transform::FromMatrix(wt.matrix);

        Render::SdfShapeDesc d;
        d.primitive = s.primitive;
        d.operation = s.operation;
        d.position = placed.position;
        d.rotation = placed.rotation;
        d.size = s.size * glm::vec4(glm::abs(placed.scale), 1.0f);
        d.blend = s.blend;
        d.order = s.order;
        d.material = s.material;
        d.objectId = Scene::ObjectIdOf(entity);

        auto [it, inserted] = m_Sdfs.try_emplace(entity.Raw());
        SdfEntry& e = it->second;
        e.seen = true;
        if (inserted || !world.GetSdfShape(e.handle)) {
            e.handle = world.CreateSdfShape(d);
            e.last = d;
        } else if (!SameShape(e.last, d)) {
            world.UpdateSdfShape(e.handle, d);
            e.last = d;
        }
    });

    for (auto it = m_Sdfs.begin(); it != m_Sdfs.end();) {
        if (!it->second.seen) {
            world.DestroySdfShape(it->second.handle);
            it = m_Sdfs.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderSync::SyncLights(Scene& scene) {
    Render::World& world = m_Renderer.GetWorld();
    Registry& reg = scene.GetRegistry();
    for (auto& [key, e] : m_Lights) e.seen = false;

    reg.Each<LightComponent, WorldTransform>([&](Entity entity, LightComponent& l, WorldTransform& wt) {
        Render::LightDesc d;
        d.type = l.type;
        d.color = l.color;
        d.intensity = l.intensity;
        d.range = l.range;
        d.castShadows = l.castShadows;
        d.position = glm::vec3(wt.matrix[3]);
        d.direction = glm::normalize(-glm::vec3(wt.matrix[2])); // forward = -Z

        auto [it, inserted] = m_Lights.try_emplace(entity.Raw());
        it->second.seen = true;
        LightEntry& e = it->second;
        if (inserted) {
            e.handle = world.CreateLight(d);
            e.last = d;
        } else if (!SameLight(e.last, d)) { // a changed sun re-bakes image-based lighting, so only push changes
            world.UpdateLight(e.handle, d);
            e.last = d;
        }
    });

    for (auto it = m_Lights.begin(); it != m_Lights.end();) {
        if (!it->second.seen) {
            world.DestroyLight(it->second.handle);
            it = m_Lights.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Astral
