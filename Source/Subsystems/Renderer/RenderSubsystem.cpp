#include "RenderSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Platform/PlatformSubsystem.h"
#include "../ECS/ECSSubsystem.h"
#include "../ECS/Components.h"
#include "../Asset/AssetSubsystem.h"
#include "Material/Material.h"
#include "VulkanMeshManager.h"
#include "VulkanTextureManager.h"
#include "Buffers/VulkanTexture.h"
#include "Buffers/VulkanBuffer.h"
#include "VulkanRenderer.h"
#include "GraphicsDevice.h"
#include "VulkanBindlessSystem.h"
#include "VulkanUtils.h"
#include "Bounds.h"
#include "PostProcessingSubsystem.h"
#include "TonemappingEffect.h"
#include "BloomEffect.h"
#include <map>
#include <glm/gtc/quaternion.hpp>

// UI Integration includes
#ifdef ASTRAL_USE_IMGUI
    #include "ThirdParty/imgui.h"
    #include <imgui_impl_vulkan.h>
#endif

namespace AstralEngine {

#define MAX_LIGHTS 16
#define SHADOW_MAP_SIZE 2048

struct SceneUBO {
    glm::mat4 view; glm::mat4 projection; glm::mat4 inverseView; glm::mat4 inverseProjection;
    glm::vec4 viewPosition;
    int lightCount;
    float _padding[3];
    GPULight lights[MAX_LIGHTS];
};

RenderSubsystem::RenderSubsystem() = default;
RenderSubsystem::~RenderSubsystem() = default;

void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    m_window = m_owner->GetSubsystem<PlatformSubsystem>()->GetWindow();
    m_ecsSubsystem = m_owner->GetSubsystem<ECSSubsystem>();
    m_assetSubsystem = m_owner->GetSubsystem<AssetSubsystem>();

    if (!m_window) {
        Logger::Error("RenderSubsystem", "Failed to get window from PlatformSubsystem");
        throw std::runtime_error("RenderSubsystem initialization failed: Window not available");
    }

    if (!m_ecsSubsystem) {
        Logger::Error("RenderSubsystem", "Failed to get ECSSubsystem");
        throw std::runtime_error("RenderSubsystem initialization failed: ECSSubsystem not available");
    }

    if (!m_assetSubsystem) {
        Logger::Error("RenderSubsystem", "Failed to get AssetSubsystem");
        throw std::runtime_error("RenderSubsystem initialization failed: AssetSubsystem not available");
    }

    try {
        m_graphicsDevice = std::make_unique<GraphicsDevice>();
        if (!m_graphicsDevice->Initialize(m_window, m_owner)) {
            Logger::Error("RenderSubsystem", "Failed to initialize GraphicsDevice");
            throw std::runtime_error("GraphicsDevice initialization failed");
        }
        
        m_vulkanMeshManager = std::make_unique<VulkanMeshManager>();
        if (!m_vulkanMeshManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem)) {
            Logger::Error("RenderSubsystem", "Failed to initialize VulkanMeshManager");
            throw std::runtime_error("VulkanMeshManager initialization failed");
        }
        
        m_vulkanTextureManager = std::make_unique<VulkanTextureManager>();
        if (!m_vulkanTextureManager->Initialize(m_graphicsDevice.get(), m_assetSubsystem)) {
            Logger::Error("RenderSubsystem", "Failed to initialize VulkanTextureManager");
            throw std::runtime_error("VulkanTextureManager initialization failed");
        }
        
