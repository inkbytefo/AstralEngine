#include "MaterialEditorWindow.h"
#include "../../../Core/Engine.h"
#include "../../../Core/Logger.h"
#include "../../../Subsystems/Asset/AssetSubsystem.h"
#include "../../../Subsystems/Asset/AssetManager.h"
#include "../../../Subsystems/Renderer/RenderSubsystem.h"
#include "../../../Subsystems/Renderer/VulkanRenderer.h"
#include "../../../Subsystems/Renderer/Camera.h"
#include "../../../Subsystems/ECS/ECSSubsystem.h"
#include "../../../Subsystems/ECS/Components.h"
#include "../../../Subsystems/ECS/Scene.h"
#include <imgui.h>
#include <fstream>
#include <sstream>

namespace AstralEngine {

MaterialEditorWindow::MaterialEditorWindow() {
    // Constructor implementation
}

void MaterialEditorWindow::OnInitialize() {
    Logger::Info("MaterialEditor", "MaterialEditorWindow başlatılıyor");
    
    // Engine ve subsystem'leri al
    m_engine = Engine::GetInstance();
    if (!m_engine) {
        Logger::Error("MaterialEditor", "Engine örneği alınamadı");
        return;
    }
    
    m_renderSubsystem = m_engine->GetSubsystem<RenderSubsystem>();
    m_assetSubsystem = m_engine->GetSubsystem<AssetSubsystem>();
    
    if (!m_renderSubsystem) {
        Logger::Error("MaterialEditor", "RenderSubsystem alınamadı");
        return;
    }
    
    if (!m_assetSubsystem) {
        Logger::Error("MaterialEditor", "AssetSubsystem alınamadı");
        return;
    }
    
    // VulkanRenderer'ı al
    m_vulkanRenderer = m_renderSubsystem->GetGraphicsDevice()->GetVulkanRenderer();
    
    // Preview render kaynaklarını oluştur
    CreatePreviewRenderPass();
    CreatePreviewFramebuffer();
    CreatePreviewPipeline();
    
    // Preview sahnesini oluştur
    CreatePreviewScene();
    
    // Mevcut materyalleri yükle
    UpdateAvailableMaterials();
    
    Logger::Info("MaterialEditor", "MaterialEditorWindow başarıyla başlatıldı");
}

void MaterialEditorWindow::OnUpdate(float deltaTime) {
    if (!m_enabled) {
        return;
    }
    
    // Auto-save kontrolü
    if (m_autoSave && m_hasUnsavedChanges) {
        m_timeSinceLastSave += deltaTime;
        if (m_timeSinceLastSave >= m_autoSaveInterval) {
            SaveMaterial();
            m_timeSinceLastSave = 0.0f;
        }
    }
    
    // Preview sahnesini güncelle
    if (m_preview.previewScene && m_preview.autoUpdate) {
        UpdatePreviewScene(deltaTime);
    }
    
    // Performans optimizasyonu için güncelleme aralığı
    m_timeSinceLastUpdate += deltaTime;
    if (m_timeSinceLastUpdate >= m_updateInterval) {
        // Materyal listesini güncelle
        UpdateAvailableMaterials();
        m_timeSinceLastUpdate = 0.0f;
    }
}

void MaterialEditorWindow::OnRender() {
    if (!m_enabled) {
        return;
    }
    
    RenderMaterialEditor();
}

void MaterialEditorWindow::OnShutdown() {
    Logger::Info("MaterialEditor", "MaterialEditorWindow kapatılıyor");
    
    // Preview render kaynaklarını temizle
    if (m_previewRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewRenderPass, nullptr);
        m_previewRenderPass = VK_NULL_HANDLE;
    }
    
    if (m_previewFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewFramebuffer, nullptr);
        m_previewFramebuffer = VK_NULL_HANDLE;
    }
    
    if (m_previewPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewPipeline, nullptr);
        m_previewPipeline = VK_NULL_HANDLE;
    }
    
    if (m_previewImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewImageView, nullptr);
        m_previewImageView = VK_NULL_HANDLE;
    }
    
    if (m_previewImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewImage, nullptr);
        m_previewImage = VK_NULL_HANDLE;
    }
    
    if (m_previewImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewImageMemory, nullptr);
        m_previewImageMemory = VK_NULL_HANDLE;
    }
    
    if (m_previewSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_renderSubsystem->GetGraphicsDevice()->GetDevice(), m_previewSampler, nullptr);
        m_previewSampler = VK_NULL_HANDLE;
    }
    
    // Preview sahnesini temizle
    m_preview.previewScene.reset();
    m_preview.previewCamera.reset();
    
    Logger::Info("MaterialEditor", "MaterialEditorWindow başarıyla kapatıldı");
}

