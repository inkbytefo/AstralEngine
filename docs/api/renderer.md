# Astral Engine Renderer API Referansı

## Giriş

Bu dokümantasyon, Astral Engine'in Vulkan tabanlı render sisteminin API'lerini detaylı olarak açıklar. Render sistem, modern 3D grafik rendering, deferred rendering pipeline, post-processing efektleri ve UI rendering gibi gelişmiş özellikler sunar.

## İçindekiler

1. [RenderSubsystem Sınıfı](#rendersubsystem-sınıfı)
2. [GraphicsDevice Sınıfı](#graphicsdevice-sınıfı)
3. [VulkanRenderer Sınıfı](#vulkanrenderer-sınıfı)
4. [Camera Sınıfı](#camera-sınıfı)
5. [Material Sistemi](#material-sistemi)
6. [PostProcessingSubsystem](#postprocessingsubsystem)
7. [UI Rendering](#ui-rendering)
8. [Vulkan Resource Yönetimi](#vulkan-resource-yönetimi)

## RenderSubsystem Sınıfı

### Genel Bakış

`RenderSubsystem` sınıfı, tüm çizim işlemlerini yönetir ve Vulkan API'sini soyutlar. Deferred rendering pipeline implementasyonu, shadow mapping, post-processing efektleri ve UI rendering sağlar.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;
    
    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::Render; }
    
    // Vulkan device erişimi
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }
    
    // Render hedefleri erişimi
    VulkanTexture* GetSceneColorTexture() const { return m_sceneColorTexture.get(); }
    VulkanTexture* GetAlbedoTexture() const { return m_gBufferAlbedo.get(); }
    VulkanTexture* GetNormalTexture() const { return m_gBufferNormal.get(); }
    VulkanTexture* GetPBRTexture() const { return m_gBufferPBR.get(); }
    VulkanTexture* GetDepthTexture() const { return m_gBufferDepth.get(); }
    VulkanTexture* GetShadowMapTexture() const { return m_shadowMapTexture.get(); }
    
    // Post-processing erişimi
    void SetPostProcessingInputTexture(VulkanTexture* sceneColorTexture);
    PostProcessingSubsystem* GetPostProcessingSubsystem() const { return m_postProcessing.get(); }
    
    // UI entegrasyonu
    void RenderUI();
    VkRenderPass GetUIRenderPass() const;
    VkCommandBuffer GetCurrentUICommandBuffer() const;
    
private:
    // Rendering pipeline metodları
    void ShadowPass();
    void GBufferPass();
    void LightingPass();
    void UpdateLightsAndShadows();
    
    // Resource yönetimi
    void CreateGBuffer();
    void DestroyGBuffer();
    void CreateLightingPassResources();
    void DestroyLightingPassResources();
    void CreateShadowPassResources();
    void DestroyShadowPassResources();
    
    // Üye değişkenler
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<VulkanMeshManager> m_vulkanMeshManager;
    std::unique_ptr<VulkanTextureManager> m_vulkanTextureManager;
    
    // G-Buffer resources
    std::unique_ptr<VulkanFramebuffer> m_gBufferFramebuffer;
    std::unique_ptr<VulkanTexture> m_gBufferAlbedo, m_gBufferNormal, m_gBufferPBR, m_gBufferDepth;
    
    // Lighting resources
    std::unique_ptr<VulkanFramebuffer> m_sceneFramebuffer;
    std::unique_ptr<VulkanTexture> m_sceneColorTexture;
    std::unique_ptr<VulkanBuffer> m_sceneUBO;
    std::vector<GPULight> m_lights;
    
    // Shadow resources
    std::unique_ptr<VulkanFramebuffer> m_shadowFramebuffer;
    std::unique_ptr<VulkanTexture> m_shadowMapTexture;
    glm::mat4 m_lightSpaceMatrix;
    
    // Post-processing
    std::unique_ptr<PostProcessingSubsystem> m_postProcessing;
    
    // UI resources
    VkRenderPass m_uiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_uiFramebuffers;
    std::vector<VkCommandBuffer> m_uiCommandBuffers;
    uint32_t m_currentFrame = 0;
};

} // namespace AstralEngine
```

### Başlatma ve Yapılandırma

#### RenderSubsystem Başlatma
```cpp
void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    
    // Gerekli subsystem'leri al
    m_window = m_owner->GetSubsystem<PlatformSubsystem>()->GetWindow();
    m_ecsSubsystem = m_owner->GetSubsystem<ECSSubsystem>();
    m_assetSubsystem = m_owner->GetSubsystem<AssetSubsystem>();
    
    try {
        // Graphics device başlat
        m_graphicsDevice = std::make_unique<GraphicsDevice>();
        if (!m_graphicsDevice->Initialize(m_window, m_owner)) {
            throw std::runtime_error("GraphicsDevice initialization failed");
        }
        
        // Resource manager'ları başlat
        m_vulkanMeshManager = std::make_unique<VulkanMeshManager>();
        m_vulkanMeshManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem);
        
        m_vulkanTextureManager = std::make_unique<VulkanTextureManager>();
        m_vulkanTextureManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem);
        
        m_materialManager = std::make_unique<MaterialManager>();
        m_materialManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem->GetAssetManager());
        
        // Kamera oluştur
        m_camera = std::make_unique<Camera>();
        Camera::Config cameraConfig;
        cameraConfig.position = glm::vec3(0.0f, 2.0f, 5.0f);
        cameraConfig.aspectRatio = (float)m_window->GetWidth() / (float)m_window->GetHeight();
        m_camera->Initialize(cameraConfig);
        
        // Render pipeline resource'larını oluştur
        CreateShadowPassResources();
        CreateGBuffer();
        CreateLightingPassResources();
        
        // Post-processing başlat
        m_postProcessing = std::make_unique<PostProcessingSubsystem>();
        m_postProcessing->Initialize(this);
        
        // UI resource'larını oluştur
        CreateUIRenderPass();
        CreateUIFramebuffers();
        CreateUICommandBuffers();
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Initialization failed: {}", e.what());
        OnShutdown(); // Kısmi başlatılmış resource'ları temizle
        throw;
    }
}
```

### Rendering Pipeline

#### 1. Shadow Pass
```cpp
void RenderSubsystem::ShadowPass() {
    try {
        auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
        if (!vulkanRenderer || !m_ecsSubsystem) return;
        
        // Işık frustum'u oluştur
        Frustum lightFrustum = CreateLightFrustumFromDirectionalLight();
        
        // Shadow caster'ları bul
        std::vector<VulkanRenderer::ResolvedRenderItem> shadowCasters;
        auto view = m_ecsSubsystem->GetRegistry().view<const RenderComponent, const TransformComponent>();
        
        for (auto entity : view) {
            const auto& renderComp = view.get<RenderComponent>(entity);
            const auto& transformComp = view.get<TransformComponent>(entity);
            
            if (!renderComp.visible || !renderComp.castsShadows) continue;
            
            // Frustum culling
            auto mesh = m_vulkanMeshManager->GetOrCreateMesh(renderComp.modelHandle);
            if (mesh && mesh->IsReady()) {
                const AABB& localAABB = mesh->GetBoundingBox();
                const glm::mat4& transform = transformComp.GetWorldMatrix();
                AABB worldAABB = localAABB.Transform(transform);
                
                if (lightFrustum.Intersects(worldAABB)) {
                    shadowCasters.push_back({transform, mesh, nullptr});
                }
            }
        }
        
        // Shadow map oluştur
        if (!shadowCasters.empty()) {
            vulkanRenderer->RecordShadowPassCommands(m_shadowFramebuffer.get(), 
                                                   m_lightSpaceMatrix, shadowCasters);
        }
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Shadow pass failed: {}", e.what());
        // Gölgesiz devam et - graceful degradation
    }
}
```

#### 2. G-Buffer Pass
```cpp
void RenderSubsystem::GBufferPass() {
    auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
    if (!vulkanRenderer || !m_ecsSubsystem) return;
    
    // Render kuyruğu oluştur - instancing için optimize edilmiş
    std::map<MeshMaterialKey, std::vector<glm::mat4>> renderQueue;
    
    // Kameranın frustum'unu al
    const auto& frustum = m_camera->GetFrustum();
    
    // ECS'den renderable entity'leri al
    auto view = m_ecsSubsystem->GetRegistry().view<const RenderComponent, const TransformComponent>();
    
    for (auto entity : view) {
        const auto& renderComp = view.get<RenderComponent>(entity);
        const auto& transformComp = view.get<TransformComponent>(entity);
        
        // Görünür değilse atla
        if (!renderComp.visible) continue;
        
        // Frustum culling
        auto mesh = m_vulkanMeshManager->GetOrCreateMesh(renderComp.modelHandle);
        if (mesh && mesh->IsReady()) {
            const AABB& localAABB = mesh->GetBoundingBox();
            const glm::mat4& transform = transformComp.GetWorldMatrix();
            AABB worldAABB = localAABB.Transform(transform);
            
            if (frustum.Intersects(worldAABB)) {
                MeshMaterialKey key = {renderComp.modelHandle, renderComp.materialHandle};
                renderQueue[key].push_back(transform);
            }
        }
    }
    
    // Batch rendering
    vulkanRenderer->RecordGBufferCommands(m_graphicsDevice->GetCurrentFrameIndex(), 
                                        m_gBufferFramebuffer.get(), renderQueue);
}
```

#### 3. Lighting Pass
```cpp
void RenderSubsystem::LightingPass() {
    auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
    if (!vulkanRenderer) return;
    
    // Lighting pass komutlarını kaydet
    vulkanRenderer->RecordLightingCommands(m_graphicsDevice->GetCurrentFrameIndex(), 
                                         m_sceneFramebuffer.get());
}
```

### Post-Processing Sistemi

#### PostProcessingSubsystem Kullanımı
```cpp
// Tonemapping efekti ayarlama
if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
    tonemapping->SetExposure(1.5f);
    tonemapping->SetGamma(2.2f);
    tonemapping->SetTonemapper(TonemapperType::ACES);
    tonemapping->SetContrast(1.1f);
    tonemapping->SetBrightness(0.1f);
    tonemapping->SetSaturation(1.2f);
}

