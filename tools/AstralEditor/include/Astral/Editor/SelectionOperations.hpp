#pragma once
#include "Astral/Editor/SelectionContext.hpp"
#include "Astral/Core/CommandStack.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include <functional>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <glm/gtc/matrix_inverse.hpp>

namespace Astral {
inline std::vector<Entity> SelectionRoots(Scene& scene, const SelectionContext& selection) {
    std::vector<Entity> roots;
    for (Entity e : selection.Entities()) {
        if (!e.IsValid() || e.GetScene() != &scene) continue;
        bool covered = false;
        std::unordered_set<EntityHandle> visited;
        for (auto p = scene.GetParent(e.GetHandle()); p != NullEntityHandle && visited.insert(p).second; p = scene.GetParent(p)) {
            if (selection.IsEntitySelected(Entity(p, &scene))) { covered = true; break; }
        }
        if (!covered) roots.push_back(e);
    }
    return roots;
}

// Apply a world-space delta only to selection roots. Selected descendants inherit it once.
inline bool ApplySelectionDelta(Scene& scene, const SelectionContext& selection, const glm::mat4& delta) {
    struct Change { Entity entity; glm::mat4 local; };
    std::vector<Change> changes;
    for (Entity e : SelectionRoots(scene, selection)) {
        if (!e.HasComponent<TransformComponent>()) continue;
        glm::mat4 local = delta * scene.GetWorldTransform(e.GetHandle());
        const auto parent = scene.GetParent(e.GetHandle());
        if (scene.GetRegistry().IsAlive(parent)) {
            const glm::mat4 parentWorld = scene.GetWorldTransform(parent);
            if (std::abs(glm::determinant(parentWorld)) < 1e-8f) return false;
            local = glm::inverse(parentWorld) * local;
        }
        changes.push_back({e, local});
    }
    for (auto& change : changes) {
        auto& t = change.entity.GetComponent<TransformComponent>();
        DecomposeTransformMatrix(change.local, t.position, t.rotation, t.scale);
    }
    return !changes.empty();
}

// Snapshot affected subtrees, not the entire scene; one user action = one undo entry.
class SelectionEntitiesCommand : public ICommand {
    struct Snapshot {
        EntityHandle source, parent;
        bool selected;
        std::vector<std::function<void(Entity)>> restore;
    };
public:
    SelectionEntitiesCommand(Scene& scene, SelectionContext& selection, bool duplicate)
        : m_Scene(scene), m_Selection(selection), m_Duplicate(duplicate), m_PrimarySource(selection.Primary().GetHandle()) {
        std::unordered_set<EntityHandle> visited;
        std::function<void(Entity)> capture = [&](Entity e) {
            if (!e.IsValid() || !visited.insert(e.GetHandle()).second) return;
            Snapshot snapshot{e.GetHandle(), scene.GetParent(e.GetHandle()), selection.IsEntitySelected(e), {}};
            auto component = [&]<typename T>() {
                if (e.HasComponent<T>()) {
                    T value = e.GetComponent<T>();
                    snapshot.restore.emplace_back([value](Entity target) { target.AddComponent<T>(value); });
                }
            };
            component.template operator()<TagComponent>();
            component.template operator()<TransformComponent>();
            component.template operator()<PreviousTransformComponent>();
            component.template operator()<CameraComponent>();
            component.template operator()<VelocityComponent>();
            component.template operator()<HealthComponent>();
            component.template operator()<SDFComponent>();
            component.template operator()<VisibilityComponent>();
            component.template operator()<SoftBodyComponent>();
            m_Snapshots.push_back(std::move(snapshot));
            m_Live.push_back(e);
            for (auto child : scene.GetChildren(e.GetHandle())) capture(Entity(child, &scene));
        };
        for (auto e : SelectionRoots(scene, selection)) capture(e);
    }
    void Execute() override { if (m_Duplicate) Restore(); else Remove(); }
    void Undo() override { if (m_Duplicate) Remove(); else Restore(); }
    std::string GetName() const override { return m_Duplicate ? "Secimi Cogalt" : "Secimi Sil"; }
private:
    void Remove() {
        for (Entity e : m_Live) if (e.IsValid()) m_Scene.DestroyEntity(e);
        m_Live.clear();
        m_Selection.ClearSelection();
    }
    void Restore() {
        m_Live.clear();
        m_Selection.ClearSelection();
        std::unordered_map<EntityHandle, EntityHandle> remap;
        for (const auto& snapshot : m_Snapshots) {
            Entity e = m_Scene.CreateEntity();
            for (const auto& restore : snapshot.restore) restore(e);
            if (m_Duplicate) {
                if (e.HasComponent<TagComponent>()) e.GetComponent<TagComponent>().tag += " (Copy)";
                if (e.HasComponent<CameraComponent>()) e.GetComponent<CameraComponent>().primary = 0;
            }
            remap[snapshot.source] = e.GetHandle();
            m_Live.push_back(e);
        }
        for (size_t i = 0; i < m_Snapshots.size(); ++i) {
            const auto& snapshot = m_Snapshots[i];
            auto parent = snapshot.parent;
            if (remap.contains(parent)) parent = remap.at(parent);
            if (m_Scene.GetRegistry().IsAlive(parent)) (void)m_Scene.SetParent(m_Live[i].GetHandle(), parent);
            if (m_Duplicate && !remap.contains(snapshot.parent) && m_Live[i].HasComponent<TransformComponent>())
                m_Live[i].GetComponent<TransformComponent>().position += glm::vec3(0.5f, 0, 0.5f);
            if (snapshot.selected) m_Selection.SelectEntity(m_Live[i], true);
        }
        if (remap.contains(m_PrimarySource)) m_Selection.SelectEntity(Entity(remap.at(m_PrimarySource), &m_Scene), true);
    }
    Scene& m_Scene;
    SelectionContext& m_Selection;
    bool m_Duplicate;
    EntityHandle m_PrimarySource;
    std::vector<Snapshot> m_Snapshots;
    std::vector<Entity> m_Live;
};
inline void EditSelection(Scene& scene, SelectionContext& selection, CommandStack* stack, bool duplicate) {
    if (selection.Entities().empty()) return;
    auto command = std::make_unique<SelectionEntitiesCommand>(scene, selection, duplicate);
    if (stack) stack->PushAndExecute(std::move(command)); else command->Execute();
}
}
