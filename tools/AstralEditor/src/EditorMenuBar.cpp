#include "Astral/Editor/EditorMenuBar.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Scene/SceneSerializer.hpp"
#include "Astral/Project/Project.hpp"
#include "Astral/Project/ProjectSerializer.hpp"
#include "Astral/Renderer/SDFEdit.hpp"

#include <imgui.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace Astral {

namespace {

// ============================================================================
// Internal Persistence State
// ============================================================================

static std::string s_CurrentScenePath = "";
static char s_PathBuffer[512] = "assets/scenes/untitled.astral";
static char s_OpenPathBuffer[512] = "assets/scenes/";

static char s_NewProjectNameBuffer[128] = "MyNewGame";
static char s_NewProjectPathBuffer[512] = "Projects/MyNewGame";
static char s_OpenProjectPathBuffer[512] = "";

static std::string s_NotificationTitle = "";
static std::string s_NotificationMessage = "";
static bool s_NotificationIsError = false;

// Flags for delayed ImGui::OpenPopup calls
static bool s_ShowNotificationPopup_Pending = false;
static bool s_ShowNewConfirmPopup_Pending = false;
static bool s_ShowSaveAsPopup_Pending = false;
static bool s_ShowLoadPopup_Pending = false;
static bool s_ShowAboutModal_Pending = false;
static bool s_ShowNewProjectPopup_Pending = false;
static bool s_ShowOpenProjectPopup_Pending = false;

static void ShowNotification(const std::string& title, const std::string& message, bool isError) {
    s_NotificationTitle = title;
    s_NotificationMessage = message;
    s_NotificationIsError = isError;
    s_ShowNotificationPopup_Pending = true;
}

#ifdef _WIN32
static std::string NativeOpenFileDialog(const char* filter = "Astral Scene (*.astral)\0*.astral\0All Files (*.*)\0*.*\0") {
    OPENFILENAMEA ofn;
    CHAR szFile[512] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = "assets\\scenes";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

static std::string NativeSaveFileDialog(const char* filter = "Astral Scene (*.astral)\0*.astral\0All Files (*.*)\0*.*\0") {
    OPENFILENAMEA ofn;
    CHAR szFile[512] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "astral";
    ofn.lpstrInitialDir = "assets\\scenes";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}
#endif

#ifdef _WIN32
static std::string NativeOpenProjectDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[512] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Astral Project (*.astralproj)\0*.astralproj\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = "Projects";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}
#endif

static void ExecuteSave(Scene& scene, const std::string& filepath) {
    if (filepath.empty()) return;

    std::filesystem::path p(filepath);
    if (!p.has_extension() || p.extension() != ".astral") {
        p.replace_extension(".astral");
    }

    if (SceneSerializer::Serialize(scene, p)) {
        s_CurrentScenePath = p.string();
        scene.SetName(p.stem().string());
        ShowNotification("Basarili", "Sahne basariyla kaydedildi:\n" + s_CurrentScenePath, false);
    } else {
        ShowNotification("Sahne Kaydetme Hatasi", "Sahne diske yazilamadi!\nLutfen dosya yolunu ve izinleri kontrol edin:\n" + p.string(), true);
    }
}

static void ExecuteOpen(Scene& scene, Entity& selectedEntity, const std::string& filepath) {
    if (filepath.empty()) return;

    std::filesystem::path p(filepath);
    if (!std::filesystem::exists(p)) {
        ShowNotification("Dosya Bulunamadi", "Belirtilen dosya mevcut degil:\n" + filepath, true);
        return;
    }

    if (SceneSerializer::Deserialize(scene, p)) {
        selectedEntity = Entity(); // Secili nesneyi temizle
        s_CurrentScenePath = p.string();
        scene.SetName(p.stem().string());
        ShowNotification("Basarili", "Sahne basariyla yuklendi:\n" + s_CurrentScenePath, false);
    } else {
        ShowNotification("Sahne Yukleme Hatasi", "Dosya deserialize edilemedi veya bozuk .astral dosyasi!\nDosya: " + filepath, true);
    }
}

enum class SceneTemplate {
    Basic,
    Empty
};

static void PopulateBasicScene(Scene& scene) {
    scene.Clear();

    // 1. Zemin (Floor Platform)
    Entity floorEntity = scene.CreateEntity();
    floorEntity.AddComponent<TransformComponent>(
        glm::vec3(0.0f, -0.5f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(12.0f, 0.2f, 12.0f)
    );
    floorEntity.AddComponent<SDFComponent>(
        static_cast<uint32_t>(PrimitiveType::Box),
        static_cast<uint32_t>(CSGOperation::Union),
        0.0f, 1u, glm::vec3(0.24f, 0.25f, 0.28f), 0.85f, 0.05f
    );

    // 2. Baslangic Kup Nesnesi (Default Cube)
    Entity cubeEntity = scene.CreateEntity();
    cubeEntity.AddComponent<TransformComponent>(
        glm::vec3(0.0f, 0.5f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(0.75f)
    );
    cubeEntity.AddComponent<SDFComponent>(
        static_cast<uint32_t>(PrimitiveType::Box),
        static_cast<uint32_t>(CSGOperation::Union),
        0.05f, 1u, glm::vec3(0.2f, 0.55f, 0.95f), 0.35f, 0.4f
    );
}

static void ExecuteNewScene(Scene& scene, Entity& selectedEntity, SceneTemplate templateType = SceneTemplate::Basic) {
    selectedEntity = Entity();
    s_CurrentScenePath.clear();

    if (templateType == SceneTemplate::Basic) {
        PopulateBasicScene(scene);
        scene.SetName("Untitled Scene (Basic)");
    } else {
        scene.Clear();
        scene.SetName("Untitled Scene (Empty)");
    }
}

static void ExecuteOpenProject(Scene& scene, Entity& selectedEntity, const std::string& filepath) {
    if (filepath.empty()) return;

    std::filesystem::path p(filepath);
    if (!std::filesystem::exists(p)) {
        ShowNotification("Proje Bulunamadi", "Belirtilen proje dosyasi mevcut degil:\n" + filepath, true);
        return;
    }

    auto project = Project::LoadProject(p);
    if (project) {
        // Varsa baslangic sahnesini yukle
        auto startScenePath = project->GetAssetFileSystemPath(project->GetConfig().startScene);
        if (std::filesystem::exists(startScenePath)) {
            ExecuteOpen(scene, selectedEntity, startScenePath.string());
        } else {
            ExecuteNewScene(scene, selectedEntity);
        }
        ShowNotification("Proje Yuklendi", "Proje basariyla yuklendi:\n" + project->GetConfig().name + "\n" + p.string(), false);
    } else {
        ShowNotification("Proje Hatasi", "Proje dosyasi yuklenemedi veya bozuk!\nDosya: " + filepath, true);
    }
}

static void ExecuteNewProject(Scene& scene, Entity& selectedEntity, const std::string& directory, const std::string& name) {
    if (directory.empty() || name.empty()) {
        ShowNotification("Gecersiz Parametre", "Proje dizini ve adi bos birakilamaz!", true);
        return;
    }

    auto project = Project::NewProject(directory, name);
    if (project) {
        ExecuteNewScene(scene, selectedEntity);
        ShowNotification("Proje Olusturuldu", "Yeni proje basariyla olusturuldu:\n" + project->GetConfig().name, false);
    } else {
        ShowNotification("Proje Hatasi", "Yeni proje olusturulamadi!", true);
    }
}

} // namespace

std::string GetEditorCurrentScenePath() {
    return s_CurrentScenePath;
}

void SetEditorCurrentScenePath(const std::string& path) {
    s_CurrentScenePath = path;
}

void DrawEditorMenuBar(Scene& scene, Entity& selectedEntity,
                       MenuBarActions& actions, bool& showDemoWindowState,
                       const InputSystem& input, CommandStack& commandStack) {
    ImGuiIO& io = ImGui::GetIO();

    // ── Keyboard Shortcuts (Ctrl+S, Ctrl+O, Ctrl+N, Ctrl+Z, Ctrl+Y) ───────────
    if (!io.WantTextInput) {
        bool ctrlPressed = input.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || input.IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
        bool shiftPressed = input.IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || input.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);

        if (ctrlPressed && shiftPressed && input.IsKeyJustPressed(GLFW_KEY_N)) {
            s_ShowNewProjectPopup_Pending = true;
        } else if (ctrlPressed && shiftPressed && input.IsKeyJustPressed(GLFW_KEY_O)) {
#ifdef _WIN32
            std::string selected = NativeOpenProjectDialog();
            if (!selected.empty()) {
                ExecuteOpenProject(scene, selectedEntity, selected);
            }
#else
            s_ShowOpenProjectPopup_Pending = true;
#endif
        } else if (ctrlPressed && !shiftPressed && input.IsKeyJustPressed(GLFW_KEY_S)) {
            if (s_CurrentScenePath.empty()) {
                s_ShowSaveAsPopup_Pending = true;
            } else {
                ExecuteSave(scene, s_CurrentScenePath);
                actions.saveScene = true;
            }
        } else if (ctrlPressed && shiftPressed && input.IsKeyJustPressed(GLFW_KEY_S)) {
            s_ShowSaveAsPopup_Pending = true;
        } else if (ctrlPressed && !shiftPressed && input.IsKeyJustPressed(GLFW_KEY_O)) {
            s_ShowLoadPopup_Pending = true;
        } else if (ctrlPressed && !shiftPressed && input.IsKeyJustPressed(GLFW_KEY_N)) {
            if (scene.GetRegistry().GetAliveEntityCount() > 0) {
                s_ShowNewConfirmPopup_Pending = true;
            } else {
                ExecuteNewScene(scene, selectedEntity);
                actions.newScene = true;
            }
        } else if (ctrlPressed && !shiftPressed && input.IsKeyJustPressed(GLFW_KEY_Z)) {
            if (commandStack.CanUndo()) {
                commandStack.Undo();
                actions.undo = true;
            }
        } else if ((ctrlPressed && input.IsKeyJustPressed(GLFW_KEY_Y)) ||
                   (ctrlPressed && shiftPressed && input.IsKeyJustPressed(GLFW_KEY_Z))) {
            if (commandStack.CanRedo()) {
                commandStack.Redo();
                actions.redo = true;
            }
        }
    }

    if (ImGui::BeginMenuBar()) {
        // ── File ──────────────────────────────────────────────────
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Yeni Proje...", "Ctrl+Shift+N")) {
                s_ShowNewProjectPopup_Pending = true;
            }
            if (ImGui::MenuItem("Projeyi Ac...", "Ctrl+Shift+O")) {
#ifdef _WIN32
                std::string selected = NativeOpenProjectDialog();
                if (!selected.empty()) {
                    ExecuteOpenProject(scene, selectedEntity, selected);
                }
#else
                s_ShowOpenProjectPopup_Pending = true;
#endif
            }
            if (ImGui::MenuItem("Projeyi Kaydet", nullptr, false, Project::GetActive() != nullptr)) {
                if (Project::SaveActive()) {
                    ShowNotification("Basarili", "Proje dosyalari basariyla kaydedildi.", false);
                } else {
                    ShowNotification("Hata", "Proje kaydedilemedi!", true);
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Yeni Sahne")) {
                if (ImGui::MenuItem("Standart Seviye (Basic Level)", "Ctrl+N")) {
                    ExecuteNewScene(scene, selectedEntity, SceneTemplate::Basic);
                    actions.newScene = true;
                }
                if (ImGui::MenuItem("Bos Seviye (Empty Level)", "Ctrl+Alt+N")) {
                    ExecuteNewScene(scene, selectedEntity, SceneTemplate::Empty);
                    actions.newScene = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Sablon Penceresini Ac...")) {
                    s_ShowNewConfirmPopup_Pending = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Sahneyi Ac...", "Ctrl+O")) {
                s_ShowLoadPopup_Pending = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Sahneyi Kaydet", "Ctrl+S")) {
                if (s_CurrentScenePath.empty()) {
                    s_ShowSaveAsPopup_Pending = true;
                } else {
                    ExecuteSave(scene, s_CurrentScenePath);
                    actions.saveScene = true;
                }
            }
            if (ImGui::MenuItem("Farkli Kaydet...", "Ctrl+Shift+S")) {
                s_ShowSaveAsPopup_Pending = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cikis", "Alt+F4")) {
                actions.exitApp = true;
            }
            ImGui::EndMenu();
        }

        // ── Edit ──────────────────────────────────────────────────
        if (ImGui::BeginMenu("Edit")) {
            std::string undoLabel = "Geri Al";
            if (commandStack.CanUndo()) {
                undoLabel += " (" + commandStack.GetUndoName() + ")";
            }
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, commandStack.CanUndo())) {
                commandStack.Undo();
                actions.undo = true;
            }

            std::string redoLabel = "Yeniden Yap";
            if (commandStack.CanRedo()) {
                redoLabel += " (" + commandStack.GetRedoName() + ")";
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, commandStack.CanRedo())) {
                commandStack.Redo();
                actions.redo = true;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Secili Nesneyi Sil", "Delete", false, selectedEntity.IsValid())) {
                actions.deleteSelected = true;
            }
            ImGui::EndMenu();
        }

        // ── Scene ─────────────────────────────────────────────────
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::BeginMenu("Yeni Nesne Ekle")) {
                if (ImGui::MenuItem("Kure (Sphere)"))       { actions.addSphere = true; }
                if (ImGui::MenuItem("Kutu (Box)"))           { actions.addBox = true; }
                if (ImGui::MenuItem("Torus (Simit)"))        { actions.addTorus = true; }
                if (ImGui::MenuItem("Silindir (Cylinder)")) { actions.addCylinder = true; }
                if (ImGui::MenuItem("Zemin (Plane)"))        { actions.addPlane = true; }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Sahneyi Temizle", nullptr, false, scene.GetRegistry().GetAliveEntityCount() > 0)) {
                actions.clearScene = true;
            }
            ImGui::EndMenu();
        }

        // ── View ──────────────────────────────────────────────────
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Layout Sifirla (Reset)")) {
                actions.resetLayout = true;
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Penceresi", nullptr, &showDemoWindowState);
            ImGui::EndMenu();
        }

        // ── Help ──────────────────────────────────────────────────
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Astral Engine Hakkinda...")) {
                s_ShowAboutModal_Pending = true;
            }
            ImGui::EndMenu();
        }

        // ── Play / Pause / Stop Toolbar ───────────────────────────
        float menuBarWidth = ImGui::GetWindowWidth();
        float buttonWidth = 75.0f;
        float totalButtonsWidth = buttonWidth * 2.0f + 10.0f;
        float centerPos = (menuBarWidth - totalButtonsWidth) * 0.5f;
        if (centerPos > ImGui::GetCursorPosX()) {
            ImGui::SameLine(centerPos);
        }

        if (scene.IsRunning()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.22f, 0.22f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.32f, 0.32f, 1.0f));
            if (ImGui::Button("Stop [F5]", ImVec2(buttonWidth, 0))) {
                actions.stopPlay = true;
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Simulasyonu Durdur ve Authoring Sahnesine Don (F5)");

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.75f, 0.25f, 1.0f));
            if (ImGui::Button("Pause [F6]", ImVec2(buttonWidth, 0))) {
                actions.pauseToggle = true;
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Duraklat / Devam Et (F6)");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.28f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.85f, 0.32f, 1.0f));
            if (ImGui::Button("Play [F5]", ImVec2(buttonWidth, 0))) {
                actions.playToggle = true;
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play Moduna Gec (Sahneyi Klonlar ve Baslatir - F5)");
        }

        ImGui::EndMenuBar();
    }

    // ========================================================================
    // Modal Dialogs Handling
    // ========================================================================

    // 1. Trigger pending popups
    if (s_ShowNewProjectPopup_Pending) {
        ImGui::OpenPopup("Yeni Proje Olustur##NewProjectModal");
        s_ShowNewProjectPopup_Pending = false;
    }
    if (s_ShowOpenProjectPopup_Pending) {
        ImGui::OpenPopup("Projeyi Ac##OpenProjectModal");
        s_ShowOpenProjectPopup_Pending = false;
    }
    if (s_ShowNewConfirmPopup_Pending) {
        ImGui::OpenPopup("Yeni Sahne Onayi##NewSceneConfirmModal");
        s_ShowNewConfirmPopup_Pending = false;
    }
    if (s_ShowSaveAsPopup_Pending) {
        if (!s_CurrentScenePath.empty()) {
            std::strncpy(s_PathBuffer, s_CurrentScenePath.c_str(), sizeof(s_PathBuffer) - 1);
        }
        ImGui::OpenPopup("Sahneyi Farkli Kaydet##SaveAsModal");
        s_ShowSaveAsPopup_Pending = false;
    }
    if (s_ShowLoadPopup_Pending) {
        ImGui::OpenPopup("Sahneyi Ac##OpenSceneModal");
        s_ShowLoadPopup_Pending = false;
    }
    if (s_ShowNotificationPopup_Pending) {
        ImGui::OpenPopup("Bildirim##SceneNotification");
        s_ShowNotificationPopup_Pending = false;
    }

    // 1.1. Yeni Proje Oluştur Diyaloğu
    if (ImGui::BeginPopupModal("Yeni Proje Olustur##NewProjectModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Yeni bir AstralEngine projesi baslatin:");
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::Text("Proje Adi:");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##NewProjectName", s_NewProjectNameBuffer, sizeof(s_NewProjectNameBuffer));

        ImGui::Dummy(ImVec2(0, 2.0f));
        ImGui::Text("Proje Dizini:");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##NewProjectPath", s_NewProjectPathBuffer, sizeof(s_NewProjectPathBuffer));

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Olustur", ImVec2(120, 28))) {
            ExecuteNewProject(scene, selectedEntity, s_NewProjectPathBuffer, s_NewProjectNameBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Iptal", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 1.2. Projeyi Aç Diyaloğu (Non-Windows / Fallback)
    if (ImGui::BeginPopupModal("Projeyi Ac##OpenProjectModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Yuklenecek proje dosyasinin yolunu girin (.astralproj):");
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##OpenProjectPathInput", s_OpenProjectPathBuffer, sizeof(s_OpenProjectPathBuffer));

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Yukle", ImVec2(120, 28))) {
            ExecuteOpenProject(scene, selectedEntity, s_OpenProjectPathBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Iptal", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. Yeni Sahne Şablon Seçim Diyaloğu (Unreal Engine Standartı)
    if (ImGui::BeginPopupModal("Yeni Sahne Onayi##NewSceneConfirmModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "YENI SAHNE SABLONU SECIN");
        ImGui::TextDisabled("Unreal Engine standardinda baslangic seviyesi secenekleri:");
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f));

        // 1. Kart: Standart Seviye (Basic Level)
        ImGui::BeginChild("BasicLevelCard", ImVec2(420, 100), true);
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "[1] Standart Seviye (Basic Level) - Onerilen");
        ImGui::TextWrapped("Zemin platformu (Floor) ve merkez baslangic kupu icerir. Hizli prototipleme ve genel tasarim icin idealdir.");
        ImGui::Dummy(ImVec2(0, 2.0f));
        if (ImGui::Button("Standart Seviye Olustur##BtnBasic", ImVec2(220, 26))) {
            ExecuteNewScene(scene, selectedEntity, SceneTemplate::Basic);
            actions.newScene = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0, 4.0f));

        // 2. Kart: Boş Seviye (Empty Level)
        ImGui::BeginChild("EmptyLevelCard", ImVec2(420, 100), true);
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "[2] Bos Seviye (Empty Level)");
        ImGui::TextWrapped("Tamamen bos sahne. Hicbir varlik icermez (0 nesne). Sifirdan ozel ortam kurmak icin.");
        ImGui::Dummy(ImVec2(0, 2.0f));
        if (ImGui::Button("Bos Seviye Olustur##BtnEmpty", ImVec2(220, 26))) {
            ExecuteNewScene(scene, selectedEntity, SceneTemplate::Empty);
            actions.newScene = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Iptal", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 3. Sahneyi Farklı Kaydet (Save As) Diyaloğu
    if (ImGui::BeginPopupModal("Sahneyi Farkli Kaydet##SaveAsModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Hedef dosya yolunu belirtin (.astral):");
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##SaveAsPathInput", s_PathBuffer, sizeof(s_PathBuffer));
        ImGui::SameLine();

#ifdef _WIN32
        if (ImGui::Button("Gozat...##BrowseSave")) {
            std::string selected = NativeSaveFileDialog();
            if (!selected.empty()) {
                std::strncpy(s_PathBuffer, selected.c_str(), sizeof(s_PathBuffer) - 1);
            }
        }
#endif

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Kaydet", ImVec2(120, 28))) {
            std::string pathStr(s_PathBuffer);
            if (!pathStr.empty()) {
                ExecuteSave(scene, pathStr);
                actions.saveScene = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Iptal", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 4. Sahneyi Aç (Open Scene) Diyaloğu
    if (ImGui::BeginPopupModal("Sahneyi Ac##OpenSceneModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Yuklenecek sahne dosyasini secin (.astral):");
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##OpenPathInput", s_OpenPathBuffer, sizeof(s_OpenPathBuffer));
        ImGui::SameLine();

#ifdef _WIN32
        if (ImGui::Button("Gozat...##BrowseOpen")) {
            std::string selected = NativeOpenFileDialog();
            if (!selected.empty()) {
                std::strncpy(s_OpenPathBuffer, selected.c_str(), sizeof(s_OpenPathBuffer) - 1);
            }
        }
#endif

        // Mevcut scenes klasöründeki dosyaları listele
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::TextDisabled("Mevcut Sahneler (assets/scenes):");
        ImGui::BeginChild("AvailableScenesList", ImVec2(440, 140), true);

        try {
            std::filesystem::path scenesDir("assets/scenes");
            if (std::filesystem::exists(scenesDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(scenesDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".astral") {
                        std::string filename = entry.path().filename().string();
                        std::string fullPath = entry.path().string();
                        bool isSelected = (fullPath == std::string(s_OpenPathBuffer));

                        if (ImGui::Selectable(filename.c_str(), isSelected)) {
                            std::strncpy(s_OpenPathBuffer, fullPath.c_str(), sizeof(s_OpenPathBuffer) - 1);
                        }

                        // Çift tıklamayla doğrudan aç
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            ExecuteOpen(scene, selectedEntity, fullPath);
                            actions.openScene = true;
                            ImGui::CloseCurrentPopup();
                            break;
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("assets/scenes dizini bulunamadi.");
            }
        } catch (...) {
            ImGui::TextDisabled("Dizin taranirken hata olustu.");
        }

        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Ac (Open)", ImVec2(120, 28))) {
            std::string pathStr(s_OpenPathBuffer);
            if (!pathStr.empty()) {
                ExecuteOpen(scene, selectedEntity, pathStr);
                actions.openScene = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Iptal", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 5. Bildirim / Hata Toast Diyaloğu
    if (ImGui::BeginPopupModal("Bildirim##SceneNotification", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (s_NotificationIsError) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "[HATA] %s", s_NotificationTitle.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "[BILGI] %s", s_NotificationTitle.c_str());
        }
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::TextUnformatted(s_NotificationMessage.c_str());
        ImGui::Dummy(ImVec2(0, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (ImGui::Button("Tamam", ImVec2(100, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 6. About Modal Diyalogu
    if (s_ShowAboutModal_Pending) {
        ImGui::OpenPopup("Astral Engine Hakkinda##AboutModal");
        s_ShowAboutModal_Pending = false;
    }

    if (ImGui::BeginPopupModal("Astral Engine Hakkinda##AboutModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Astral Engine v1.0.0");
        ImGui::TextDisabled("Vulkan 1.4 Dynamic Rendering & Signed Distance Field (SDF) Compute Raymarcher");
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f));

        ImGui::Text("Mimari Bilesenler:");
        ImGui::BulletText("Vulkan 1.4 Compute Raymarcher & Dinamik SSBO Edit Buffer (MAX_SDF_EDITS = 256)");
        ImGui::BulletText("Two-Level BrickGrid (32x16x32) Hizlandirma Yapisi & AABB Dirty Tracking");
        ImGui::BulletText("Data-Oriented Design (DOD) ECS Mimarisi & Generational Entity Handles");
        ImGui::BulletText("DOD Binary Scene Serializer v2 (Sihirli baslik, CRC32, Chunk formati)");
        ImGui::BulletText("Temporal Anti-Aliasing (Halton Jitter Subpixel + 3x3 Clamping)");
        ImGui::BulletText("Command-Stack Tabanli Geri Al / Yeniden Yap (Undo / Redo)");

        ImGui::Dummy(ImVec2(0, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f));

        ImGui::Text("Kutuphaneler ve Standartlar:");
        ImGui::TextDisabled("  - Vulkan SDK 1.4 | C++20 | GLSL Compute Shaders");
        ImGui::TextDisabled("  - GLFW 3.4 | Dear ImGui (Docking) | ImGuizmo | GLM");

        ImGui::Dummy(ImVec2(0, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f));

        if (ImGui::Button("Kapat", ImVec2(120, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Astral