// Bloom efekti ayarlama
if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
    bloom->SetThreshold(0.8f);
    bloom->SetKnee(0.5f);
    bloom->SetIntensity(0.6f);
    bloom->SetRadius(4.0f);
    bloom->SetQuality(BloomQuality::High);
    bloom->SetUseDirt(false);
    bloom->SetDirtIntensity(1.0f);
}

// Efektleri etkinleştir/devre dışı bırak
m_postProcessing->EnableEffect("TonemappingEffect", true);
m_postProcessing->EnableEffect("BloomEffect", true);

// Post-processing'i uygula
if (m_postProcessing) {
    m_postProcessing->Execute(m_graphicsDevice->GetCurrentCommandBuffer(), 
                            m_graphicsDevice->GetCurrentFrameIndex());
}
```

### UI Rendering

#### UI Başlatma
```cpp
void RenderSubsystem::CreateUIRenderPass() {
#ifdef ASTRAL_USE_IMGUI
    auto* swapchain = m_graphicsDevice->GetSwapchain();
    if (!swapchain) return;
    
    // UI render pass tanımı
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain->GetFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // Mevcut içeriği koru
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // UI render pass oluştur
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    
    if (vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), 
                          &renderPassInfo, nullptr, &m_uiRenderPass) != VK_SUCCESS) {
        Logger::Error("RenderSubsystem", "Failed to create UI render pass");
    }
