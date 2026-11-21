#pragma once

#include "Astral/Engine/Scene/Components.hpp"
#include "Astral/Engine/Ecs/Registry.hpp"
#include "Astral/Render/RenderTypes.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Astral {

// A scene: entities with components, plus scene-wide settings (environment).
class Scene {
public:
    explicit Scene(std::string name = "Untitled");

    Registry& GetRegistry() { return m_Registry; }
    const Registry& GetRegistry() const { return m_Registry; }

    // Creates an entity with Id, Name and Transform components.
    Entity CreateEntity(std::string_view name = "Entity");
    Entity CreateEntityWithId(uint64_t id, std::string_view name);
    // Destroys the entity and all of its children.
    void DestroyEntity(Entity entity);
    bool IsValid(Entity entity) const { return m_Registry.Valid(entity); }

    Entity FindByName(std::string_view name) const;
    Entity FindById(uint64_t id) const;
    std::string_view GetName(Entity entity) const;

    // Hierarchy
    Entity GetParent(Entity entity) const;
    std::vector<Entity> GetChildren(Entity entity) const;
    std::vector<Entity> GetRoots() const;
    // Re-parents `child`; with keepWorldTransform the child does not move.
    void SetParent(Entity child, Entity parent, bool keepWorldTransform = true);

    // Computes WorldTransform for every entity with a Transform (parents first).
    void UpdateWorldTransforms();
    glm::mat4 GetWorldMatrix(Entity entity) const;

    // Mapping used by picking: renderer object ids are entity slot + 1.
    static uint32_t ObjectIdOf(Entity entity) { return entity.index + 1; }
    Entity EntityFromObjectId(uint32_t objectId) const;

    // The first entity with a primary CameraComponent, or null.
    Entity GetPrimaryCamera() const;

    // Deep copy (through the serializer), used by the editor's play mode.
    std::unique_ptr<Scene> Clone() const;

    std::string name;
    Render::EnvironmentDesc environment;

private:
    mutable Registry m_Registry;
};

// Builds a render camera from an entity with Transform + CameraComponent.
Render::Camera MakeRenderCamera(const Scene& scene, Entity cameraEntity);

} // namespace Astral