void MaterialEditorWindow::LoadSettings(const std::string& settings) {
    // Ayarları yükle
    // JSON formatında ayarları parse et
    try {
        // TODO: JSON parsing implementasyonu
        Logger::Info("MaterialEditor", "Ayarlar yüklendi");
    } catch (const std::exception& e) {
        Logger::Error("MaterialEditor", "Ayarlar yüklenirken hata: {}", e.what());
    }
}

std::string MaterialEditorWindow::SaveSettings() const {
    // Ayarları kaydet
    // JSON formatında ayarları serialize et
    try {
        // TODO: JSON serialization implementasyonu
        Logger::Info("MaterialEditor", "Ayarlar kaydedildi");
        return "{}"; // Placeholder
    } catch (const std::exception& e) {
        Logger::Error("MaterialEditor", "Ayarlar kaydedilirken hata: {}", e.what());
        return "{}";
    }
}

void MaterialEditorWindow::LoadMaterial(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        Logger::Error("MaterialEditor", "Geçersiz materyal handle");
        return;
    }
    
    m_currentMaterial = handle;
    m_hasUnsavedChanges = false;
    
    // Materyal verilerini yükle
    const Material* material = GetCurrentMaterial();
    if (material) {
        // Shader uniform'leri yükle
        UpdateShaderUniforms();
        
        // Texture slot'ları yükle
        UpdateTextureSlots();
        
        // Preview'i güncelle
        CreatePreviewScene();
        
        Logger::Info("MaterialEditor", "Materyal yüklendi: {}", handle.GetPath());
    } else {
        Logger::Error("MaterialEditor", "Materyal yüklenemedi: {}", handle.GetPath());
    }
}

void MaterialEditorWindow::SaveMaterial() {
    if (!IsMaterialLoaded()) {
        Logger::Warning("MaterialEditor", "Kaydedilecek materyal yüklenmedi");
        return;
    }
    
    // Materyal verilerini kaydet
    ApplyShaderUniforms();
    
    // Asset subsystem üzerinden kaydet
    if (m_assetSubsystem) {
        auto assetManager = m_assetSubsystem->GetAssetManager();
        if (assetManager) {
            // TODO: AssetManager üzerinden materyali kaydetme implementasyonu
            m_hasUnsavedChanges = false;
            Logger::Info("MaterialEditor", "Materyal kaydedildi: {}", m_currentMaterial.GetPath());
        }
    }
}

void MaterialEditorWindow::SaveMaterialAs(const std::string& path) {
    if (!IsMaterialLoaded()) {
        Logger::Warning("MaterialEditor", "Kaydedilecek materyal yüklenmedi");
        return;
    }
    
    // Materyali yeni path'e kaydet
    // TODO: SaveAs implementasyonu
    m_hasUnsavedChanges = false;
    Logger::Info("MaterialEditor", "Materyal farklı kaydedildi: {}", path);
}

void MaterialEditorWindow::CreateNewMaterial() {
    // Yeni materyal oluştur
    Material::Config config;
    config.type = MaterialType::PBR;
    config.name = "NewMaterial";
    
    // TODO: Yeni materyal oluşturma implementasyonu
    m_hasUnsavedChanges = true;
    Logger::Info("MaterialEditor", "Yeni materyal oluşturuldu");
}

void MaterialEditorWindow::ReloadMaterial() {
    if (!IsMaterialLoaded()) {
        Logger::Warning("MaterialEditor", "Yeniden yüklenecek materyal yüklenmedi");
        return;
    }
    
    // Materyali yeniden yükle
    LoadMaterial(m_currentMaterial);
    Logger::Info("MaterialEditor", "Materyal yeniden yüklendi: {}", m_currentMaterial.GetPath());
}

void MaterialEditorWindow::SetMaterialProperty(const std::string& property, const std::any& value) {
    if (!IsMaterialLoaded()) {
        return;
    }
    
    // Materyal özelliğini ayarla
    // TODO: Materyal özelliği ayarlama implementasyonu
    m_hasUnsavedChanges = true;
}

