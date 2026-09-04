#include "Astral/Editor/Panels/SceneHierarchy.hpp"
#include "Astral/Core/Components.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>

namespace Astral {

static const char* s_PrimitiveNames[] = {
    "Kure (Sphere)",
    "Kutu (Box)",
    "Torus (Simit)",
    "Zemin (Plane)",
    "Kapsul (Capsule)",
    "Silindir (Cylinder)"
};

namespace {

std::string BuildEntityLabel(Entity entity) {
    std::string label;
    if (entity.HasComponent<TagComponent>()) {
        label = entity.GetComponent<TagComponent>().tag;
    }
    if (label.empty() || label == "Entity") {
        label = "Entity " + std::to_string(entity.GetIndex());
    }
    if (entity.HasComponent<SDFComponent>()) {
        const auto type = std::min<size_t>(entity.GetComponent<SDFComponent>().primitiveType, std::size(s_PrimitiveNames) - 1);
        label += "  ·  " + std::string(s_PrimitiveNames[type]);
    }
    return label;
}

} // namespace

void SceneHierarchy::DrawEntityNode(Scene& scene, EntityHandle entityId, Entity& selectedEntity) {
    auto& registry = scene.GetRegistry();
    if (!registry.IsAlive(entityId) || !m_Visited.insert(entityId).second) return;

    Entity currentEntity(entityId, &scene);
    const bool isSelected = selectedEntity == currentEntity;
    const HierarchyComponent* hierarchy = registry.HasComponent<HierarchyComponent>(entityId)
        ? &registry.GetComponent<HierarchyComponent>(entityId)
        : nullptr;
    const bool hasChildren = hierarchy && std::any_of(
        hierarchy->children.begin(), hierarchy->children.end(),
        [&registry](EntityHandle child) { return registry.IsAlive(child); });

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(entityId)));
    const std::string label = BuildEntityLabel(currentEntity);
    const bool open = ImGui::TreeNodeEx("##EntityNode", flags, "%s", label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) selectedEntity = currentEntity;

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ASTRAL_ENTITY_HANDLE", &entityId, sizeof(entityId));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASTRAL_ENTITY_HANDLE")) {
            m_PendingChild = *static_cast<const EntityHandle*>(payload->Data);
            m_PendingParent = entityId;
            m_HasPendingReparent = true;
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Alt nesne ekle")) m_PendingChild = entityId;
        const bool hasParent = hierarchy && registry.IsAlive(hierarchy->parent);
        if (ImGui::MenuItem("Parent bagini kaldir", nullptr, false, hasParent)) {
            m_PendingChild = entityId;
            m_PendingParent = NullEntityHandle;
            m_HasPendingReparent = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(hasChildren ? "Alt agacla birlikte sil" : "Sil")) m_PendingDelete = entityId;
        ImGui::EndPopup();
    }

    if (hasChildren && open) {
        for (EntityHandle child : hierarchy->children) DrawEntityNode(scene, child, selectedEntity);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void SceneHierarchy::Draw(Scene& scene, Entity& selectedEntity) {
    ImGui::Begin("Sahne Hiyerarsisi");

    m_PendingDelete = NullEntityHandle;
    m_PendingChild = NullEntityHandle;
    m_PendingParent = NullEntityHandle;
    m_HasPendingReparent = false;

    // ── Add Entity button (full-width, accent styled) ────────
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.176f, 0.365f, 0.667f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.220f, 0.420f, 0.750f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.145f, 0.310f, 0.580f, 1.0f));

    if (ImGui::Button("+ Yeni Nesne Ekle", ImVec2(-1, 28))) {
        Entity newObj = scene.CreateEntity();
        newObj.AddComponent<TransformComponent>(
            glm::vec3(0.0f, 0.8f, 0.0f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(0.5f)
        );
        newObj.AddComponent<SDFComponent>(
            0u,   // Kure
            3u,   // SmoothUnion
            0.25f, 1u,
            glm::vec3(0.85f, 0.35f, 0.15f),
            0.3f, 0.5f
        );
        selectedEntity = newObj;
    }

    ImGui::PopStyleColor(3);

    auto& transforms = scene.GetRegistry().GetView<TransformComponent>();
    ImGui::SeparatorText("SAHNE AGACI");
    ImGui::TextDisabled("%zu nesne  ·  Surukleyerek parent ata", transforms.Size());

    // ── Entity list ──────────────────────────────────────────
    ImVec2 listSize = ImVec2(-1, ImGui::GetContentRegionAvail().y - 40.0f);
    ImGui::BeginChild("EntityList", listSize, true);

    m_Roots.clear();
    m_Visited.clear();
    m_Roots.reserve(transforms.Size());
    m_Visited.reserve(transforms.Size());
    for (auto&& [entityId, transform] : transforms) {
        (void)transform;
        const bool hasValidParent = scene.GetRegistry().HasComponent<HierarchyComponent>(entityId) &&
            scene.GetRegistry().IsAlive(scene.GetRegistry().GetComponent<HierarchyComponent>(entityId).parent);
        if (!hasValidParent) m_Roots.push_back(entityId);
    }
    for (EntityHandle root : m_Roots) DrawEntityNode(scene, root, selectedEntity);
    for (auto&& [entityId, transform] : transforms) {
        (void)transform;
        DrawEntityNode(scene, entityId, selectedEntity);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Button("Koke birak", ImVec2(-1.0f, 28.0f));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASTRAL_ENTITY_HANDLE")) {
            m_PendingChild = *static_cast<const EntityHandle*>(payload->Data);
            m_PendingParent = NullEntityHandle;
            m_HasPendingReparent = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    if (m_HasPendingReparent) {
        m_ReparentRejected = !scene.SetParent(m_PendingChild, m_PendingParent);
    } else if (m_PendingChild != NullEntityHandle) {
        Entity child = scene.CreateEntity();
        child.AddComponent<TransformComponent>();
        child.AddComponent<TagComponent>("Child");
        m_ReparentRejected = !scene.SetParent(child.GetHandle(), m_PendingChild);
        selectedEntity = child;
    }
    if (m_PendingDelete != NullEntityHandle) {
        scene.DestroyEntity(m_PendingDelete);
        if (!selectedEntity.IsValid()) selectedEntity = Entity();
    }
    if (m_ReparentRejected) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Parent atamasi reddedildi: dongu veya gecersiz hedef.");
    }

    // ── Delete selected button ───────────────────────────────
    if (selectedEntity.IsValid()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("Secili Nesneyi Sil", ImVec2(-1, 26))) {
            scene.DestroyEntity(selectedEntity);
            selectedEntity = Entity();
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::End();
}

} // namespace Astral