        m_materialManager = std::make_unique<MaterialManager>();
        if (!m_materialManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem->GetAssetManager(), m_graphicsDevice->GetBindlessSystem())) {
            Logger::Error("RenderSubsystem", "Failed to initialize MaterialManager");
            throw std::runtime_error("MaterialManager initialization failed");
        }
        
        m_camera = std::make_unique<Camera>();
        Camera::Config cameraConfig; 
        cameraConfig.position = glm::vec3(0.0f, 2.0f, 5.0f); 
        cameraConfig.aspectRatio = (float)m_window->GetWidth() / (float)m_window->GetHeight();
        if (!m_camera->Initialize(cameraConfig)) {
            Logger::Error("RenderSubsystem", "Failed to initialize Camera");
            throw std::runtime_error("Camera initialization failed");
        }
        
        Logger::Info("RenderSubsystem", "Modern Dynamic Rendering pipeline active");
        
        CreateShadowPassResources();
        CreateGBuffer();
        CreateLightingPassResources();
        
        // PostProcessingSubsystem'i oluştur ve başlat
        m_postProcessing = std::make_unique<PostProcessingSubsystem>();
        if (m_postProcessing) {
            if (!m_postProcessing->Initialize(this)) {
                Logger::Warning("RenderSubsystem", "Failed to initialize PostProcessingSubsystem, continuing without it");
                m_postProcessing.reset();
            } else {
                // PostProcessingSubsystem'e VulkanRenderer pointer'ını geç
                SetVulkanRenderer(m_graphicsDevice->GetVulkanRenderer());
            }
        }
        
        // Swapchain boyutlarını al
        auto* swapchain = m_graphicsDevice->GetSwapchain();
        if (!swapchain) {
            Logger::Error("RenderSubsystem", "Failed to get swapchain from GraphicsDevice");
            throw std::runtime_error("Swapchain not available");
        }
        
        uint32_t width = swapchain->GetWidth();
        uint32_t height = swapchain->GetHeight();
        
        // Sahne rengi texture'ını oluştur
        CreateSceneColorTexture(width, height);
        
        // PostProcessingSubsystem'e input texture'ı ayarla
        if (m_postProcessing) {
            m_postProcessing->SetInputTexture(m_sceneColorTexture.get());
        }

        // UI resources oluştur
    #ifdef ASTRAL_USE_IMGUI
        CreateUIRenderPass();
        CreateUIFramebuffers();
        CreateUICommandBuffers();
    #endif
        
        Logger::Info("RenderSubsystem", "RenderSubsystem initialized successfully");
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Initialization failed: {}", e.what());
        // Clean up any partially initialized resources
        OnShutdown();
        throw;
    }
}
    
    /* YENİ POST-PROCESSING KULLANIM ÖRNEKLERİ:
     
     // PostProcessingSubsystem'e erişim için RenderSubsystem üzerinden GetPostProcessingSubsystem() kullanılır
     // auto* postProcessing = GetPostProcessingSubsystem();
     
     // 1. Tonemapping efekt parametrelerini ayarlama (yeni kullanım şekli)
     if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
         tonemapping->SetExposure(1.5f);      // Exposure değerini ayarla
         tonemapping->SetGamma(2.2f);        // Gamma correction değerini ayarla
         tonemapping->SetTonemapper(1);       // Tonemapper tipi (0: ACES, 1: Reinhard, 2: Filmic, 3: Custom)
         tonemapping->SetContrast(1.1f);     // Kontrast değerini ayarla
         tonemapping->SetBrightness(0.1f);   // Parlaklık değerini ayarla
         tonemapping->SetSaturation(1.2f);   // Doygunluk değerini ayarla
     }
     
     // 2. Bloom efekt parametrelerini ayarlama (yeni kullanım şekli)
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetThreshold(0.8f);        // Parlaklık eşiğini ayarla
         bloom->SetKnee(0.5f);             // Eşik yumuşatma değerini ayarla
         bloom->SetIntensity(0.6f);        // Bloom yoğunluğunu ayarla
         bloom->SetRadius(4.0f);           // Bulanıklık yarıçapını ayarla
         bloom->SetQuality(1);             // Kalite seviyesi (0: low, 1: medium, 2: high)
         bloom->SetUseDirt(false);         // Lens dirt kullanımı
         bloom->SetDirtIntensity(1.0f);    // Lens dirt yoğunluğu
     }
     
     // 3. Efektleri etkinleştir/devre dışı bırakma
     m_postProcessing->EnableEffect("TonemappingEffect", true);
     m_postProcessing->EnableEffect("BloomEffect", true);
     
     // 4. Dinamik olarak efekt parametrelerini güncelleme (örneğin gün/gece döngüsü için)
     // Gündüz ayarları
     if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
         tonemapping->SetExposure(1.3f);
     }
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetThreshold(1.1f);
         bloom->SetIntensity(0.4f);
     }
     
     // Gece ayarları
     if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
         tonemapping->SetExposure(0.8f);
         tonemapping->SetBrightness(0.2f);
     }
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetThreshold(0.7f);
         bloom->SetIntensity(0.9f);
     }
     
     // 5. Performans optimizasyonu için kalite ayarları
     // Düşük kalite (performans odaklı)
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetQuality(0);
         bloom->SetIntensity(0.3f);
     }
     
     // Yüksek kalite (görsel kalite odaklı)
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetQuality(2);
         bloom->SetIntensity(0.8f);
     }
     
     // 6. HDR içerik için optimizasyon
     if (auto* tonemapping = m_postProcessing->GetEffect<TonemappingEffect>()) {
         tonemapping->SetExposure(1.5f);
         tonemapping->SetTonemapper(0); // ACES tonemapper
     }
     if (auto* bloom = m_postProcessing->GetEffect<BloomEffect>()) {
         bloom->SetThreshold(1.2f);
         bloom->SetIntensity(0.6f);
     }
     
     NOT: Bu örnek kodlar, yeni PostProcessingSubsystem yapısına göre efekt parametrelerine
     nasıl erişileceğini göstermektedir. Eski kullanım şekli olan:
     // m_postProcessing->SetTonemappingExposure(1.5f);
     // m_postProcessing->SetBloomThreshold(0.8f);
     yerine artık GetEffect<T>() template metodu kullanılmaktadır.
    */
}