#endif
}
```

#### UI Rendering
```cpp
void RenderSubsystem::RenderUI() {
#ifdef ASTRAL_USE_IMGUI
    VkCommandBuffer commandBuffer = GetCurrentUICommandBuffer();
    if (!commandBuffer) return;
    
    // UI render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_uiRenderPass;
    renderPassInfo.framebuffer = m_uiFramebuffers[m_currentFrame];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_window->GetWidth(), m_window->GetHeight()};
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // ImGui rendering
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->CmdListsCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
    
    vkCmdEndRenderPass(commandBuffer);
#endif
}
```

## GraphicsDevice Sınıfı

### Genel Bakış

`GraphicsDevice` sınıfı, Vulkan API'sinin temel yapılarını yönetir: instance, device, swapchain ve command buffer'lar.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();
    
    bool Initialize(Window* window, Engine* engine);
    void Shutdown();
    
    // Vulkan device erişimi
    VulkanDevice* GetVulkanDevice() const { return m_vulkanDevice.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_swapchain.get(); }
    VulkanRenderer* GetVulkanRenderer() const { return m_vulkanRenderer.get(); }
    
    // Frame yönetimi
    bool BeginFrame();
    void EndFrame();
    void Present();
    
    // Command buffer erişimi
    VkCommandBuffer GetCurrentCommandBuffer() const;
    uint32_t GetCurrentFrameIndex() const { return m_currentFrame; }
    
    // Device properties
    bool IsInitialized() const { return m_initialized; }
    VkPhysicalDevice GetPhysicalDevice() const;
    VkDevice GetDevice() const;
    VkQueue GetGraphicsQueue() const;
    uint32_t GetGraphicsQueueFamily() const;
    
private:
    bool m_initialized = false;
    Window* m_window = nullptr;
    Engine* m_engine = nullptr;
    
    std::unique_ptr<VulkanInstance> m_instance;
    std::unique_ptr<VulkanDevice> m_vulkanDevice;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::unique_ptr<VulkanRenderer> m_vulkanRenderer;
    
    uint32_t m_currentFrame = 0;
};

} // namespace AstralEngine
```

