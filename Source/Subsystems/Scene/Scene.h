#pragma once

#include <entt/entt.hpp>
#include "../../Core/UUID.h"
#include "../../ECS/Components.h"
#include "../../Core/ISubsystem.h" // Added

namespace AstralEngine {

    class Entity;

    class Scene : public ISubsystem { // Inherit from ISubsystem
    public:
        Scene();
        ~Scene() override;

        // ISubsystem interface implementation
        void OnInitialize(Engine* owner) override;
        void OnUpdate(float ts) override;
        void OnShutdown() override;

        const char* GetName() const override { return "SceneSubsystem"; }
        UpdateStage GetUpdateStage() const override { return UpdateStage::Update; }

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        void OnRender(); // Keep for future use

        entt::registry& Reg() { return m_Registry; }

        // Hierarchy helpers
        void ParentEntity(Entity child, Entity parent);
        void UnparentEntity(Entity child);

    private:
        entt::registry m_Registry;
        Engine* m_owner = nullptr;
        
        void UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform);

        friend class Entity;
    };

}