void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice || !m_graphicsDevice->IsInitialized()) return;

    if (m_materialManager) m_materialManager->Update();

    if (m_graphicsDevice->BeginFrame()) {
        UpdateLightsAndShadows();
        ShadowPass();
        GBufferPass();
        LightingPass(); // Bu artık m_sceneColorTexture'a yazar
        
        // UI rendering
        RenderUI();

        // Post-processing geçişi
        if (m_postProcessing) {
            m_postProcessing->Execute(m_graphicsDevice->GetCurrentCommandBuffer(), m_graphicsDevice->GetCurrentFrameIndex());
        } else {
            // Post-processing yoksa, m_sceneColorTexture'ı doğrudan swapchain'e blit et
            if (m_sceneColorTexture) {
                BlitToSwapchain(m_graphicsDevice->GetCurrentCommandBuffer(), m_sceneColorTexture.get());
            }
        }
    }
}

void RenderSubsystem::RenderUI() {
#ifdef ASTRAL_USE_IMGUI
    if (!m_owner) return;

    auto* uiSubsystem = m_owner->GetSubsystem<UISubsystem>();
    if (!uiSubsystem) return;

    VkCommandBuffer commandBuffer = GetCurrentUICommandBuffer();

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
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);
#endif
}

VkRenderPass RenderSubsystem::GetUIRenderPass() const {
#ifdef ASTRAL_USE_IMGUI
    return m_uiRenderPass;
#else
    return VK_NULL_HANDLE;
#endif
}

VkCommandBuffer RenderSubsystem::GetCurrentUICommandBuffer() const {
#ifdef ASTRAL_USE_IMGUI
    // UI command buffer'ları henüz implement edilmemiş
    // Şimdilik mevcut command buffer'ı döndür
    return m_graphicsDevice->GetCurrentCommandBuffer();
#else
    return VK_NULL_HANDLE;
#endif
}

void RenderSubsystem::OnShutdown() {
    if (m_graphicsDevice) vkDeviceWaitIdle(m_graphicsDevice->GetVulkanDevice()->GetDevice());
    
    // UI resources temizliği
#ifdef ASTRAL_USE_IMGUI
    DestroyUIResources();
#endif

    // PostProcessingSubsystem'i kapat
    if (m_postProcessing) {
        m_postProcessing->Shutdown();
        m_postProcessing.reset();
    }
    
    DestroyShadowPassResources();
    DestroyLightingPassResources();
    DestroySceneColorTexture();
    if (m_materialManager) m_materialManager->Shutdown();
    if (m_vulkanTextureManager) m_vulkanTextureManager->Shutdown();
    if (m_vulkanMeshManager) m_vulkanMeshManager->Shutdown();
    if (m_camera) m_camera->Shutdown();
    if (m_graphicsDevice) m_graphicsDevice->Shutdown();
}