### Frame Yönetimi

#### Frame Başlatma
```cpp
bool GraphicsDevice::BeginFrame() {
    if (!m_initialized) return false;
    
    // Swapchain'den yeni image al
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        m_vulkanDevice->GetDevice(),
        m_swapchain->GetSwapchain(),
        UINT64_MAX, // Timeout
        m_vulkanDevice->GetImageAvailableSemaphore(m_currentFrame),
        VK_NULL_HANDLE,
        &imageIndex
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain yeniden oluşturulmalı
        RecreateSwapchain();
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        Logger::Error("GraphicsDevice", "Failed to acquire swapchain image");
        return false;
    }
    
    m_currentFrame = imageIndex;
    
    // Command buffer'ı başlat
    VkCommandBuffer cmd = GetCurrentCommandBuffer();
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        Logger::Error("GraphicsDevice", "Failed to begin command buffer");
        return false;
    }
    
    return true;
}
```

## Camera Sınıfı

### Genel Bakış

`Camera` sınıfı, 3D kamera yönetimini sağlar: pozisyon, rotasyon, projeksiyon ve frustum hesaplamaları.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class Camera {
public:
    struct Config {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);
        glm::vec3 rotation = glm::vec3(0.0f); // Euler angles (radians)
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        float aspectRatio = 16.0f / 9.0f;
        float fov = glm::radians(60.0f);
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
    };
    
    Camera();
    ~Camera();
    
    bool Initialize(const Config& config);
    void Shutdown();
    
    // Kamera kontrolü
    void SetPosition(const glm::vec3& position);
    void SetRotation(const glm::vec3& rotation); // Euler angles
    void SetAspectRatio(float aspectRatio);
    void SetFOV(float fovRadians);
    void SetClipPlanes(float nearPlane, float farPlane);
    
    // Matris hesaplamaları
    const glm::mat4& GetViewMatrix() const { return m_viewMatrix; }
    const glm::mat4& GetProjectionMatrix() const { return m_projectionMatrix; }
    const glm::mat4& GetViewProjectionMatrix() const { return m_viewProjectionMatrix; }
    
    // Pozisyon ve yön
    const glm::vec3& GetPosition() const { return m_position; }
    const glm::vec3& GetForward() const { return m_forward; }
    const glm::vec3& GetRight() const { return m_right; }
    const glm::vec3& GetUp() const { return m_up; }
    
    // Frustum
    const Frustum& GetFrustum() const { return m_frustum; }
    
    // Güncelleme
    void Update();
    