void MaterialEditorWindow::SetMaterialTexture(const std::string& textureSlot, const AssetHandle& texture) {
    if (!IsMaterialLoaded()) {
        return;
    }
    
    // Materyal texture'ını ayarla
    m_textureSlots[textureSlot] = texture;
    m_hasUnsavedChanges = true;
}

void MaterialEditorWindow::SetMaterialShader(const AssetHandle& shader) {
    if (!IsMaterialLoaded()) {
        return;
    }
    
    // Materyal shader'ını ayarla
    // TODO: Shader ayarlama implementasyonu
    m_hasUnsavedChanges = true;
}

void MaterialEditorWindow::SetShaderUniform(const std::string& uniformName, const std::any& value) {
    auto it = m_shaderUniforms.find(uniformName);
    if (it != m_shaderUniforms.end()) {
        it->second.value = value;
        it->second.isDirty = false;
        m_hasUnsavedChanges = true;
    }
}

void MaterialEditorWindow::ResetShaderUniform(const std::string& uniformName) {
    auto it = m_shaderUniforms.find(uniformName);
    if (it != m_shaderUniforms.end()) {
        // Varsayılan değere geri dön
        // TODO: Varsayılan değer implementasyonu
        it->second.isDirty = false;
        m_hasUnsavedChanges = true;
    }
}

void MaterialEditorWindow::ApplyShaderUniforms() {
    // Tüm shader uniform'larını uygula
    for (auto& [name, uniform] : m_shaderUniforms) {
        if (uniform.isDirty) {
            SetShaderUniform(name, uniform.value);
        }
    }
}

void MaterialEditorWindow::SetPreviewModel(const std::string& modelPath) {
    // Preview modelini ayarla
    // TODO: Preview modeli ayarlama implementasyonu
}

void MaterialEditorWindow::SetPreviewBackground(const glm::vec4& color) {
    m_backgroundColor = color;
}

void MaterialEditorWindow::SetPreviewLighting(const glm::vec3& position, const glm::vec3& color, float intensity) {
    m_preview.lightPosition = position;
    m_preview.lightColor = color;
    m_preview.lightIntensity = intensity;
}

const Material* MaterialEditorWindow::GetCurrentMaterial() const {
    if (!m_currentMaterial.IsValid()) {
        return nullptr;
    }
    
    if (m_assetSubsystem) {
        auto assetManager = m_assetSubsystem->GetAssetManager();
        if (assetManager) {
            return assetManager->GetAsset<Material>(m_currentMaterial).get();
        }
    }
    
    return nullptr;
}

void MaterialEditorWindow::RenderMaterialEditor() {
    if (!ImGui::Begin("Material Editor", &m_enabled)) {
        ImGui::End();
        return;
    }
    
    // Toolbar
    RenderToolbar();
    
    // Ana içerik bölmesi
    ImGui::Columns(2, "MaterialEditorColumns", true);
    
    // Sol kolon - Materyal listesi ve özellikleri
    if (m_showMaterialList) {
        RenderMaterialList();
    }
    
    if (m_showMaterialProperties) {
        RenderMaterialProperties();
    }
    
    ImGui::NextColumn();
    
    // Sağ kolon - Preview ve shader uniform'ları
    if (m_showMaterialPreview) {
        RenderMaterialPreview();
    }
    
    if (m_showShaderUniforms) {
        RenderShaderUniforms();
    }
    
    if (m_showTextureSlots) {
        RenderTextureSlots();
    }
    
    ImGui::Columns(1);
    
    // Ayarlar penceresi
    if (m_showSettings) {
        RenderSettings();
    }
    
    ImGui::End();
}

void MaterialEditorWindow::RenderToolbar() {
    ImGui::BeginToolbar("MaterialEditorToolbar");
    
    // Yeni materyal oluştur
    if (ImGui::Button("New")) {
        CreateNewMaterial();
    }
    ImGui::SameLine();
    
    // Materyal yükle
    if (ImGui::Button("Load")) {
        // TODO: Materyal yükleme dialog'u
    }
    ImGui::SameLine();
    
    // Materyali kaydet
    if (ImGui::Button("Save")) {
        SaveMaterial();
    }
    ImGui::SameLine();
    
    // Farklı kaydet
    if (ImGui::Button("Save As")) {
        // TODO: Farklı kaydet dialog'u
    }
    ImGui::SameLine();
    
    // Yeniden yükle
    if (ImGui::Button("Reload")) {
        ReloadMaterial();
    }
    ImGui::SameLine();
    
    // Ayarlar
    if (ImGui::Button("Settings")) {
        m_showSettings = !m_showSettings;
    }
    
    ImGui::EndToolbar();
}