void RenderSubsystem::ShadowPass() {
    try {
        auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
        if (!vulkanRenderer) {
            Logger::Warning("RenderSubsystem", "Cannot perform shadow pass: VulkanRenderer is null");
            return;
        }
        
        if (!m_ecsSubsystem) {
            Logger::Warning("RenderSubsystem", "Cannot perform shadow pass: ECSSubsystem is null");
            return;
        }
        
        if (!m_vulkanMeshManager) {
            Logger::Warning("RenderSubsystem", "Cannot perform shadow pass: VulkanMeshManager is null");
            return;
        }

        // Işığın bakış açısından frustum oluştur
        Frustum lightFrustum;
        const glm::mat4 transposed_vp = glm::transpose(m_lightSpaceMatrix);
        lightFrustum.planes[0] = transposed_vp[3] + transposed_vp[0]; // Left
        lightFrustum.planes[1] = transposed_vp[3] - transposed_vp[0]; // Right
        lightFrustum.planes[2] = transposed_vp[3] + transposed_vp[1]; // Bottom
        lightFrustum.planes[3] = transposed_vp[3] - transposed_vp[1]; // Top
        lightFrustum.planes[4] = transposed_vp[3] + transposed_vp[2]; // Near
        lightFrustum.planes[5] = transposed_vp[3] - transposed_vp[2]; // Far
        for (auto& plane : lightFrustum.planes) {
            plane = plane / glm::length(glm::vec3(plane));
        }

        std::vector<VulkanRenderer::ResolvedRenderItem> shadowCasters;
        try {
            auto view = m_ecsSubsystem->GetRegistry().view<const RenderComponent, const TransformComponent>();
            for (auto entity : view) {
                const auto& renderComp = view.get<RenderComponent>(entity);
                if (!renderComp.visible || !renderComp.castsShadows) continue;
                
                try {
                    auto mesh = m_vulkanMeshManager->GetOrCreateMesh(renderComp.modelHandle);
                    if (mesh && mesh->IsReady()) {
                        const AABB& localAABB = mesh->GetBoundingBox();
                        const glm::mat4& transform = view.get<TransformComponent>(entity).GetWorldMatrix();
                        AABB worldAABB = localAABB.Transform(transform);

                        if (lightFrustum.Intersects(worldAABB)) {
                            shadowCasters.push_back({transform, mesh, nullptr});
                        }
                    }
                } catch (const std::exception& e) {
                    Logger::Warning("RenderSubsystem", "Failed to process entity {} for shadow casting: {}", 
                                  static_cast<uint32_t>(entity), e.what());
                    continue;
                }
            }
        } catch (const std::exception& e) {
            Logger::Warning("RenderSubsystem", "Failed to iterate ECS entities for shadow pass: {}", e.what());
            return;
        }
        
        if (!shadowCasters.empty()) {
            vulkanRenderer->RecordShadowPassCommands(m_shadowFramebuffer.get(), m_lightSpaceMatrix, shadowCasters);
            Logger::Debug("RenderSubsystem", "Recorded shadow pass commands for {} shadow casters", shadowCasters.size());
        }
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Shadow pass failed: {}", e.what());
        // Don't throw here as this would crash the entire render loop
        // Just log the error and continue without shadows
    }
}

void RenderSubsystem::LightingPass() {
    auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
    if (vulkanRenderer) {
        std::vector<VulkanTexture*> gbufferInputs = {
            m_gbAlbedoTexture.get(),
            m_gbNormalTexture.get(),
            m_gbPBRTexture.get()
        };
        vulkanRenderer->RecordLightingCommands(m_graphicsDevice->GetCurrentFrameIndex(), m_sceneColorTexture.get(), gbufferInputs, m_gbDepthTexture.get());
    }
}