private:
    void UpdateMatrices();
    void UpdateFrustum();
    
    Config m_config;
    glm::vec3 m_position;
    glm::vec3 m_forward;
    glm::vec3 m_right;
    glm::vec3 m_up;
    
    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewProjectionMatrix;
    
    Frustum m_frustum;
    bool m_initialized = false;
};

} // namespace AstralEngine
```

### Kamera Kontrolü

#### Temel Hareket
```cpp
Camera::Config config;
config.position = glm::vec3(0.0f, 2.0f, 5.0f);
config.aspectRatio = 1920.0f / 1080.0f;

Camera camera;
camera.Initialize(config);

// Kamera pozisyonunu güncelle
camera.SetPosition(glm::vec3(10.0f, 5.0f, 0.0f));

// Rotasyon ayarla (Euler angles - radians)
camera.SetRotation(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)); // 90 derece Y ekseni

// Kamera matrislerini güncelle
camera.Update();

// Matrisleri al
const glm::mat4& view = camera.GetViewMatrix();
const glm::mat4& proj = camera.GetProjectionMatrix();
const glm::mat4& viewProj = camera.GetViewProjectionMatrix();
```

#### Mouse Kontrolü
```cpp
class CameraController {
    Camera* m_camera;
    glm::vec2 m_lastMousePos;
    float m_mouseSensitivity = 0.005f;
    
public:
    void OnMouseMove(float x, float y) {
        glm::vec2 delta = glm::vec2(x, y) - m_lastMousePos;
        m_lastMousePos = glm::vec2(x, y);
        
        // Mouse hareketini kamera rotasyonuna çevir
        glm::vec3 rotation = m_camera->GetRotation();
        rotation.y += delta.x * m_mouseSensitivity; // Yaw
        rotation.x += delta.y * m_mouseSensitivity; // Pitch
        
        // Pitch sınırlaması
        rotation.x = glm::clamp(rotation.x, -glm::pi<float>()/2.0f + 0.1f, 
                               glm::pi<float>()/2.0f - 0.1f);
        
        m_camera->SetRotation(rotation);
        m_camera->Update();
    }
};
```

## Material Sistemi

### Genel Bakış

Material sistemi, PBR (Physically Based Rendering) tabanlı materyal yönetimi sağlar. Albedo, normal, metalness, roughness, AO texture'larını destekler.

### Material Sınıfı

```cpp
namespace AstralEngine {

class Material {
public:
    struct Properties {
        glm::vec4 albedoColor = glm::vec4(1.0f);          // RGB + alpha
        glm::vec4 emissiveColor = glm::vec4(0.0f);        // RGB + intensity
        glm::vec4 metallicRoughness = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f); // metal, rough, ao, padding
        glm::vec4 normalScale = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f); // scale, padding
    };
    
    Material();
    ~Material();
    
    // Properties
    void SetAlbedoColor(const glm::vec4& color);
    void SetEmissiveColor(const glm::vec4& color);
    void SetMetallic(float metallic);
    void SetRoughness(float roughness);
    void SetNormalScale(float scale);
    
    // Textures
    void SetAlbedoTexture(AssetHandle texture);
    void SetNormalTexture(AssetHandle texture);
    void SetMetallicTexture(AssetHandle texture);
    void SetRoughnessTexture(AssetHandle texture);
    void SetAOTexture(AssetHandle texture);
    void SetEmissiveTexture(AssetHandle texture);
    
    // Vulkan resources
    VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }
    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    
    // Güncelleme
    void UpdateUniformBuffer();
    
private:
    Properties m_properties;
    AssetHandle m_albedoTexture;
    AssetHandle m_normalTexture;
    AssetHandle m_metallicTexture;
    AssetHandle m_roughnessTexture;
    AssetHandle m_aoTexture;
    AssetHandle m_emissiveTexture;
    
    VulkanBuffer* m_uniformBuffer;
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
};

} // namespace AstralEngine
```

### Material Kullanımı

#### PBR Material Oluşturma
```cpp
// Asset'leri yükle
AssetHandle albedoTexture = assetManager->Load<Texture>("materials/brick_albedo.png");
AssetHandle normalTexture = assetManager->Load<Texture>("materials/brick_normal.png");
AssetHandle metallicTexture = assetManager->Load<Texture>("materials/brick_metallic.png");
AssetHandle roughnessTexture = assetManager->Load<Texture>("materials/brick_roughness.png");
AssetHandle aoTexture = assetManager->Load<Texture>("materials/brick_ao.png");

