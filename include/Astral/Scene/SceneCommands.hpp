#pragma once

#include "Astral/Core/CommandStack.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/Components.hpp"
#include <optional>
#include <string>

namespace Astral {

/// Yeni nesne olusturma komutu. Execute edildiginde nesneyi sahneye ekler, Undo'da yok eder.
class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(Scene& scene,
                        std::string name,
                        TransformComponent transform,
                        SDFComponent sdf,
                        Entity* outSelected = nullptr)
        : m_Scene(scene),
          m_Name(std::move(name)),
          m_Transform(transform),
          m_SDF(sdf),
          m_OutSelected(outSelected) {}

    void Execute() override {
        m_Entity = m_Scene.CreateEntity();
        m_Entity.AddComponent<TagComponent>(m_Name);
        m_Entity.AddComponent<TransformComponent>(m_Transform);
        m_Entity.AddComponent<SDFComponent>(m_SDF);
        if (m_OutSelected) {
            *m_OutSelected = m_Entity;
        }
    }

    void Undo() override {
        if (m_Entity.IsValid()) {
            if (m_OutSelected && *m_OutSelected == m_Entity) {
                *m_OutSelected = Entity();
            }
            m_Scene.DestroyEntity(m_Entity);
            m_Entity = Entity();
        }
    }

    [[nodiscard]] std::string GetName() const override {
        return "Nesne Ekle (" + m_Name + ")";
    }

    [[nodiscard]] Entity GetEntity() const noexcept { return m_Entity; }

private:
    Scene& m_Scene;
    std::string m_Name;
    TransformComponent m_Transform;
    SDFComponent m_SDF;
    Entity* m_OutSelected;
    Entity m_Entity;
};

/// Secili nesneyi silme komutu. Bilesen snapshot'i alir; Undo edildiginde birebir geri yukler.
class DeleteEntityCommand : public ICommand {
public:
    DeleteEntityCommand(Scene& scene, Entity entity, Entity* outSelected = nullptr)
        : m_Scene(scene),
          m_OutSelected(outSelected) {
        if (entity.IsValid()) {
            if (entity.HasComponent<TagComponent>()) {
                m_Tag = entity.GetComponent<TagComponent>();
            } else {
                m_Tag = TagComponent{"Entity"};
            }

            if (entity.HasComponent<TransformComponent>()) {
                m_Transform = entity.GetComponent<TransformComponent>();
            }

            if (entity.HasComponent<SDFComponent>()) {
                m_SDF = entity.GetComponent<SDFComponent>();
            }

            if (entity.HasComponent<VelocityComponent>()) {
                m_Velocity = entity.GetComponent<VelocityComponent>();
            }

            m_Entity = entity;
        }
    }

    void Execute() override {
        if (m_Entity.IsValid()) {
            if (m_OutSelected && *m_OutSelected == m_Entity) {
                *m_OutSelected = Entity();
            }
            m_Scene.DestroyEntity(m_Entity);
            m_Entity = Entity();
        }
    }

    void Undo() override {
        m_Entity = m_Scene.CreateEntity();
        if (m_Tag) {
            m_Entity.AddComponent<TagComponent>(*m_Tag);
        }
        if (m_Transform) {
            m_Entity.AddComponent<TransformComponent>(*m_Transform);
        }
        if (m_SDF) {
            m_Entity.AddComponent<SDFComponent>(*m_SDF);
        }
        if (m_Velocity) {
            m_Entity.AddComponent<VelocityComponent>(*m_Velocity);
        }
        if (m_OutSelected) {
            *m_OutSelected = m_Entity;
        }
    }

    [[nodiscard]] std::string GetName() const override {
        return "Nesneyi Sil (" + (m_Tag ? m_Tag->tag : "Entity") + ")";
    }

    [[nodiscard]] Entity GetEntity() const noexcept { return m_Entity; }

private:
    Scene& m_Scene;
    Entity* m_OutSelected;
    Entity m_Entity;

    std::optional<TagComponent> m_Tag;
    std::optional<TransformComponent> m_Transform;
    std::optional<SDFComponent> m_SDF;
    std::optional<VelocityComponent> m_Velocity;
};

/// Transform degisikligi komutu (Gizmo veya Inspector icin)
class ModifyTransformCommand : public ICommand {
public:
    ModifyTransformCommand(Entity entity, TransformComponent oldT, TransformComponent newT)
        : m_Entity(entity),
          m_OldTransform(oldT),
          m_NewTransform(newT) {}

    void Execute() override {
        if (m_Entity.IsValid() && m_Entity.HasComponent<TransformComponent>()) {
            m_Entity.GetComponent<TransformComponent>() = m_NewTransform;
        }
    }

    void Undo() override {
        if (m_Entity.IsValid() && m_Entity.HasComponent<TransformComponent>()) {
            m_Entity.GetComponent<TransformComponent>() = m_OldTransform;
        }
    }

    [[nodiscard]] std::string GetName() const override {
        return "Transform Degistir";
    }

private:
    Entity m_Entity;
    TransformComponent m_OldTransform;
    TransformComponent m_NewTransform;
};

} // namespace Astral
