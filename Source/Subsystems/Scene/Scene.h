#pragma once

#include <entt/entt.hpp>
#include "../../Core/UUID.h"
#include "../../ECS/Components.h"

namespace AstralEngine {

    class Entity;

    class Scene {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        void OnUpdate(float ts);
        void OnRender(); // Will be used later for scene-based rendering

        entt::registry& Reg() { return m_Registry; }

        // Hierarchy helpers
        void ParentEntity(Entity child, Entity parent);
        void UnparentEntity(Entity child);

    private:
        entt::registry m_Registry;
        
        void UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform);

        friend class Entity;
    };

}
