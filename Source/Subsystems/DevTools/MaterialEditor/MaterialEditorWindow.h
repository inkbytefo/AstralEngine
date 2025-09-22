#pragma once
#include "../Interfaces/IDeveloperTool.h"
#include "../Common/DevToolsTypes.h"
#include "../../../Subsystems/Asset/AssetHandle.h"
#include "../../../Subsystems/Asset/Material.h"
#include "../../../Subsystems/Renderer/Material/Material.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <any>

class Engine;
class RenderSubsystem;
class AssetSubsystem;
class VulkanRenderer;
class Camera;
class Scene;

namespace AstralEngine {

class MaterialEditorWindow : public IDeveloperTool {
public:
    struct MaterialPreview {
        AssetHandle materialHandle;
        std::unique_ptr<Camera> previewCamera;
        std::unique_ptr<Scene> previewScene;
        bool autoUpdate = true;
        float rotationSpeed = 30.0f;
        glm::vec3 lightPosition = glm::vec3(2.0f, 2.0f, 2.0f);
        glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        float lightIntensity = 1.0f;
    };
    
    struct ShaderUniform {
        std::string name;
        std::string type;
        std::any value;
        std::any minValue;
        std::any maxValue;
        std::string description;
        bool isDirty = false;
    };
    
    MaterialEditorWindow();
    ~MaterialEditorWindow() = default;
    
    // IDeveloperTool implementasyonu
    void OnInitialize() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
    
    const std::string& GetName() const override { return m_name; }
    bool IsEnabled() const override { return m_enabled; }
    void SetEnabled(bool enabled) override { m_enabled = enabled; }
    
    void LoadSettings(const std::string& settings) override;
    std::string SaveSettings() const override;
    
    // Materyal yönetimi
    void LoadMaterial(const AssetHandle& handle);
    void SaveMaterial();
    void SaveMaterialAs(const std::string& path);
    void CreateNewMaterial();
    void ReloadMaterial();
    
    // Materyal özellikleri
    void SetMaterialProperty(const std::string& property, const std::any& value);
    void SetMaterialTexture(const std::string& textureSlot, const AssetHandle& texture);
    void SetMaterialShader(const AssetHandle& shader);
    
    // Shader uniform düzenleme
    void SetShaderUniform(const std::string& uniformName, const std::any& value);
    void ResetShaderUniform(const std::string& uniformName);
    void ApplyShaderUniforms();
    
    // Preview ayarları
    void SetPreviewModel(const std::string& modelPath);
    void SetPreviewBackground(const glm::vec4& color);
    void SetPreviewLighting(const glm::vec3& position, const glm::vec3& color, float intensity);
    
    // Yardımcı metodlar
    bool IsMaterialLoaded() const { return m_currentMaterial.IsValid(); }
    const Material* GetCurrentMaterial() const;
    const AssetHandle& GetCurrentMaterialHandle() const { return m_currentMaterial; }
    
private:
    // Render metodları
    void RenderMaterialEditor();
    void RenderMaterialList();
    void RenderMaterialProperties();
    void RenderMaterialPreview();
    void RenderShaderUniforms();
    void RenderTextureSlots();
    void RenderToolbar();
    void RenderSettings();
    
    // Materyal işleme
    void UpdateMaterialProperties();
    void UpdateShaderUniforms();
    void UpdateTextureSlots();
    void CreatePreviewScene();
    void UpdatePreviewScene(float deltaTime);
    void UpdateAvailableMaterials();
    
    // Shader uniform render
    void RenderUniformValue(ShaderUniform& uniform);
    template<typename T>
    void RenderUniformValueImpl(ShaderUniform& uniform);
    
    // Texture slot render
    void RenderTextureSlot(const std::string& slotName, const AssetHandle& texture);
    
    // Preview rendering
    void RenderPreview(VkCommandBuffer commandBuffer);
    void CreatePreviewRenderPass();
    void CreatePreviewFramebuffer();
    void CreatePreviewPipeline();
    
