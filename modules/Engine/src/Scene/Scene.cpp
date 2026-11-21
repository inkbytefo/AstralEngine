#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Engine/Scene/SceneSerializer.hpp"

#include "Astral/Core/UUID.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <functional>
#include <unordered_map>

namespace Astral {

glm::mat4 Transform::Matrix() const {
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

Transform Transform::FromMatrix(const glm::mat4& m) {
    Transform t;
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    if (!glm::decompose(m, t.scale, t.rotation, t.position, skew, perspective)) return Transform{};
    t.rotation = glm::normalize(t.rotation);
    return t;
}

Scene::Scene(std::string n) : name(std::move(n)) {}

Entity Scene::CreateEntity(std::string_view entityName) { return CreateEntityWithId(static_cast<uint64_t>(UUID()), entityName); }

Entity Scene::CreateEntityWithId(uint64_t id, std::string_view entityName) {
    Entity e = m_Registry.Create();
    m_Registry.Add<IdComponent>(e, IdComponent{id});
    m_Registry.Add<NameComponent>(e, NameComponent{std::string(entityName)});
    m_Registry.Add<Transform>(e);
    return e;
}

void Scene::DestroyEntity(Entity entity) {
    if (!m_Registry.Valid(entity)) return;
    for (Entity child : GetChildren(entity)) DestroyEntity(child);
    m_Registry.Destroy(entity);
}

Entity Scene::FindByName(std::string_view n) const {
    Entity found;
    m_Registry.Each<NameComponent>([&](Entity e, NameComponent& c) {
        if (!found && c.name == n) found = e;
    });
    return found;
}

Entity Scene::FindById(uint64_t id) const {
    Entity found;
    m_Registry.Each<IdComponent>([&](Entity e, IdComponent& c) {
        if (!found && c.id == id) found = e;
    });
    return found;
}

std::string_view Scene::GetName(Entity entity) const {
    const auto* n = m_Registry.TryGet<NameComponent>(entity);
    return n ? std::string_view(n->name) : std::string_view();
}

Entity Scene::GetParent(Entity entity) const {
    const auto* p = m_Registry.TryGet<Parent>(entity);
    return p && m_Registry.Valid(p->entity) ? p->entity : Entity{};
}

std::vector<Entity> Scene::GetChildren(Entity entity) const {
    std::vector<Entity> children;
    m_Registry.Each<Parent>([&](Entity e, Parent& p) {
        if (p.entity == entity) children.push_back(e);
    });
    return children;
}

std::vector<Entity> Scene::GetRoots() const {
    std::vector<Entity> roots;
    for (Entity e : m_Registry.Entities())
        if (!GetParent(e)) roots.push_back(e);
    return roots;
}

void Scene::SetParent(Entity child, Entity parent, bool keepWorld) {
    if (!m_Registry.Valid(child) || child == parent) return;
    // Refuse cycles: parent must not be a descendant of child.
    for (Entity p = parent; p; p = GetParent(p))
        if (p == child) return;

    glm::mat4 world = GetWorldMatrix(child);
    if (parent && m_Registry.Valid(parent)) m_Registry.Add<Parent>(child, Parent{parent});
    else m_Registry.Remove<Parent>(child);

    if (keepWorld) {
        if (auto* t = m_Registry.TryGet<Transform>(child))
            *t = Transform::FromMatrix(parent ? glm::inverse(GetWorldMatrix(parent)) * world : world);
    }
}

glm::mat4 Scene::GetWorldMatrix(Entity entity) const {
    glm::mat4 m(1.0f);
    int depth = 0;
    for (Entity e = entity; e && depth < 256; e = GetParent(e), ++depth) {
        if (const auto* t = m_Registry.TryGet<Transform>(e)) m = t->Matrix() * m;
    }
    return m;
}

void Scene::UpdateWorldTransforms() {
    std::unordered_map<uint32_t, glm::mat4> cache;
    std::function<glm::mat4(Entity, int)> world = [&](Entity e, int depth) -> glm::mat4 {
        if (auto it = cache.find(e.index); it != cache.end()) return it->second;
        const auto* t = m_Registry.TryGet<Transform>(e);
        glm::mat4 local = t ? t->Matrix() : glm::mat4(1.0f);
        Entity parent = GetParent(e);
        glm::mat4 m = parent && depth < 256 ? world(parent, depth + 1) * local : local;
        cache[e.index] = m;
        return m;
    };
    m_Registry.Each<Transform>([&](Entity e, Transform&) {
        glm::mat4 m = world(e, 0);
        m_Registry.Add<WorldTransform>(e, WorldTransform{m});
    });
}

Entity Scene::EntityFromObjectId(uint32_t objectId) const {
    if (objectId == 0) return {};
    return m_Registry.FromIndex(objectId - 1);
}

Entity Scene::GetPrimaryCamera() const {
    Entity found;
    m_Registry.Each<CameraComponent>([&](Entity e, CameraComponent& c) {
        if (!found && c.primary) found = e;
    });
    return found;
}

std::unique_ptr<Scene> Scene::Clone() const {
    auto copy = std::make_unique<Scene>(name);
    std::string error;
    DeserializeScene(SerializeScene(*this), *copy, error);
    return copy;
}

Render::Camera MakeRenderCamera(const Scene& scene, Entity e) {
    Render::Camera cam;
    glm::mat4 world = scene.GetWorldMatrix(e);
    cam.position = glm::vec3(world[3]);
    cam.rotation = glm::normalize(glm::quat_cast(glm::mat3(glm::normalize(glm::vec3(world[0])),
                                                           glm::normalize(glm::vec3(world[1])),
                                                           glm::normalize(glm::vec3(world[2])))));
    if (const auto* c = scene.GetRegistry().TryGet<CameraComponent>(e)) {
        cam.verticalFov = c->verticalFov;
        cam.nearPlane = c->nearPlane;
        cam.farPlane = c->farPlane;
    }
    return cam;
}

} // namespace Astral
