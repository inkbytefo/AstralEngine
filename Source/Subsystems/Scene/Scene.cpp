#include "Scene.h"
#include "Entity.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

    Scene::Scene() {
    }

    Scene::~Scene() {
    }

    Entity Scene::CreateEntity(const std::string& name) {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<WorldTransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.tag = name.empty() ? "Entity" : name;
        entity.AddComponent<NameComponent>(tag.tag); // Keep NameComponent synced for now
        entity.AddComponent<RelationshipComponent>();
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        if (!entity) return;

        // Destroy children first
        auto& relation = entity.GetComponent<RelationshipComponent>();
        for (auto childHandle : relation.Children) {
            Entity child = { childHandle, this };
            DestroyEntity(child);
        }

        // Remove from parent
        if (relation.Parent != entt::null) {
            Entity parent = { relation.Parent, this };
            auto& parentRelation = parent.GetComponent<RelationshipComponent>();
            auto it = std::find(parentRelation.Children.begin(), parentRelation.Children.end(), (entt::entity)entity);
            if (it != parentRelation.Children.end()) {
                parentRelation.Children.erase(it);
            }
        }

        m_Registry.destroy(entity);
    }

    void Scene::ParentEntity(Entity child, Entity parent) {
        if (!child || !parent) return;
        if (child == parent) return;

        UnparentEntity(child);

        auto& childRelation = child.GetComponent<RelationshipComponent>();
        childRelation.Parent = parent;

        auto& parentRelation = parent.GetComponent<RelationshipComponent>();
        parentRelation.Children.push_back(child);
    }

    void Scene::UnparentEntity(Entity child) {
        if (!child) return;

        auto& childRelation = child.GetComponent<RelationshipComponent>();
        if (childRelation.Parent == entt::null) return;

        Entity parent = { childRelation.Parent, this };
        auto& parentRelation = parent.GetComponent<RelationshipComponent>();
        auto it = std::find(parentRelation.Children.begin(), parentRelation.Children.end(), (entt::entity)child);
        if (it != parentRelation.Children.end()) {
            parentRelation.Children.erase(it);
        }

        childRelation.Parent = entt::null;
    }

    void Scene::OnUpdate(float ts) {
        // Update transforms
        auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
        for (auto entity : view) {
            const auto& relation = view.get<RelationshipComponent>(entity);
            if (relation.Parent == entt::null) {
                UpdateEntityTransform(entity, glm::mat4(1.0f));
            }
        }
    }

    void Scene::UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform) {
        auto& transform = m_Registry.get<TransformComponent>(entity);
        glm::mat4 currentTransform = parentTransform * transform.GetLocalMatrix();

        if (m_Registry.all_of<WorldTransformComponent>(entity)) {
            m_Registry.get<WorldTransformComponent>(entity).Transform = currentTransform;
        }
        else {
             m_Registry.emplace<WorldTransformComponent>(entity, currentTransform);
        }

        if (m_Registry.all_of<RelationshipComponent>(entity)) {
            auto& children = m_Registry.get<RelationshipComponent>(entity).Children;
            for (auto child : children) {
                if (m_Registry.valid(child)) {
                    UpdateEntityTransform(child, currentTransform);
                }
            }
        }
    }

    void Scene::OnRender() {
        // Render system loop
    }

}
