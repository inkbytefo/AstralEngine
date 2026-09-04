#include "Astral/Editor/EditorMenuBar.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Scene/SceneSerializer.hpp"

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

static std::string s_NotificationTitle = "";
static std::string s_NotificationMessage = "";
static bool s_NotificationIsError = false;

// Flags for delayed ImGui::OpenPopup calls
static bool s_ShowNotificationPopup_Pending = false;
static bool s_ShowNewConfirmPopup_Pending = false;
static bool s_ShowSaveAsPopup_Pending = false;
static bool s_ShowLoadPopup_Pending = false;

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

static void ExecuteNewScene(Scene& scene, Entity& selectedEntity) {
    scene.Clear();
    selectedEntity = Entity();
    s_CurrentScenePath.clear();
    scene.SetName("Untitled Scene");
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
                       const InputSystem& input) {
    ImGuiIO& io = ImGui::GetIO();

    // ── Keyboard Shortcuts (Ctrl+S, Ctrl+O, Ctrl+N) ───────────
    if (!io.WantTextInput) {
        bool ctrlPressed = input.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || input.IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
        bool shiftPressed = input.IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || input.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);

        if (ctrlPressed && !shiftPressed && input.IsKeyJustPressed(GLFW_KEY_S)) {
            if (s_CurrentScenePath.empty()) {
                s_ShowSaveAsPopup_Pending = true;
            } else {
                ExecuteSave(scene, s_CurrentScenePath);
                actions.saveScene = true;
            }
        } else if (ctrlPressed && shiftPressed && input.IsKeyJustPressed(GLFW_KEY_S)) {
            s_ShowSaveAsPopup_Pending = true;
        } else if (ctrlPressed && input.IsKeyJustPressed(GLFW_KEY_O)) {
            s_ShowLoadPopup_Pending = true;
        } else if (ctrlPressed && input.IsKeyJustPressed(GLFW_KEY_N)) {
            if (scene.GetRegistry().GetAliveEntityCount() > 0) {
                s_ShowNewConfirmPopup_Pending = true;
            } else {
                ExecuteNewScene(scene, selectedEntity);
                actions.newScene = true;
            }
        }
    }

    if (ImGui::BeginMenuBar()) {
        // ── File ──────────────────────────────────────────────────
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Yeni Sahne", "Ctrl+N")) {
                if (scene.GetRegistry().GetAliveEntityCount() > 0) {
                    s_ShowNewConfirmPopup_Pending = true;
                } else {
                    ExecuteNewScene(scene, selectedEntity);
                    actions.newScene = true;
                }
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
            if (ImGui::MenuItem("Geri Al", "Ctrl+Z", false, false)) {
                // Placeholder: undo
            }
            if (ImGui::MenuItem("Yeniden Yap", "Ctrl+Y", false, false)) {
                // Placeholder: redo
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
            if (ImGui::MenuItem("Astral Engine Hakkinda")) {
                ShowNotification("Astral Engine v1.0.0",
                                 "Vulkan 1.4 Dynamic Rendering & Signed Distance Field (SDF) Compute Raymarcher Engine.\n"
                                 "Data-Oriented Design (DOD) ECS mimarisi ve yuksek basarimli ikili serilestirici.",
                                 false);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // ========================================================================
    // Modal Dialogs Handling
    // ========================================================================

    // 1. Trigger pending popups
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

    // 2. Yeni Sahne Onay Diyaloğu
    if (ImGui::BeginPopupModal("Yeni Sahne Onayi##NewSceneConfirmModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Mevcut sahnedeki tum varliklar temizlenecek.\nKaydedilmemis degisiklikler kaybolabilir.\n\nYeni sahne olusturmak istediginizden emin misiniz?");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f));

        if (ImGui::Button("Evet, Yeni Sahne Olustur", ImVec2(200, 28))) {
            ExecuteNewScene(scene, selectedEntity);
            actions.newScene = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
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
}

} // namespace Astral