    std::string m_name = "Material Editor";
    bool m_enabled = true;
    
    // Materyal verileri
    AssetHandle m_currentMaterial;
    MaterialPreview m_preview;
    std::unordered_map<std::string, ShaderUniform> m_shaderUniforms;
    std::unordered_map<std::string, AssetHandle> m_textureSlots;
    
    // Preview render objeleri
    VkRenderPass m_previewRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_previewFramebuffer = VK_NULL_HANDLE;
    VkPipeline m_previewPipeline = VK_NULL_HANDLE;
    VkImage m_previewImage = VK_NULL_HANDLE;
    VkImageView m_previewImageView = VK_NULL_HANDLE;
    VkDeviceMemory m_previewImageMemory = VK_NULL_HANDLE;
    VkSampler m_previewSampler = VK_NULL_HANDLE;
    VkExtent2D m_previewExtent = {512, 512};
    
    // Bağlantılar
    Engine* m_engine = nullptr;
    RenderSubsystem* m_renderSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    VulkanRenderer* m_vulkanRenderer = nullptr;
    
    // UI durumu
    bool m_showMaterialList = true;
    bool m_showMaterialProperties = true;
    bool m_showMaterialPreview = true;
    bool m_showShaderUniforms = true;
    bool m_showTextureSlots = true;
    bool m_showSettings = false;
    
    // Materyal listesi
    std::vector<AssetHandle> m_availableMaterials;
    std::string m_materialFilter = "";
    int m_selectedMaterialIndex = -1;
    
    // Ayarlar
    bool m_autoSave = false;
    bool m_hotReload = true;
    float m_autoSaveInterval = 30.0f;
    float m_timeSinceLastSave = 0.0f;
    bool m_showGrid = true;
    bool m_showAxes = true;
    glm::vec4 m_backgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Performans
    float m_timeSinceLastUpdate = 0.0f;
    float m_updateInterval = 0.1f;
    
    // Geçici dosya yönetimi
    std::string m_tempMaterialPath;
    bool m_hasUnsavedChanges = false;
};

// Template implementasyonları
template<typename T>
void MaterialEditorWindow::RenderUniformValueImpl(ShaderUniform& uniform) {
    T value = std::any_cast<T>(uniform.value);
    T minVal = std::any_cast<T>(uniform.minValue);
    T maxVal = std::any_cast<T>(uniform.maxValue);
    
    if constexpr (std::is_same_v<T, float>) {
        if (ImGui::SliderFloat(uniform.name.c_str(), &value, minVal, maxVal)) {
            uniform.value = value;
            uniform.isDirty = true;
        }
    } else if constexpr (std::is_same_v<T, int>) {
        if (ImGui::SliderInt(uniform.name.c_str(), &value, minVal, maxVal)) {
            uniform.value = value;
            uniform.isDirty = true;
        }
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        if (ImGui::SliderFloat2(uniform.name.c_str(), &value[0], minVal.x, maxVal.x)) {
            uniform.value = value;
            uniform.isDirty = true;
        }
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        if (ImGui::SliderFloat3(uniform.name.c_str(), &value[0], minVal.x, maxVal.x)) {
            uniform.value = value;
            uniform.isDirty = true;
        }
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
        if (ImGui::ColorEdit4(uniform.name.c_str(), &value[0])) {
            uniform.value = value;
            uniform.isDirty = true;
        }
    } else {
        ImGui::Text("Unsupported uniform type: %s", uniform.type.c_str());
    }
    
    if (uniform.isDirty) {
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            SetShaderUniform(uniform.name, uniform.value);
            uniform.isDirty = false;
        }
    }
}

} // namespace AstralEngine

// ImGui toolbar helper fonksiyonları
namespace ImGui {
    void BeginToolbar(const char* str_id);
    void EndToolbar();
    bool ToolbarButton(const char* label, const char* tooltip = nullptr);
    void ToolbarSeparator();
    bool ToolbarToggle(const char* label, bool* v, const char* tooltip = nullptr);
}