// Material oluştur
Material material;
material.SetAlbedoTexture(albedoTexture);
material.SetNormalTexture(normalTexture);
material.SetMetallicTexture(metallicTexture);
material.SetRoughnessTexture(roughnessTexture);
material.SetAOTexture(aoTexture);

// PBR parametreleri
material.SetAlbedoColor(glm::vec4(1.0f)); // Beyaz albedo
material.SetMetallic(0.0f); // Tamamen dielektrik
material.SetRoughness(0.8f); // Pürüzlü yüzey
material.SetNormalScale(1.0f); // Normal map scale

// Vulkan resource'larını güncelle
material.UpdateUniformBuffer();
```

## PostProcessingSubsystem

### Genel Bakış

PostProcessingSubsystem, render sonrası efektleri yönetir: tonemapping, bloom, color grading gibi efektler.

### Sınıf Tanımı

```cpp
namespace AstralEngine {

class PostProcessingSubsystem {
public:
    PostProcessingSubsystem();
    ~PostProcessingSubsystem();
    
    bool Initialize(RenderSubsystem* renderSubsystem);
    void Shutdown();
    
    // Efekt yönetimi
    template<typename T>
    T* GetEffect();
    
    void EnableEffect(const std::string& effectName, bool enabled);
    bool IsEffectEnabled(const std::string& effectName) const;
    
    // Render pipeline
    void Execute(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    
    // Input/Output texture'lar
    void SetInputTexture(VulkanTexture* texture);
    VulkanTexture* GetOutputTexture() const { return m_outputTexture.get(); }
    
private:
    RenderSubsystem* m_renderSubsystem = nullptr;
    std::vector<std::unique_ptr<IPostProcessingEffect>> m_effects;
    std::vector<std::unique_ptr<VulkanTexture>> m_renderTargets;
    
    VulkanTexture* m_inputTexture = nullptr;
    std::unique_ptr<VulkanTexture> m_outputTexture;
    std::unique_ptr<VulkanFramebuffer> m_framebuffer;
};

} // namespace AstralEngine
```

### Post-Processing Efektleri

#### TonemappingEffect
```cpp
class TonemappingEffect : public IPostProcessingEffect {
public:
    // Parametreler
    void SetExposure(float exposure);
    void SetGamma(float gamma);
    void SetTonemapper(TonemapperType type); // ACES, Reinhard, Filmic, Custom
    void SetContrast(float contrast);
    void SetBrightness(float brightness);
    void SetSaturation(float saturation);
    
    // HDR'den LDR'ye dönüşüm
    void Execute(VkCommandBuffer cmd, VulkanTexture* input, VulkanTexture* output) override;
};
```

#### BloomEffect
```cpp
class BloomEffect : public IPostProcessingEffect {
public:
    // Parametreler
    void SetThreshold(float threshold);      // Parlaklık eşiği
    void SetKnee(float knee);                 // Eşik yumuşatma
    void SetIntensity(float intensity);       // Bloom yoğunluğu
    void SetRadius(float radius);             // Bulanıklık yarıçapı
    void SetQuality(BloomQuality quality);    // Kalite seviyesi
    void SetUseDirt(bool useDirt);            // Lens dirt efekti
    void SetDirtIntensity(float intensity);   // Lens dirt yoğunluğu
    