void MaterialEditorWindow::RenderMaterialList() {
    if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    // Filtre
    ImGui::InputText("Filter", &m_materialFilter);
    
    // Materyal listesi
    ImGui::BeginChild("MaterialList", ImVec2(0, 200), true);
    
    for (size_t i = 0; i < m_availableMaterials.size(); ++i) {
        const auto& handle = m_availableMaterials[i];
        
        // Filtre kontrolü
        if (!m_materialFilter.empty()) {
            std::string path = handle.GetPath();
            if (path.find(m_materialFilter) == std::string::npos) {
                continue;
            }
        }
        
        bool isSelected = (m_selectedMaterialIndex == static_cast<int>(i));
        if (ImGui::Selectable(handle.GetPath().c_str(), isSelected)) {
            m_selectedMaterialIndex = static_cast<int>(i);
            LoadMaterial(handle);
        }
    }
    
    ImGui::EndChild();
}

void MaterialEditorWindow::RenderMaterialProperties() {
    if (!ImGui::CollapsingHeader("Material Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    if (!IsMaterialLoaded()) {
        ImGui::Text("No material loaded");
        return;
    }
    
    const Material* material = GetCurrentMaterial();
    if (!material) {
        ImGui::Text("Failed to get material");
        return;
    }
    
    // Materyal özelliklerini göster
    const auto& props = material->GetProperties();
    
    // Base Color
    glm::vec3 baseColor = props.baseColor;
    if (ImGui::ColorEdit3("Base Color", &baseColor[0])) {
        SetMaterialProperty("baseColor", baseColor);
    }
    
    // Metallic
    float metallic = props.metallic;
    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
        SetMaterialProperty("metallic", metallic);
    }
    
    // Roughness
    float roughness = props.roughness;
    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
        SetMaterialProperty("roughness", roughness);
    }
    
    // Ambient Occlusion
    float ao = props.ao;
    if (ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f)) {
        SetMaterialProperty("ao", ao);
    }
    
    // Opacity
    float opacity = props.opacity;
    if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f)) {
        SetMaterialProperty("opacity", opacity);
    }
    
    // Transparent
    bool transparent = props.transparent;
    if (ImGui::Checkbox("Transparent", &transparent)) {
        SetMaterialProperty("transparent", transparent);
    }
    
    // Double Sided
    bool doubleSided = props.doubleSided;
    if (ImGui::Checkbox("Double Sided", &doubleSided)) {
        SetMaterialProperty("doubleSided", doubleSided);
    }
}

void MaterialEditorWindow::RenderMaterialPreview() {
    if (!ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    // Preview boyutu
    ImVec2 previewSize = ImVec2(m_previewExtent.width, m_previewExtent.height);
    
    // Preview image
    // TODO: Preview texture render implementasyonu
    ImGui::Image(nullptr, previewSize);
    
    // Preview kontrolleri
    if (ImGui::Button("Reset Camera")) {
        if (m_preview.previewCamera) {
            m_preview.previewCamera->SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
            m_preview.previewCamera->SetLookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        }
    }
    ImGui::SameLine();
    
    bool autoUpdate = m_preview.autoUpdate;
    if (ImGui::Checkbox("Auto Update", &autoUpdate)) {
        m_preview.autoUpdate = autoUpdate;
    }
    
    // Rotation speed
    float rotationSpeed = m_preview.rotationSpeed;
    if (ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 180.0f)) {
        m_preview.rotationSpeed = rotationSpeed;
    }
}