void RenderSubsystem::UpdateLightsAndShadows() {
    if (!m_ecsSubsystem) return;

    m_lights.clear();
    auto view = m_ecsSubsystem->GetRegistry().view<const LightComponent, const TransformComponent>();
    for (auto entity : view) {
        if (m_lights.size() >= MAX_LIGHTS) break;
        const auto& lightComp = view.get<LightComponent>(entity);
        const auto& transComp = view.get<TransformComponent>(entity);
        GPULight light{};
        light.position = transComp.position;
        light.color = lightComp.color;
        light.intensity = lightComp.intensity;
        light.range = lightComp.range;
        light.type = static_cast<int>(lightComp.type);
        glm::mat4 rotMatrix = glm::toMat4(glm::quat(transComp.rotation));
        light.direction = glm::normalize(glm::vec3(rotMatrix * glm::vec4(0, 0, -1, 0)));
        light.castsShadows = lightComp.castsShadows ? 1.0f : 0.0f;

        if (light.type == LIGHT_TYPE_DIRECTIONAL && light.castsShadows > 0.5f) {
            glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 75.0f);
            glm::mat4 lightView = glm::lookAt(light.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_lightSpaceMatrix = lightProjection * lightView;
            light.lightSpaceMatrix = m_lightSpaceMatrix;
        }
        m_lights.push_back(light);
    }

    SceneUBO uboData;
    uboData.view = m_camera->GetViewMatrix(); uboData.projection = m_camera->GetProjectionMatrix();
    uboData.inverseView = glm::inverse(uboData.view); uboData.inverseProjection = glm::inverse(uboData.projection);
    uboData.viewPosition = glm::vec4(m_camera->GetPosition(), 1.0f);
    uboData.lightCount = m_lights.size();
    memcpy(uboData.lights, m_lights.data(), m_lights.size() * sizeof(GPULight));
    void* data = m_sceneUBO->Map();
    memcpy(data, &uboData, sizeof(SceneUBO));
    m_sceneUBO->Unmap();
}

void RenderSubsystem::CreateShadowPassResources() {
    try {
        if (!m_graphicsDevice) {
            Logger::Error("RenderSubsystem", "Cannot create shadow pass resources: GraphicsDevice is null");
            throw std::runtime_error("GraphicsDevice is null");
        }

        m_shadowMapTexture = std::make_unique<VulkanTexture>();
        VkFormat depthFormat = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
        if (depthFormat == VK_FORMAT_UNDEFINED) {
            Logger::Error("RenderSubsystem", "Failed to find suitable depth format for shadow map");
            throw std::runtime_error("No suitable depth format found");
        }

        // Configure shadow map texture using the new config-based approach
        VulkanTexture::Config cfg;
        cfg.width = SHADOW_MAP_SIZE;
        cfg.height = SHADOW_MAP_SIZE;
        cfg.format = depthFormat;
        cfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        cfg.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        cfg.name = "ShadowMap";

        if (!m_shadowMapTexture->Initialize(m_graphicsDevice.get(), cfg)) {
            Logger::Error("RenderSubsystem", "Failed to initialize shadow map texture");
            throw std::runtime_error("Shadow map texture initialization failed");
        }

        m_shadowFramebuffer = std::make_unique<VulkanFramebuffer>();
        VulkanFramebuffer::Config fbConfig;
        fbConfig.device = m_graphicsDevice->GetVulkanDevice();
        
        auto* vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
        if (!vulkanRenderer) {
            Logger::Error("RenderSubsystem", "Cannot get VulkanRenderer for shadow pass");
            throw std::runtime_error("VulkanRenderer is null");
        }
        
        fbConfig.renderPass = vulkanRenderer->GetShadowRenderPass();
        if (fbConfig.renderPass == VK_NULL_HANDLE) {
            Logger::Error("RenderSubsystem", "Shadow render pass is null");
            throw std::runtime_error("Shadow render pass is null");
        }
        
        fbConfig.width = SHADOW_MAP_SIZE; 
        fbConfig.height = SHADOW_MAP_SIZE;
        fbConfig.attachments = { m_shadowMapTexture->GetImageView() };
        
        if (!m_shadowFramebuffer->Initialize(fbConfig)) {
            Logger::Error("RenderSubsystem", "Failed to initialize shadow framebuffer");
            throw std::runtime_error("Shadow framebuffer initialization failed");
        }
        
        Logger::Info("RenderSubsystem", "Shadow pass resources created successfully");
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Failed to create shadow pass resources: {}", e.what());
        // Clean up any partially created resources
        DestroyShadowPassResources();
        throw;
    }
}

void RenderSubsystem::DestroyShadowPassResources() {
    if (m_shadowFramebuffer) m_shadowFramebuffer->Shutdown();
    if (m_shadowMapTexture) m_shadowMapTexture->Shutdown();
}