    // Bloom uygula
    void Execute(VkCommandBuffer cmd, VulkanTexture* input, VulkanTexture* output) override;
};
```

### Post-Processing Kullanımı

#### Efekt Konfigürasyonu
```cpp
// PostProcessingSubsystem'e erişim
auto* postProcessing = renderSubsystem->GetPostProcessingSubsystem();
if (!postProcessing) return;

// Tonemapping ayarla
if (auto* tonemapping = postProcessing->GetEffect<TonemappingEffect>()) {
    tonemapping->SetExposure(1.2f);
    tonemapping->SetGamma(2.2f);
    tonemapping->SetTonemapper(TonemapperType::ACES);
    tonemapping->SetContrast(1.1f);
    tonemapping->SetBrightness(0.05f);
    tonemapping->SetSaturation(1.1f);
}

// Bloom ayarla
if (auto* bloom = postProcessing->GetEffect<BloomEffect>()) {
    bloom->SetThreshold(0.8f);
    bloom->SetKnee(0.5f);
    bloom->SetIntensity(0.6f);
    bloom->SetRadius(4.0f);
    bloom->SetQuality(BloomQuality::High);
    bloom->SetUseDirt(true);
    bloom->SetDirtIntensity(0.8f);
}

// Efektleri etkinleştir
postProcessing->EnableEffect("TonemappingEffect", true);
postProcessing->EnableEffect("BloomEffect", true);
```

#### Dinamik Parametre Güncelleme
```cpp
// Gün/gece döngüsü için dinamik ayarlar
void UpdatePostProcessing(float timeOfDay) {
    if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
        if (timeOfDay < 0.3f) { // Gece
            tonemapping->SetExposure(0.8f);
            tonemapping->SetBrightness(0.2f);
        } else if (timeOfDay > 0.7f) { // Gündüz
            tonemapping->SetExposure(1.3f);
            tonemapping->SetBrightness(0.0f);
        } else { // Alacakaranlık
            tonemapping->SetExposure(1.0f);
            tonemapping->SetBrightness(0.1f);
        }
    }
    
    if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
        // Güneş ışığına göre bloom ayarla
        float sunIntensity = GetSunIntensity(timeOfDay);
        bloom->SetThreshold(1.0f + sunIntensity * 0.2f);
        bloom->SetIntensity(0.4f + sunIntensity * 0.4f);
    }
}
```

## UI Rendering

### ImGui Entegrasyonu

#### UI Başlatma
```cpp
void UISubsystem::InitializeImGui() {
#ifdef ASTRAL_USE_IMGUI
    // ImGui context oluştur
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Platform ve renderer backend'leri başlat
    ImGui_ImplSDL3_InitForVulkan(window->GetSDLWindow());
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = graphicsDevice->GetInstance();
    init_info.PhysicalDevice = graphicsDevice->GetPhysicalDevice();
    init_info.Device = graphicsDevice->GetDevice();
    init_info.QueueFamily = graphicsDevice->GetGraphicsQueueFamily();
    init_info.Queue = graphicsDevice->GetGraphicsQueue();
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = graphicsDevice->GetSwapchain()->GetImageCount();
    
    // Render pass'i RenderSubsystem'den al
    ImGui_ImplVulkan_Init(&init_info, renderSubsystem->GetUIRenderPass());
    
    // Font atlas oluştur
    VkCommandBuffer cmd = graphicsDevice->GetVulkanDevice()->BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    graphicsDevice->GetVulkanDevice()->EndSingleTimeCommands(cmd);
#endif
}
```

#### UI Rendering Loop'u
```cpp
void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice || !m_graphicsDevice->IsInitialized()) return;
    
    if (m_materialManager) m_materialManager->Update();
    
    if (m_graphicsDevice->BeginFrame()) {
        // 1. Shadow Pass
        ShadowPass();
        
        // 2. G-Buffer Pass
        GBufferPass();
        
        // 3. Lighting Pass
        LightingPass();
        
        // 4. Post-Processing
        if (m_postProcessing) {
            m_postProcessing->Execute(m_graphicsDevice->GetCurrentCommandBuffer(), 
                                    m_graphicsDevice->GetCurrentFrameIndex());
        }
        
        // 5. UI Rendering
        RenderUI();
        
        // 6. Frame sonlandırma
        m_graphicsDevice->EndFrame();
        m_graphicsDevice->Present();
    }
}
```

## Vulkan Resource Yönetimi

### Resource Lifecycle

#### RAII Pattern
```cpp
class VulkanTexture {
public:
    VulkanTexture();
    ~VulkanTexture();
    
    bool Initialize(VulkanDevice* device, uint32_t width, uint32_t height, 
                   VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties);
    
    void Shutdown();
    
    // Resource erişimi
    VkImage GetImage() const { return m_image; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    
private:
    VulkanDevice* m_device = nullptr;
    
    // Vulkan resource'lar
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    
    bool m_initialized = false;
};
```

### Memory Management

#### Vulkan Memory Allocator
```cpp
class VulkanMemoryManager {
public:
    bool Initialize(VulkanDevice* device);
    void Shutdown();
    
