#include "Astral/Editor/Panels/SceneHierarchy.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Scene/SceneCommands.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>

namespace Astral {

namespace {

bool IsEntitySelfVisible(const Registry& registry, EntityHandle entity) {
    if (registry.HasComponent<VisibilityComponent>(entity)) {
        return registry.GetComponent<VisibilityComponent>(entity).isVisible;
    }
    if (registry.HasComponent<SDFComponent>(entity)) {
        return registry.GetComponent<SDFComponent>(entity).isVisible != 0;
    }
    return true;
}

void ToggleEntityVisibility(Registry& registry, EntityHandle entity) {
    bool current = IsEntitySelfVisible(registry, entity);
    bool next = !current;

    if (registry.HasComponent<SDFComponent>(entity)) {
        registry.GetComponent<SDFComponent>(entity).isVisible = next ? 1 : 0;
    }
    if (registry.HasComponent<VisibilityComponent>(entity)) {
        registry.GetComponent<VisibilityComponent>(entity).isVisible = next;
    } else {
        registry.AddComponent<VisibilityComponent>(entity, VisibilityComponent{ next });
    }
}

struct NodeIconInfo {
    const char* symbol;
    ImVec4 color;
};

// Görsel 2'deki gibi minimalist renkli daire/kutu ikonları
NodeIconInfo GetNodeIcon(Entity entity) {
    if (entity.HasComponent<SDFComponent>()) {
        uint32_t type = entity.GetComponent<SDFComponent>().primitiveType;
        switch (type) {
            case 0: return { "(o)", ImVec4(0.35f, 0.65f, 0.95f, 1.0f) }; // Mavi daire (Sphere)
            case 1: return { "[]",  ImVec4(0.35f, 0.85f, 0.45f, 1.0f) }; // Yeşil kutu (Box)
            case 2: return { "(o)", ImVec4(0.95f, 0.75f, 0.20f, 1.0f) }; // Sarı simit (Torus)
            case 3: return { "--",  ImVec4(0.60f, 0.60f, 0.65f, 1.0f) }; // Gri zemin (Plane)
            default:return { "(o)", ImVec4(0.40f, 0.85f, 0.85f, 1.0f) }; // Turkuaz
        }
    }
    // Genel / Root nesne için kırmızımsı içi boş halka (Görsel 2'deki gibi)
    return { "(o)", ImVec4(0.90f, 0.38f, 0.38f, 1.0f) };
}

std::string ToLowerString(std::string_view str) {
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower;
}

std::string GetEntityDisplayName(Entity entity) {
    if (entity.HasComponent<TagComponent>()) {
        const auto& tag = entity.GetComponent<TagComponent>().tag;
        if (!tag.empty()) return tag;
    }
    return "Entity " + std::to_string(entity.GetIndex());
}

} // namespace

bool SceneHierarchy::EntityMatchesFilter(Scene& scene, EntityHandle entity) const {
    if (m_SearchFilter[0] == '\0') return true;

    Entity e(entity, &scene);
    std::string name = ToLowerString(GetEntityDisplayName(e));
    std::string filter = ToLowerString(m_SearchFilter);

    if (name.find(filter) != std::string::npos) return true;

    if (e.HasComponent<SDFComponent>()) {
        const char* typeNames[] = { "kure", "kutu", "torus", "zemin", "kapsul", "silindir", "sphere", "box", "plane" };
        for (const char* tn : typeNames) {
            if (filter.find(tn) != std::string::npos) return true;
        }
    }
    return false;
}

bool SceneHierarchy::NodeOrDescendantMatchesFilter(Scene& scene, EntityHandle entity) const {
    if (m_SearchFilter[0] == '\0') return true;
    if (EntityMatchesFilter(scene, entity)) return true;

    auto& registry = scene.GetRegistry();
    if (registry.HasComponent<HierarchyComponent>(entity)) {
        const auto& hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        for (EntityHandle child : hierarchy.children) {
            if (registry.IsAlive(child) && NodeOrDescendantMatchesFilter(scene, child)) {
                return true;
            }
        }
    }
    return false;
}

void SceneHierarchy::DrawEntityNode(Scene& scene, EntityHandle entityId, Entity& selectedEntity, bool isFiltered) {
    auto& registry = scene.GetRegistry();
    if (!registry.IsAlive(entityId) || !m_Visited.insert(entityId).second) return;

    if (isFiltered && !NodeOrDescendantMatchesFilter(scene, entityId)) {
        return;
    }

    Entity currentEntity(entityId, &scene);
    const bool isSelected = (selectedEntity == currentEntity);
    const HierarchyComponent* hierarchy = registry.HasComponent<HierarchyComponent>(entityId)
        ? &registry.GetComponent<HierarchyComponent>(entityId)
        : nullptr;

    const bool hasChildren = hierarchy && std::any_of(
        hierarchy->children.begin(), hierarchy->children.end(),
        [&registry](EntityHandle child) { return registry.IsAlive(child); });

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_AllowOverlap;

    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected)   flags |= ImGuiTreeNodeFlags_Selected;
    if (isFiltered && hasChildren) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(entityId)));

    // ── Seçili Düğüm İçin Özel Yuvarlatılmış Arka Plan (Görsel 2 Selection Pill) ──
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (isSelected) {
        ImVec2 rectMin = cursorScreenPos;
        ImVec2 rectMax = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x, rectMin.y + rowHeight);
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(40, 68, 98, 220), 4.0f);
    }

    // ── Görsel 2: Hiyerarşi Rehber Kılavuz Çizgisi ────────────────────────────
    const bool hasParent = hierarchy && registry.IsAlive(hierarchy->parent);
    if (hasParent) {
        float lineX = cursorScreenPos.x - 10.0f;
        float lineMidY = cursorScreenPos.y + rowHeight * 0.5f;
        // Dikey dal çizgisi
        drawList->AddLine(ImVec2(lineX, cursorScreenPos.y - 2.0f), ImVec2(lineX, lineMidY), IM_COL32(70, 80, 95, 180), 1.0f);
        // Yatay L kolu
        drawList->AddLine(ImVec2(lineX, lineMidY), ImVec2(lineX + 8.0f, lineMidY), IM_COL32(70, 80, 95, 180), 1.0f);
    }

    // ── Görsel 2: Minimalist Renkli İkon (O / []) ─────────────────────────────
    NodeIconInfo iconInfo = GetNodeIcon(currentEntity);
    ImGui::TextColored(iconInfo.color, "%s", iconInfo.symbol);
    ImGui::SameLine(0, 5.0f);

    // ── Düğüm İsmi ve TreeNode ───────────────────────────────────────────────
    std::string displayName = GetEntityDisplayName(currentEntity);

    if (m_RenamingEntity == entityId) {
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::InputText("##InlineRename", m_RenameBuffer, sizeof(m_RenameBuffer),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (m_RenameBuffer[0] != '\0') {
                std::string newName = m_RenameBuffer;
                std::string oldName = currentEntity.HasComponent<TagComponent>() ? currentEntity.GetComponent<TagComponent>().tag : "";
                if (m_CommandStack) {
                    m_CommandStack->PushAndExecute(std::make_unique<RenameEntityCommand>(currentEntity, oldName, newName));
                } else {
                    if (currentEntity.HasComponent<TagComponent>()) {
                        currentEntity.GetComponent<TagComponent>().tag = newName;
                    } else {
                        currentEntity.AddComponent<TagComponent>(newName);
                    }
                }
            }
            m_RenamingEntity = NullEntityHandle;
        }
        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_RenamingEntity = NullEntityHandle;
        }
    }

    const bool open = ImGui::TreeNodeEx("##NodeText", flags, "%s", displayName.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedEntity = currentEntity;
    }

    // ── Drag & Drop: Doğrudan TreeNodeEx satırına bağlanmalıdır! ─────────────
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ASTRAL_ENTITY_HANDLE", &entityId, sizeof(entityId));
        ImGui::Text("Tasi: %s", displayName.c_str());
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

    // Çift tıklamayla yeniden adlandırma (F2)
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !hasChildren) {
        m_RenamingEntity = entityId;
        std::strncpy(m_RenameBuffer, displayName.c_str(), sizeof(m_RenameBuffer) - 1);
    }

    // ── Görsel 2: Sağ Tarafa Hizalanmış Hızlı Eylemler (Right-Aligned Icons) ──
    const float contentRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    float currentIconX = contentRight - 48.0f;

    // 1. Görünürlük (Eye / Target Icon)
    const bool isSelfVisible = IsEntitySelfVisible(registry, entityId);

    ImGui::SameLine(currentIconX);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Text, isSelfVisible ? ImVec4(0.85f, 0.85f, 0.85f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

    if (ImGui::SmallButton(isSelfVisible ? "(o)" : "(-)")) {
        if (m_CommandStack) {
            m_CommandStack->PushAndExecute(std::make_unique<SetVisibilityCommand>(currentEntity, isSelfVisible, !isSelfVisible));
        } else {
            ToggleEntityVisibility(registry, entityId);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isSelfVisible ? "Gorunur (Gizlemek icin tiklayin)" : "Gizli (Gostermek icin tiklayin)");
    }
    ImGui::PopStyleColor(3);

    // 2. SDF / Bileşen Rozeti
    if (currentEntity.HasComponent<SDFComponent>()) {
        ImGui::SameLine(contentRight - 22.0f);
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 0.8f), "[S]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("SDF Geometri Bileseni");
        }
    }

    // ── Sağ Tık Menüsü (Context Menu) ─────────────────────────────────────────
    if (ImGui::BeginPopupContextItem()) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "%s", displayName.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Yeniden Adlandir", "F2")) {
            m_RenamingEntity = entityId;
            std::strncpy(m_RenameBuffer, displayName.c_str(), sizeof(m_RenameBuffer) - 1);
        }

        if (ImGui::MenuItem("Klonla (Duplicate)", "Ctrl+D")) {
            m_PendingDuplicate = entityId;
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Alt Nesne Ekle (Add Child)")) {
            if (ImGui::MenuItem("Kure (Sphere)"))       { m_PendingAddPrimitive = 0; m_PendingAddToParent = entityId; }
            if (ImGui::MenuItem("Kutu (Box)"))           { m_PendingAddPrimitive = 1; m_PendingAddToParent = entityId; }
            if (ImGui::MenuItem("Torus (Simit)"))        { m_PendingAddPrimitive = 2; m_PendingAddToParent = entityId; }
            if (ImGui::MenuItem("Silindir (Cylinder)")) { m_PendingAddPrimitive = 5; m_PendingAddToParent = entityId; }
            if (ImGui::MenuItem("Bos Nesne (Empty)"))   { m_PendingAddPrimitive = 99; m_PendingAddToParent = entityId; }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Parent Bagini Kaldir", nullptr, false, hasParent)) {
            m_PendingChild = entityId;
            m_PendingParent = NullEntityHandle;
            m_HasPendingReparent = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem(hasChildren ? "Alt Agacla Birlikte Sil" : "Sil (Delete)", "Del")) {
            m_PendingDelete = entityId;
        }
        ImGui::EndPopup();
    }

    // ── Çocuk Düğümleri Çiz ──────────────────────────────────────────────────
    if (hasChildren && open) {
        for (EntityHandle child : hierarchy->children) {
            DrawEntityNode(scene, child, selectedEntity, isFiltered);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneHierarchy::Draw(Scene& scene, Entity& selectedEntity, CommandStack* commandStack) {
    m_CommandStack = commandStack;
    ImGui::Begin("Sahne Hiyerarsisi");

    m_PendingDelete = NullEntityHandle;
    m_PendingDuplicate = NullEntityHandle;
    m_PendingChild = NullEntityHandle;
    m_PendingParent = NullEntityHandle;
    m_HasPendingReparent = false;
    m_PendingAddPrimitive = -1;
    m_PendingAddToParent = NullEntityHandle;

    // ── 1. Üst Aksiyon Çubuğu: + Yeni Nesne Ekle Butonu ──────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.60f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.42f, 0.72f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.30f, 0.52f, 1.0f));

    if (ImGui::Button("+ Yeni Nesne Ekle", ImVec2(-1, 28))) {
        ImGui::OpenPopup("AddPrimitivePopup");
    }
    ImGui::PopStyleColor(3);

    if (ImGui::BeginPopup("AddPrimitivePopup")) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Yeni 3D Primitif");
        ImGui::Separator();
        if (ImGui::MenuItem("Kure (Sphere)"))       { m_PendingAddPrimitive = 0; }
        if (ImGui::MenuItem("Kutu (Box)"))           { m_PendingAddPrimitive = 1; }
        if (ImGui::MenuItem("Torus (Simit)"))        { m_PendingAddPrimitive = 2; }
        if (ImGui::MenuItem("Zemin (Plane)"))        { m_PendingAddPrimitive = 3; }
        if (ImGui::MenuItem("Silindir (Cylinder)")) { m_PendingAddPrimitive = 5; }
        if (ImGui::MenuItem("Bos Nesne (Empty)"))   { m_PendingAddPrimitive = 99; }
        ImGui::EndPopup();
    }

    ImGui::Dummy(ImVec2(0, 2.0f));

    // ── 2. Canlı Arama / Filtreleme Çubuğu (Search Box) ─────────────────────
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 26.0f);
    ImGui::InputTextWithHint("##HierarchySearch", "Nesne ara... (Filter)", m_SearchFilter, sizeof(m_SearchFilter));
    ImGui::SameLine();
    if (ImGui::Button("X", ImVec2(22, 0))) {
        m_SearchFilter[0] = '\0';
    }

    ImGui::Separator();

    auto& transforms = scene.GetRegistry().GetView<TransformComponent>();
    const size_t entityCount = transforms.Size();

    // ── 3. Klavye Kısayolları (Ctrl+D = Duplicate, Del = Delete, F2 = Rename) ─
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && selectedEntity.IsValid()) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            bool ctrlPressed = io.KeyCtrl;
            if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_D)) {
                m_PendingDuplicate = selectedEntity.GetHandle();
            } else if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                m_PendingDelete = selectedEntity.GetHandle();
            } else if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
                m_RenamingEntity = selectedEntity.GetHandle();
                std::strncpy(m_RenameBuffer, GetEntityDisplayName(selectedEntity).c_str(), sizeof(m_RenameBuffer) - 1);
            }
        }
    }

    // ── 4. Başlık ve Sayaç ──────────────────────────────────────────────────
    ImGui::TextDisabled("SAHNE AGACI (%zu)", entityCount);

    // ── 5. Varlık Ağacı Görünümü (Tree View) ────────────────────────────────
    ImVec2 listSize = ImVec2(-1, ImGui::GetContentRegionAvail().y - 36.0f);
    ImGui::BeginChild("EntityTreeChild", listSize, true);

    m_Roots.clear();
    m_Visited.clear();
    m_Roots.reserve(entityCount);
    m_Visited.reserve(entityCount);

    // O(N) tek geçişle kökleri belirle
    for (auto&& [entityId, transform] : transforms) {
        (void)transform;
        const bool hasValidParent = scene.GetRegistry().HasComponent<HierarchyComponent>(entityId) &&
            scene.GetRegistry().IsAlive(scene.GetRegistry().GetComponent<HierarchyComponent>(entityId).parent);
        if (!hasValidParent) {
            m_Roots.push_back(entityId);
        }
    }

    bool isFiltered = (m_SearchFilter[0] != '\0');

    // Kök düğümlerden özyinelemeli ağacı çiz
    for (EntityHandle root : m_Roots) {
        DrawEntityNode(scene, root, selectedEntity, isFiltered);
    }

    // Boş alana tıklayınca seçimi temizle
    // Kalan boş alana tıklayınca seçimi temizle ve kök seviyeye taşıma (Unparent) hedefi yap
    const float remainingY = ImGui::GetContentRegionAvail().y;
    if (remainingY > 0.0f) {
        ImGui::Dummy(ImVec2(-1.0f, remainingY));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedEntity = Entity();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASTRAL_ENTITY_HANDLE")) {
                m_PendingChild = *static_cast<const EntityHandle*>(payload->Data);
                m_PendingParent = NullEntityHandle;
                m_HasPendingReparent = true;
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::EndChild();

    // ── 6. Bekleyen Eylemlerin İşlenmesi ────────────────────────────────────
    if (m_PendingAddPrimitive >= 0) {
        glm::vec3 pos = glm::vec3(0.0f, 0.8f, 0.0f);
        glm::vec3 scale = glm::vec3(0.7f);
        glm::vec3 albedo = glm::vec3(0.85f, 0.45f, 0.2f);

        std::string tag = "New Object";
        if (m_PendingAddPrimitive == 0) { tag = "Sphere"; scale = glm::vec3(0.75f); albedo = glm::vec3(0.9f, 0.3f, 0.2f); }
        else if (m_PendingAddPrimitive == 1) { tag = "Box"; scale = glm::vec3(0.6f); albedo = glm::vec3(0.25f, 0.6f, 0.95f); }
        else if (m_PendingAddPrimitive == 2) { tag = "Torus"; scale = glm::vec3(0.7f, 0.25f, 1.0f); albedo = glm::vec3(0.95f, 0.8f, 0.15f); }
        else if (m_PendingAddPrimitive == 3) { tag = "Plane"; scale = glm::vec3(12.0f, 0.2f, 12.0f); pos = glm::vec3(0.0f, -0.5f, 0.0f); albedo = glm::vec3(0.3f, 0.32f, 0.35f); }
        else if (m_PendingAddPrimitive == 5) { tag = "Cylinder"; scale = glm::vec3(0.5f, 0.8f, 0.5f); albedo = glm::vec3(0.4f, 0.85f, 0.85f); }
        else if (m_PendingAddPrimitive == 99) { tag = "Empty Node"; }

        TransformComponent t(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale);
        SDFComponent s(
            static_cast<uint32_t>(m_PendingAddPrimitive == 99 ? 0 : m_PendingAddPrimitive),
            static_cast<uint32_t>(CSGOperation::Union),
            0.2f, 1u, albedo, 0.4f, 0.3f
        );

        if (m_CommandStack) {
            m_CommandStack->PushAndExecute(std::make_unique<CreateEntityCommand>(scene, tag, t, s, &selectedEntity));
            if (m_PendingAddToParent != NullEntityHandle && selectedEntity.IsValid()) {
                m_CommandStack->PushAndExecute(std::make_unique<ReparentEntityCommand>(scene, selectedEntity.GetHandle(), NullEntityHandle, m_PendingAddToParent));
            }
        } else {
            Entity newObj = scene.CreateEntity();
            newObj.AddComponent<TagComponent>(tag);
            newObj.AddComponent<TransformComponent>(t);
            if (m_PendingAddPrimitive != 99) {
                newObj.AddComponent<SDFComponent>(s);
            }
            if (m_PendingAddToParent != NullEntityHandle) {
                (void)scene.SetParent(newObj.GetHandle(), m_PendingAddToParent);
            }
            selectedEntity = newObj;
        }

        m_PendingAddPrimitive = -1;
    }

    if (m_PendingDuplicate != NullEntityHandle) {
        Entity copy = scene.DuplicateEntity(m_PendingDuplicate);
        if (copy.IsValid()) {
            selectedEntity = copy;
        }
    }

    if (m_HasPendingReparent) {
        Entity childEnt(m_PendingChild, &scene);
        EntityHandle oldParent = NullEntityHandle;
        if (childEnt.IsValid() && childEnt.HasComponent<HierarchyComponent>()) {
            oldParent = childEnt.GetComponent<HierarchyComponent>().parent;
        }
        if (m_CommandStack) {
            m_CommandStack->PushAndExecute(std::make_unique<ReparentEntityCommand>(scene, m_PendingChild, oldParent, m_PendingParent));
        } else {
            m_ReparentRejected = !scene.SetParent(m_PendingChild, m_PendingParent);
        }
    }

    if (m_PendingDelete != NullEntityHandle) {
        if (m_CommandStack) {
            m_CommandStack->PushAndExecute(std::make_unique<DeleteEntityCommand>(scene, Entity(m_PendingDelete, &scene), &selectedEntity));
        } else {
            scene.DestroyEntity(m_PendingDelete);
            if (selectedEntity.GetHandle() == m_PendingDelete) {
                selectedEntity = Entity();
            }
        }
    }

    // ── 7. Alt Bilgi / Sil Butonu ───────────────────────────────────────────
    if (selectedEntity.IsValid()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.12f, 0.12f, 1.0f));

        if (ImGui::Button("Secili Nesneyi Sil (Del)", ImVec2(-1, 26))) {
            if (m_CommandStack) {
                m_CommandStack->PushAndExecute(std::make_unique<DeleteEntityCommand>(scene, selectedEntity, &selectedEntity));
            } else {
                scene.DestroyEntity(selectedEntity);
                selectedEntity = Entity();
            }
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::TextDisabled("Nesne secilmedi");
    }

    if (m_ReparentRejected) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Parent reddedildi: dongu olusur!");
    }

    ImGui::End();
}

} // namespace Astral