void MaterialEditorWindow::RenderShaderUniforms() {
    if (!ImGui::CollapsingHeader("Shader Uniforms", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    if (m_shaderUniforms.empty()) {
        ImGui::Text("No shader uniforms available");
        return;
    }
    
    for (auto& [name, uniform] : m_shaderUniforms) {
        ImGui::PushID(name.c_str());
        
        if (uniform.type == "float") {
            RenderUniformValueImpl<float>(uniform);
        } else if (uniform.type == "int") {
            RenderUniformValueImpl<int>(uniform);
        } else if (uniform.type == "vec2") {
            RenderUniformValueImpl<glm::vec2>(uniform);
        } else if (uniform.type == "vec3") {
            RenderUniformValueImpl<glm::vec3>(uniform);
        } else if (uniform.type == "vec4") {
            RenderUniformValueImpl<glm::vec4>(uniform);
        } else {
            ImGui::Text("Unsupported uniform type: %s", uniform.type.c_str());
        }
        
        if (!uniform.description.empty()) {
            ImGui::SameLine();
            ImGui::HelpMarker(uniform.description.c_str());
        }
        
        ImGui::PopID();
    }
}

void MaterialEditorWindow::RenderTextureSlots() {
    if (!ImGui::CollapsingHeader("Texture Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    if (!IsMaterialLoaded()) {
        ImGui::Text("No material loaded");
        return;
    }
    
    // Texture slot'larını göster
    for (const auto& [slotName, textureHandle] : m_textureSlots) {
        RenderTextureSlot(slotName, textureHandle);
    }
}

void MaterialEditorWindow::RenderSettings() {
    if (!ImGui::Begin("Material Editor Settings", &m_showSettings)) {
        ImGui::End();
        return;
    }
    
    // Auto-save
    ImGui::Checkbox("Auto Save", &m_autoSave);
    if (m_autoSave) {
        ImGui::SliderFloat("Auto Save Interval", &m_autoSaveInterval, 10.0f, 300.0f);
    }
    
    // Hot reload
    ImGui::Checkbox("Hot Reload", &m_hotReload);
    
    // Preview ayarları
    ImGui::Separator();
    ImGui::Text("Preview Settings");
    
    ImGui::Checkbox("Show Grid", &m_showGrid);
    ImGui::Checkbox("Show Axes", &m_showAxes);
    
    // Background color
    ImGui::ColorEdit4("Background Color", &m_backgroundColor[0]);
    
    // Update interval
    ImGui::SliderFloat("Update Interval", &m_updateInterval, 0.01f, 1.0f);
    
    ImGui::End();
}

void MaterialEditorWindow::UpdateMaterialProperties() {
    const Material* material = GetCurrentMaterial();
    if (!material) return;
    
    // Materyal özelliklerini shader uniform'larına aktar
    const auto& props = material->GetProperties();
    
    // Base Color uniform'unu güncelle
    auto baseColorIt = m_shaderUniforms.find("baseColor");
    if (baseColorIt != m_shaderUniforms.end()) {
        baseColorIt->second.value = props.baseColor;
    }
    
    // Metallic uniform'unu güncelle
    auto metallicIt = m_shaderUniforms.find("metallic");
    if (metallicIt != m_shaderUniforms.end()) {
        metallicIt->second.value = props.metallic;
    }
    
    // Roughness uniform'unu güncelle
    auto roughnessIt = m_shaderUniforms.find("roughness");
    if (roughnessIt != m_shaderUniforms.end()) {
        roughnessIt->second.value = props.roughness;
    }
    
    // Ambient Occlusion uniform'unu güncelle
    auto aoIt = m_shaderUniforms.find("ao");
    if (aoIt != m_shaderUniforms.end()) {
        aoIt->second.value = props.ao;
    }
    
    // Opacity uniform'unu güncelle
    auto opacityIt = m_shaderUniforms.find("opacity");
    if (opacityIt != m_shaderUniforms.end()) {
        opacityIt->second.value = props.opacity;
    }
}

void MaterialEditorWindow::UpdateShaderUniforms() {
    const Material* material = GetCurrentMaterial();
    if (!material) return;
    
    m_shaderUniforms.clear();
    
    // Shader programından uniform bilgilerini al
    // TODO: Shader uniform reflection implementasyonu
    // Şimdilik standart PBR materyal uniform'larını ekle
    
    const auto& props = material->GetProperties();
    
    // Base Color
    ShaderUniform baseColorUniform;
    baseColorUniform.name = "baseColor";
    baseColorUniform.type = "vec3";
    baseColorUniform.value = props.baseColor;
    baseColorUniform.minValue = glm::vec3(0.0f);
    baseColorUniform.maxValue = glm::vec3(1.0f);
    baseColorUniform.description = "Base color of the material";
    m_shaderUniforms["baseColor"] = baseColorUniform;
    
    // Metallic
    ShaderUniform metallicUniform;
    metallicUniform.name = "metallic";
    metallicUniform.type = "float";
    metallicUniform.value = props.metallic;
    metallicUniform.minValue = 0.0f;
    metallicUniform.maxValue = 1.0f;
    metallicUniform.description = "Metallic property of the material";
    m_shaderUniforms["metallic"] = metallicUniform;
    
    // Roughness
    ShaderUniform roughnessUniform;
    roughnessUniform.name = "roughness";
    roughnessUniform.type = "float";
    roughnessUniform.value = props.roughness;
    roughnessUniform.minValue = 0.0f;
    roughnessUniform.maxValue = 1.0f;
    roughnessUniform.description = "Roughness property of the material";
    m_shaderUniforms["roughness"] = roughnessUniform;
    
    // Ambient Occlusion
    ShaderUniform aoUniform;
    aoUniform.name = "ao";
    aoUniform.type = "float";
    aoUniform.value = props.ao;
    aoUniform.minValue = 0.0f;
    aoUniform.maxValue = 1.0f;
    aoUniform.description = "Ambient occlusion property of the material";
    m_shaderUniforms["ao"] = aoUniform;
    
    // Opacity
    ShaderUniform opacityUniform;
    opacityUniform.name = "opacity";
    opacityUniform.type = "float";
    opacityUniform.value = props.opacity;
    opacityUniform.minValue = 0.0f;
    opacityUniform.maxValue = 1.0f;
    opacityUniform.description = "Opacity property of the material";
    m_shaderUniforms["opacity"] = opacityUniform;
    
    // Eğer materyal transparent ise, alpha uniform'u ekle
    if (props.transparent) {
        ShaderUniform alphaUniform;
        alphaUniform.name = "alpha";
        alphaUniform.type = "float";
        alphaUniform.value = props.opacity;
        alphaUniform.minValue = 0.0f;
        alphaUniform.maxValue = 1.0f;
        alphaUniform.description = "Alpha transparency value";
        m_shaderUniforms["alpha"] = alphaUniform;
    }
    
    // Işık pozisyonu (preview için)
    ShaderUniform lightPosUniform;
    lightPosUniform.name = "lightPosition";
    lightPosUniform.type = "vec3";
    lightPosUniform.value = m_preview.lightPosition;
    lightPosUniform.minValue = glm::vec3(-10.0f);
    lightPosUniform.maxValue = glm::vec3(10.0f);
    lightPosUniform.description = "Light position for preview";
    m_shaderUniforms["lightPosition"] = lightPosUniform;
    
    // Işık rengi (preview için)
    ShaderUniform lightColorUniform;
    lightColorUniform.name = "lightColor";
    lightColorUniform.type = "vec3";
    lightColorUniform.value = m_preview.lightColor;
    lightColorUniform.minValue = glm::vec3(0.0f);
    lightColorUniform.maxValue = glm::vec3(1.0f);
    lightColorUniform.description = "Light color for preview";
    m_shaderUniforms["lightColor"] = lightColorUniform;
    
    // Işık yoğunluğu (preview için)
    ShaderUniform lightIntensityUniform;
    lightIntensityUniform.name = "lightIntensity";
    lightIntensityUniform.type = "float";
    lightIntensityUniform.value = m_preview.lightIntensity;
    lightIntensityUniform.minValue = 0.0f;
    lightIntensityUniform.maxValue = 10.0f;
    lightIntensityUniform.description = "Light intensity for preview";
    m_shaderUniforms["lightIntensity"] = lightIntensityUniform;
}

void MaterialEditorWindow::UpdateTextureSlots() {
    const Material* material = GetCurrentMaterial();
    if (!material) return;
    
    m_textureSlots.clear();
    
    // Texture slot'larını güncelle
    const auto& textures = material->GetTextures();
    
    // Standart PBR texture slot'larını ekle
    if (textures.find("albedo") != textures.end()) {
        m_textureSlots["albedo"] = textures.at("albedo");
    }
    
    if (textures.find("normal") != textures.end()) {
        m_textureSlots["normal"] = textures.at("normal");
    }
    
    if (textures.find("metallic") != textures.end()) {
        m_textureSlots["metallic"] = textures.at("metallic");
    }
    
    if (textures.find("roughness") != textures.end()) {
        m_textureSlots["roughness"] = textures.at("roughness");
    }
    
    if (textures.find("ao") != textures.end()) {
        m_textureSlots["ao"] = textures.at("ao");
    }
    
    if (textures.find("emissive") != textures.end()) {
        m_textureSlots["emissive"] = textures.at("emissive");
    }
    
    if (textures.find("opacity") != textures.end()) {
        m_textureSlots["opacity"] = textures.at("opacity");
    }
    
    // Eğer materyal transparent ise, opacity mask texture'ını ekle
    if (material->GetProperties().transparent && textures.find("opacityMask") != textures.end()) {
        m_textureSlots["opacityMask"] = textures.at("opacityMask");
    }
    
    // Custom texture slot'larını da ekle
    for (const auto& [slotName, textureHandle] : textures) {
        // Standart slot'lar zaten eklendi, sadece custom olanları ekle
        if (slotName != "albedo" && slotName != "normal" && slotName != "metallic" &&
            slotName != "roughness" && slotName != "ao" && slotName != "emissive" &&
            slotName != "opacity" && slotName != "opacityMask") {
            m_textureSlots[slotName] = textureHandle;
        }
    }
}

void MaterialEditorWindow::CreatePreviewScene() {
    // Preview sahnesini oluştur
    m_preview.previewScene = std::make_unique<Scene>();
    
    // Preview kamerasını ayarla
    m_preview.previewCamera = std::make_unique<Camera>();
    m_preview.previewCamera->SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
    m_preview.previewCamera->SetLookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    m_preview.previewCamera->SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    
    // Preview objesini ekle (sphere)
    // TODO: Preview objesi ekleme implementasyonu
    // Şimdilik placeholder olarak boş bir obje oluştur
    if (m_assetSubsystem) {
        auto assetManager = m_assetSubsystem->GetAssetManager();
        if (assetManager) {
            // Varsayılan bir sphere modeli yükle
            // AssetHandle sphereHandle = assetManager->LoadAsset("Models/Primitives/sphere.fbx");
            // if (sphereHandle.IsValid()) {
            //     // Modeli sahneye ekle
            //     // auto model = assetManager->GetAsset<Model>(sphereHandle);
            //     // if (model) {
            //     //     // Model entity'sini oluştur
            //     //     Entity previewEntity = m_preview.previewScene->CreateEntity("PreviewObject");
            //     //     // Model component'ini ekle
            //     //     previewEntity.AddComponent<ModelComponent>(model);
            //     //     // Transform component'ini ayarla
            //     //     auto& transform = previewEntity.GetComponent<TransformComponent>();
            //     //     transform.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            //     //     transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
            //     // }
            // }
            
            // Geçici olarak basit bir entity oluştur
            Entity previewEntity = m_preview.previewScene->CreateEntity("PreviewObject");
            auto& transform = previewEntity.GetComponent<TransformComponent>();
            transform.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
        }
    }
    
    // Işık kaynağı ekle
    Entity lightEntity = m_preview.previewScene->CreateEntity("PreviewLight");
    auto& lightTransform = lightEntity.GetComponent<TransformComponent>();
    lightTransform.SetPosition(m_preview.lightPosition);
    
    // Işık component'ini ekle
    // TODO: Light component implementasyonu
    // auto& light = lightEntity.AddComponent<LightComponent>();
    // light.SetType(LightType::Directional);
    // light.SetColor(m_preview.lightColor);
    // light.SetIntensity(m_preview.lightIntensity);
    // light.SetDirection(glm::normalize(-m_preview.lightPosition));
    
    Logger::Info("MaterialEditor", "Preview sahnesi oluşturuldu");
}

void MaterialEditorWindow::UpdatePreviewScene(float deltaTime) {
    if (!m_preview.previewCamera || !m_preview.autoUpdate) {
        return;
    }
    
    // Kamerayı döndür
    float angle = glm::radians(m_preview.rotationSpeed * deltaTime);
    glm::vec3 position = m_preview.previewCamera->GetPosition();
    position.x = position.x * cos(angle) - position.z * sin(angle);
    position.z = position.x * sin(angle) + position.z * cos(angle);
    m_preview.previewCamera->SetPosition(position);
    m_preview.previewCamera->SetLookAt(glm::vec3(0.0f, 0.0f, 0.0f));
}

void MaterialEditorWindow::RenderUniformValue(ShaderUniform& uniform) {
    // Uniform değerini render et
    if (uniform.type == "float") {
        RenderUniformValueImpl<float>(uniform);
    } else if (uniform.type == "int") {
        RenderUniformValueImpl<int>(uniform);
    } else if (uniform.type == "vec2") {
        RenderUniformValueImpl<glm::vec2>(uniform);
    } else if (uniform.type == "vec3") {
        RenderUniformValueImpl<glm::vec3>(uniform);
    } else if (uniform.type == "vec4") {
        RenderUniformValueImpl<glm::vec4>(uniform);
    } else {
        ImGui::Text("Unsupported uniform type: %s", uniform.type.c_str());
    }
}

void MaterialEditorWindow::RenderTextureSlot(const std::string& slotName, const AssetHandle& texture) {
    ImGui::PushID(slotName.c_str());
    
    // Texture slot adı
    ImGui::Text("%s", slotName.c_str());
    ImGui::SameLine();
    
    // Texture yükle butonu
    if (ImGui::Button("Load")) {
        // TODO: Texture yükleme dialog'u
    }
    ImGui::SameLine();
    
    // Texture temizle butonu
    if (ImGui::Button("Clear")) {
        m_textureSlots[slotName] = AssetHandle();
        m_hasUnsavedChanges = true;
    }
    
    // Texture bilgisi
    if (texture.IsValid()) {
        ImGui::Text("Path: %s", texture.GetPath().c_str());
    } else {
        ImGui::Text("No texture loaded");
    }
    
    ImGui::PopID();
}

void MaterialEditorWindow::RenderPreview(VkCommandBuffer commandBuffer) {
    if (!m_previewRenderPass || !m_previewFramebuffer) {
        return;
    }
    
    // Preview render pass başlat
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_previewRenderPass;
    renderPassInfo.framebuffer = m_previewFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_previewExtent;
    
    VkClearValue clearValues[2];
    clearValues[0].color = {{m_backgroundColor.r, m_backgroundColor.g, m_backgroundColor.b, m_backgroundColor.a}};
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Preview objesini render et
    if (m_preview.previewScene && m_currentMaterial.IsValid()) {
        // Materyal ile objeyi render et
        // TODO: Preview render implementasyonu
    }
    
    vkCmdEndRenderPass(commandBuffer);
}

void MaterialEditorWindow::CreatePreviewRenderPass() {
    // Preview render pass oluştur
    // TODO: Implementasyon
}

void MaterialEditorWindow::CreatePreviewFramebuffer() {
    // Preview framebuffer oluştur
    // TODO: Implementasyon
}

void MaterialEditorWindow::CreatePreviewPipeline() {
    // Preview pipeline oluştur
    // TODO: Implementasyon
}

void MaterialEditorWindow::UpdateAvailableMaterials() {
    if (!m_assetSubsystem) {
        return;
    }
    
    auto assetManager = m_assetSubsystem->GetAssetManager();
    if (!assetManager) {
        return;
    }
    
    // Mevcut materyalleri güncelle
    m_availableMaterials.clear();
    
    // Asset registry'den materyalleri çek
    // TODO: Asset registry'den materyalleri çekme implementasyonu
    // Şimdilik placeholder olarak boş bir liste bırak
    
    // Geçici olarak Assets/Materials klasöründeki materyalleri tara
    // Bu kısım AssetSubsystem tamamen implemente edildiğinde güncellenecek
    std::vector<std::string> materialPaths = {
        "Assets/Materials/Default.amat"
    };
    
    for (const auto& path : materialPaths) {
        AssetHandle handle = assetManager->LoadAsset(path);
        if (handle.IsValid()) {
            m_availableMaterials.push_back(handle);
        }
    }
    
    // Eğer hiç materyal yüklenemediyse, log yaz
    if (m_availableMaterials.empty()) {
        Logger::Warning("MaterialEditor", "Hiç materyal bulunamadı");
    } else {
        Logger::Info("MaterialEditor", "{} materyal bulundu", m_availableMaterials.size());
    }
}

} // namespace AstralEngine

// ImGui toolbar helper fonksiyonları
namespace ImGui {
    void BeginToolbar(const char* str_id) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
        
        ImGui::Begin(str_id, nullptr, window_flags);
        ImGui::PopStyleVar(3);
    }
    
    void EndToolbar() {
        ImGui::End();
    }
    
    bool ToolbarButton(const char* label, const char* tooltip = nullptr) {
        bool clicked = ImGui::Button(label);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
        return clicked;
    }
    
    void ToolbarSeparator() {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
    }
    
    bool ToolbarToggle(const char* label, bool* v, const char* tooltip = nullptr) {
        bool clicked = ImGui::Checkbox(label, v);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
        return clicked;
    }
}