// Other resource creation/destruction and pass implementations are omitted for brevity.
void RenderSubsystem::CreateGBuffer() {
        m_gbAlbedoTexture = std::make_unique<VulkanTexture>();
        VulkanTexture::Config albedoCfg;
        albedoCfg.width = width; albedoCfg.height = height;
        albedoCfg.format = VK_FORMAT_R8G8B8A8_UNORM;
        albedoCfg.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        albedoCfg.name = "GBufferAlbedo";
        m_gbAlbedoTexture->Initialize(m_graphicsDevice.get(), albedoCfg);

        m_gbNormalTexture = std::make_unique<VulkanTexture>();
        VulkanTexture::Config normalCfg;
        normalCfg.width = width; normalCfg.height = height;
        normalCfg.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        normalCfg.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        normalCfg.name = "GBufferNormal";
        m_gbNormalTexture->Initialize(m_graphicsDevice.get(), normalCfg);

        m_gbPBRTexture = std::make_unique<VulkanTexture>();
        VulkanTexture::Config pbrCfg;
        pbrCfg.width = width; pbrCfg.height = height;
        pbrCfg.format = VK_FORMAT_R8G8B8A8_UNORM;
        pbrCfg.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        pbrCfg.name = "GBufferPBR";
        m_gbPBRTexture->Initialize(m_graphicsDevice.get(), pbrCfg);

        m_gbDepthTexture = std::make_unique<VulkanTexture>();
        VulkanTexture::Config depthCfg;
        depthCfg.width = width; depthCfg.height = height;
        depthCfg.format = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
        depthCfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthCfg.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthCfg.name = "GBufferDepth";
        m_gbDepthTexture->Initialize(m_graphicsDevice.get(), depthCfg);

        Logger::Info("RenderSubsystem", "Modern G-Buffer textures created successfully ({0}x{1})", width, height);
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Failed to create G-Buffer: {}", e.what());
        // Clean up any partially created resources
        DestroyGBuffer();
        throw;
    }
}
void RenderSubsystem::DestroyGBuffer() {
    if (m_gbAlbedoTexture) m_gbAlbedoTexture->Shutdown();
    if (m_gbNormalTexture) m_gbNormalTexture->Shutdown();
    if (m_gbPBRTexture) m_gbPBRTexture->Shutdown();
    if (m_gbDepthTexture) m_gbDepthTexture->Shutdown();
}
void RenderSubsystem::CreateLightingPassResources() { /* ... */ }
void RenderSubsystem::DestroyLightingPassResources() { /* ... */ }

void RenderSubsystem::CreateSceneColorTexture(uint32_t width, uint32_t height) {
    try {
        if (!m_graphicsDevice) {
            Logger::Error("RenderSubsystem", "Cannot create scene color texture: GraphicsDevice is null");
            throw std::runtime_error("GraphicsDevice is null");
        }

        VulkanTexture::Config config;
        config.width = width;
        config.height = height;
        config.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format
        config.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        config.name = "SceneColor";
        
        m_sceneColorTexture = std::make_unique<VulkanTexture>();
        if (!m_sceneColorTexture->Initialize(m_graphicsDevice.get(), config)) {
            Logger::Error("RenderSubsystem", "Failed to create scene color texture");
            throw std::runtime_error("Scene color texture initialization failed");
        }
        
        Logger::Info("RenderSubsystem", "Created scene color texture ({0}x{1})", width, height);
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Failed to create scene color texture: {}", e.what());
        m_sceneColorTexture.reset();
        throw;
    }
}

void RenderSubsystem::CreateUIRenderPass() {
#ifdef ASTRAL_USE_IMGUI
    auto* swapchain = m_graphicsDevice->GetSwapchain();
    if (!swapchain) return;

    // Attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain->GetFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // UI üzerine çiz
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &renderPassInfo, nullptr, &m_uiRenderPass) != VK_SUCCESS) {
        Logger::Error("RenderSubsystem", "Failed to create UI render pass");
    } else {
        Logger::Info("RenderSubsystem", "UI render pass created successfully");
    }
#endif
}

void RenderSubsystem::DestroySceneColorTexture() {
    if (m_sceneColorTexture) {
        m_sceneColorTexture->Shutdown();
        m_sceneColorTexture.reset();
    }
}