    // Buffer oluşturma
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VulkanBuffer& buffer);
    
    // Texture oluşturma
    bool CreateImage(uint32_t width, uint32_t height, VkFormat format,
                    VkImageTiling tiling, VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties, VulkanTexture& texture);
    
    // Memory istatistikleri
    VkDeviceSize GetTotalAllocated() const;
    VkDeviceSize GetTotalUsed() const;
    
private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
};
```

## Hata Yönetimi ve Debugging

### Vulkan Validation Layers

#### Debug Build Konfigürasyonu
```cpp
#ifdef ASTRAL_ENABLE_VALIDATION
    // Validation layers etkinleştir
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    
    // Debug messenger ekle
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = DebugCallback;
    
    instanceCreateInfo.pNext = &debugCreateInfo;
#endif
```

### Debug Callback Fonksiyonu
```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    std::string prefix;
    
    // Mesaj önemine göre prefix seç
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        prefix = "[VULKAN TRACE] ";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        prefix = "[VULKAN INFO] ";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        prefix = "[VULKAN WARNING] ";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        prefix = "[VULKAN ERROR] ";
    }
    
    // Log mesajı
    Logger::Error("Vulkan", "{}{}", prefix, pCallbackData->pMessage);
    
    // Validation hatalarını daha detaylı logla
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        Logger::Error("Vulkan", "  Objects: {}", pCallbackData->objectCount);
        for (uint32_t i = 0; i < pCallbackData->objectCount; ++i) {
            const auto& object = pCallbackData->pObjects[i];
            Logger::Error("Vulkan", "    - {}: {}", object.objectType, object.objectHandle);
        }
    }
    
    return VK_FALSE; // Vulkan fonksiyonlarının çalışmasına devam et
}
```

## Performans İyileştirmeleri

### Pipeline Caching
```cpp
class VulkanRenderer {
    std::unordered_map<size_t, VkPipeline> m_pipelineCache;
    
    VkPipeline GetOrCreatePipeline(const Material& material, VkRenderPass renderPass) {
        // Unique pipeline hash oluştur
        size_t hash = 0;
        HashCombine(hash, material.GetShaderHandle().GetHash());
        HashCombine(hash, material.GetRasterizationState());
        HashCombine(hash, material.GetDepthStencilState());
        HashCombine(hash, reinterpret_cast<uintptr_t>(renderPass));
        
        // Cache lookup
        auto it = m_pipelineCache.find(hash);
        if (it != m_pipelineCache.end()) {
            return it->second;
        }
        
        // Yeni pipeline oluştur
        VkPipeline pipeline = CreatePipeline(material, renderPass);
        m_pipelineCache[hash] = pipeline;
        
        return pipeline;
    }
};
```


## Sık Karşılaşılan Sorular

**S: RenderSubsystem birden fazla pencere destekliyor mu?**
A: Şu anda tek pencere destekleniyor. Multi-window desteği gelecek versiyonlarda planlanıyor.

**S: Vulkan validation layers nasıl etkinleştirilir?**
A: CMake build sırasında `-DASTRAL_ENABLE_VALIDATION=ON` kullanın.

**S: Post-processing efektleri nasıl sıralanır?**
A: Efektler ekleme sırasına göre uygulanır: Input → Tonemapping → Bloom → Output

**S: UI rendering hangi aşamada yapılır?**
A: Post-processing'ten sonra, final present öncesinde.

**S: Shadow map çözünürlüğü nasıl değiştirilir?**
A: `SHADOW_MAP_SIZE` makrosunu değiştirin (varsayılan: 2048x2048).

**S: Kaç ışık destekleniyor?**
A: `MAX_LIGHTS` kadar (varsayılan: 16). Performans için 4-8 arası önerilir.

Bu API referansı, Astral Engine'in güçlü ve esnek render sisteminin tüm özelliklerini kapsamaktadır. Modern Vulkan tabanlı rendering, PBR materyaller, gelişmiş post-processing ve profesyonel debugging araçları ile donatılmıştır.