void RenderSubsystem::SetVulkanRenderer(VulkanRenderer* renderer) {
    // PostProcessingSubsystem'e VulkanRenderer pointer'ını geç
    if (m_postProcessing) {
        m_postProcessing->SetVulkanRenderer(renderer);
        Logger::Info("RenderSubsystem", "VulkanRenderer pointer set for PostProcessingSubsystem");
    } else {
        Logger::Warning("RenderSubsystem", "PostProcessingSubsystem is not available, cannot set VulkanRenderer");
    }
}
void RenderSubsystem::GBufferPass() {
    if (!m_graphicsDevice || !m_ecsSubsystem) {
        Logger::Warning("RenderSubsystem", "Cannot perform G-Buffer pass: Required subsystems are null");
        return;
    }

    try {
        auto* vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
        if (!vulkanRenderer) return;

        std::vector<VulkanTexture*> targets = {
            m_gbAlbedoTexture.get(),
            m_gbNormalTexture.get(),
            m_gbPBRTexture.get()
        };
        
        // Render queue oluştur
        std::map<MeshMaterialKey, std::vector<glm::mat4>> renderQueue;
        auto view = m_ecsSubsystem->GetRegistry().view<const RenderComponent, const TransformComponent>();
        for (auto entity : view) {
            const auto& renderComp = view.get<RenderComponent>(entity);
            if (!renderComp.visible) continue;
            
            auto mesh = m_vulkanMeshManager->GetOrCreateMesh(renderComp.modelHandle);
            auto mat = m_materialManager->GetMaterial(renderComp.materialHandle);
            
            if (mesh && mat && mesh->IsReady() && mat->IsInitialized()) {
                MeshMaterialKey key{mesh, mat};
                renderQueue[key].push_back(view.get<TransformComponent>(entity).GetWorldMatrix());
            }
        }

        vulkanRenderer->RecordGBufferCommands(m_graphicsDevice->GetCurrentFrameIndex(), targets, m_gbDepthTexture.get(), renderQueue);
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "G-Buffer pass failed: {}", e.what());
    }
}

void RenderSubsystem::BlitToSwapchain(VkCommandBuffer commandBuffer, VulkanTexture* sourceTexture) {
    try {
        if (!m_graphicsDevice || !sourceTexture) {
            Logger::Error("RenderSubsystem", "Cannot blit to swapchain without graphics device or source texture!");
            throw std::invalid_argument("Invalid parameters for blit operation");
        }
        
        auto* swapchain = m_graphicsDevice->GetSwapchain();
        if (!swapchain) {
            Logger::Error("RenderSubsystem", "Cannot blit to swapchain: swapchain is null");
            throw std::runtime_error("Swapchain is null");
        }
        
        // Validate source texture
        if (sourceTexture->GetImage() == VK_NULL_HANDLE) {
            Logger::Error("RenderSubsystem", "Source texture image is null");
            throw std::runtime_error("Source texture image is null");
        }
        
        VkImage swapchainImage = swapchain->GetCurrentImage();
        if (swapchainImage == VK_NULL_HANDLE) {
            Logger::Error("RenderSubsystem", "Swapchain image is null");
            throw std::runtime_error("Swapchain image is null");
        }
        
        // Image blit için barrier
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = sourceTexture->GetImage();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Swapchain image için barrier
        barrier.image = swapchainImage;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Blit işlemi
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {static_cast<int32_t>(sourceTexture->GetWidth()),
                             static_cast<int32_t>(sourceTexture->GetHeight()), 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {static_cast<int32_t>(swapchain->GetWidth()),
                             static_cast<int32_t>(swapchain->GetHeight()), 1};
        
        vkCmdBlitImage(commandBuffer, sourceTexture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        
        // Swapchain image'i presentation için hazırla
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Source texture'ı eski haline döndür
        barrier.image = sourceTexture->GetImage();
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        Logger::Debug("RenderSubsystem", "Successfully blitted texture to swapchain");
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Failed to blit to swapchain: {}", e.what());
        // Don't throw here as this is called during normal rendering
        // Just log the error and continue
    }
}

void RenderSubsystem::DestroyUIResources() {
#ifdef ASTRAL_USE_IMGUI
    auto* device = m_graphicsDevice->GetVulkanDevice();
    if (!device) return;

    // UI render pass'i temizle
    if (m_uiRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device->GetDevice(), m_uiRenderPass, nullptr);
        m_uiRenderPass = VK_NULL_HANDLE;
    }

    // UI framebuffers'ları temizle
    for (auto& framebuffer : m_uiFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device->GetDevice(), framebuffer, nullptr);
        }
    }
    m_uiFramebuffers.clear();

    // UI command pool'ları temizle
    for (auto& commandPool : m_uiCommandPools) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device->GetDevice(), commandPool, nullptr);
        }
    }
    m_uiCommandPools.clear();

    // UI command buffer'ları temizle
    m_uiCommandBuffers.clear();

    Logger::Info("RenderSubsystem", "UI resources destroyed successfully");
#endif
}

VulkanTexture* RenderSubsystem::GetAlbedoTexture() const {
    EnsureGBufferPassInitialized();
    if (!m_gBufferPass) {
        Logger::Warning("RenderSubsystem", "GBufferPass is null, cannot get albedo texture");
        return nullptr;
    }
    return m_gBufferPass->GetAlbedoTexture();
}

VulkanTexture* RenderSubsystem::GetNormalTexture() const {
    EnsureGBufferPassInitialized();
    if (!m_gBufferPass) {
        Logger::Warning("RenderSubsystem", "GBufferPass is null, cannot get normal texture");
        return nullptr;
    }
    return m_gBufferPass->GetNormalTexture();
}

VulkanTexture* RenderSubsystem::GetPBRTexture() const {
    EnsureGBufferPassInitialized();
    if (!m_gBufferPass) {
        Logger::Warning("RenderSubsystem", "GBufferPass is null, cannot get PBR texture");
        return nullptr;
    }
    return m_gBufferPass->GetPBRTexture();
}

VulkanTexture* RenderSubsystem::GetDepthTexture() const {
    EnsureGBufferPassInitialized();
    if (!m_gBufferPass) {
        Logger::Warning("RenderSubsystem", "GBufferPass is null, cannot get depth texture");
        return nullptr;
    }
    return m_gBufferPass->GetDepthTexture();
}

VulkanFramebuffer* RenderSubsystem::GetGBufferFramebuffer() const {
    EnsureGBufferPassInitialized();
    if (!m_gBufferFramebuffer) {
        Logger::Warning("RenderSubsystem", "G-Buffer framebuffer is null");
        return nullptr;
    }
    return m_gBufferFramebuffer.get();
}

void RenderSubsystem::EnsureGBufferPassInitialized() {
    if (!m_gBufferPass) {
        Logger::Error("RenderSubsystem", "GBufferPass is not initialized");
        
        // Hata recovery: GBufferPass'i yeniden başlatmayı dene
        try {
            Logger::Info("RenderSubsystem", "Attempting to recover GBufferPass...");
            
            // GBufferPass'i oluştur ve başlat
            m_gBufferPass = std::make_unique<GBufferPass>();
            if (!m_gBufferPass->Initialize(this)) {
                Logger::Error("RenderSubsystem", "Failed to recover GBufferPass initialization");
                throw std::runtime_error("GBufferPass recovery failed");
            }
            
            // G-Buffer kaynaklarını yeniden oluştur
            CreateGBuffer();
            
            Logger::Info("RenderSubsystem", "GBufferPass successfully recovered");
            
        } catch (const std::exception& e) {
            Logger::Error("RenderSubsystem", "GBufferPass recovery failed: {}", e.what());
            // Recovery başarısız oldu, null pointer ile devam et
            // Bu durumda texture access metotları null dönecek
            return;
        }
    }
    
    // GBufferPass'in düzgün çalışıp çalışmadığını kontrol et
    try {
        // Temel texture'ların varlığını kontrol et
        if (!m_gBufferPass->GetAlbedoTexture()) {
            Logger::Warning("RenderSubsystem", "G-Buffer albedo texture is null");
        }
        if (!m_gBufferPass->GetNormalTexture()) {
            Logger::Warning("RenderSubsystem", "G-Buffer normal texture is null");
        }
        if (!m_gBufferPass->GetPBRTexture()) {
            Logger::Warning("RenderSubsystem", "G-Buffer PBR texture is null");
        }
        if (!m_gBufferPass->GetDepthTexture()) {
            Logger::Warning("RenderSubsystem", "G-Buffer depth texture is null");
        }
        
        // Framebuffer'ın varlığını kontrol et
        if (!m_gBufferFramebuffer) {
            Logger::Warning("RenderSubsystem", "G-Buffer framebuffer is null");
        }
        
    } catch (const std::exception& e) {
        Logger::Error("RenderSubsystem", "Failed to validate GBufferPass: {}", e.what());
        // Validation hatası, ama devam et
    }
}

}