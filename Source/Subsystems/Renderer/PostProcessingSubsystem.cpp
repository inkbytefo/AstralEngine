#include "PostProcessingSubsystem.h"
#include "../../Core/Engine.h"
#include "../Platform/PlatformSubsystem.h"
#include "../Asset/AssetSubsystem.h"
#include "../Asset/AssetData.h"
#include "../../Events/EventManager.h"
#include "GraphicsDevice.h"
#include "Core/VulkanDevice.h"
#include "Core/VulkanSwapchain.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include <glm/glm.hpp>
#include <array>

namespace AstralEngine {

PostProcessingSubsystem::PostProcessingSubsystem() {
    Logger::Debug("PostProcessingSubsystem", "PostProcessingSubsystem created");
}

PostProcessingSubsystem::~PostProcessingSubsystem() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("PostProcessingSubsystem", "PostProcessingSubsystem destroyed");
}

bool PostProcessingSubsystem::Initialize(RenderSubsystem* owner) {
    m_owner = owner;
    
    Logger::Info("PostProcessingSubsystem", "Initializing post-processing subsystem...");

    // GraphicsDevice ve renderer'ı al
    auto graphicsDevice = m_owner->GetGraphicsDevice();
    if (!graphicsDevice) {
        Logger::Fatal("PostProcessingSubsystem", "GraphicsDevice not available!");
        return false;
    }

    // VulkanRenderer'ı al
    m_renderer = m_owner->GetVulkanRenderer();
    if (!m_renderer) {
        Logger::Fatal("PostProcessingSubsystem", "VulkanRenderer not available!");
        return false;
    }

    // Swapchain boyutlarını al
    auto swapchain = graphicsDevice->GetSwapchain();
    if (!swapchain) {
        Logger::Fatal("PostProcessingSubsystem", "Swapchain not available!");
        return false;
    }

    auto extent = swapchain->GetExtent();
    m_width = extent.width;
    m_height = extent.height;
    
    // Boyutları kaydet
    m_currentWidth = m_width;
    m_currentHeight = m_height;

    // Post-processing render pass'ini oluştur
    CreateRenderPass();
    if (m_postProcessRenderPass == VK_NULL_HANDLE) {
        Logger::Error("PostProcessingSubsystem", "Failed to create post-processing render pass!");
        return false;
    }

    // Ping-pong framebuffer'ları oluştur
    CreateFramebuffers(m_width, m_height);
    if (!m_pingFramebuffer || !m_pongFramebuffer) {
        Logger::Error("PostProcessingSubsystem", "Failed to create ping-pong framebuffers!");
        return false;
    }

    // Full-screen pipeline'ını oluştur
    CreateFullScreenPipeline();
    if (m_fullScreenPipeline == VK_NULL_HANDLE) {
        Logger::Error("PostProcessingSubsystem", "Failed to create full-screen pipeline!");
        return false;
    }

    // Input texture'ı ayarla (RenderSubsystem'den gelen sahne rengi)
    SetInputTexture(m_owner->GetSceneColorTexture());

    // Bloom efektini oluştur ve başlat
    auto bloomEffect = std::make_unique<BloomEffect>();
    if (!bloomEffect->Initialize(m_renderer)) {
        Logger::Error("PostProcessingSubsystem", "Failed to initialize bloom effect!");
        return false;
    }
    
    // Bloom efektini efekt zincirine ekle (tonemapping'den önce)
    AddEffect(std::move(bloomEffect));
    
    // Tonemapping efektini oluştur ve başlat
    auto tonemappingEffect = std::make_unique<TonemappingEffect>();
    if (!tonemappingEffect->Initialize(m_renderer)) {
        Logger::Error("PostProcessingSubsystem", "Failed to initialize tonemapping effect!");
        return false;
    }
    
    // Tonemapping efektini efekt zincirine ekle
    AddEffect(std::move(tonemappingEffect));

    m_initialized = true;
    Logger::Info("PostProcessingSubsystem", "Post-processing subsystem initialized successfully");
    return true;
}

void PostProcessingSubsystem::Execute(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    if (!m_initialized || !m_inputTexture) {
        // Input texture yok veya efekt yoksa, doğrudan swapchain'e çiz
        if (m_owner && m_owner->GetGraphicsDevice()) {
            BlitToSwapchain(m_owner->GetGraphicsDevice()->GetCurrentCommandBuffer(), m_inputTexture);
        }
        return;
    }

    // VulkanRenderer bağımlılık kontrolü
    if (!m_renderer) {
        Logger::Error("PostProcessingSubsystem", "VulkanRenderer is not available! Skipping post-processing.");
        // VulkanRenderer yoksa, input texture'ı doğrudan swapchain'e çiz
        if (m_owner && m_owner->GetGraphicsDevice()) {
            BlitToSwapchain(m_owner->GetGraphicsDevice()->GetCurrentCommandBuffer(), m_inputTexture);
        }
        return;
    }
    
    auto* graphicsDevice = m_owner->GetGraphicsDevice();
    
    // Swapchain yeniden boyutlandırma kontrolü
    uint32_t newWidth = graphicsDevice->GetSwapchain()->GetWidth();
    uint32_t newHeight = graphicsDevice->GetSwapchain()->GetHeight();
    if (newWidth != m_currentWidth || newHeight != m_currentHeight) {
        RecreateFramebuffers();
        m_currentWidth = newWidth;
        m_currentHeight = newHeight;
    }
    
    // Aktif efektleri filtrele
    std::vector<IPostProcessingEffect*> activeEffects;
    for (auto& effect : m_effects) {
        if (effect->IsEnabled()) {
            activeEffects.push_back(effect.get());
        }
    }
    
    if (activeEffects.empty()) {
        // Aktif efekt yoksa, input texture'ı doğrudan swapchain'e kopyala
        BlitToSwapchain(commandBuffer, m_inputTexture);
        return;
    }
    
    // Efekt zincirini uygula
    VulkanTexture* currentInput = m_inputTexture;
    VulkanFramebuffer* currentOutput = m_pingFramebuffer.get();
    
    for (size_t i = 0; i < activeEffects.size(); ++i) {
        auto* effect = activeEffects[i];
        
        // Son efekt mi kontrol et
        bool isLastEffect = (i == activeEffects.size() - 1);
        
        if (isLastEffect) {
            // Son efekt, doğrudan swapchain'e çiz
            effect->RecordCommands(commandBuffer, currentInput, nullptr, frameIndex);
        } else {
            // Ara efekt, ping-pong framebuffer'ına çiz
            effect->RecordCommands(commandBuffer, currentInput, currentOutput, frameIndex);
            
            // Input/output değiştir
            currentInput = (currentOutput == m_pingFramebuffer.get()) ?
                          m_pongTexture.get() : m_pingTexture.get();
            currentOutput = (currentOutput == m_pingFramebuffer.get()) ?
                           m_pongFramebuffer.get() : m_pingFramebuffer.get();
        }
    }
    
    // Tonemapping efekti için özel loglama
    auto tonemappingIt = m_effectNameMap.find("TonemappingEffect");
    if (tonemappingIt != m_effectNameMap.end() && tonemappingIt->second->IsEnabled()) {
        Logger::Debug("PostProcessingSubsystem", "Tonemapping effect applied successfully");
    }
    
    // Bloom efekti için özel loglama
    auto bloomIt = m_effectNameMap.find("BloomEffect");
    if (bloomIt != m_effectNameMap.end() && bloomIt->second->IsEnabled()) {
        Logger::Debug("PostProcessingSubsystem", "Bloom effect applied successfully");
    }
}

void PostProcessingSubsystem::Shutdown() {
    Logger::Info("PostProcessingSubsystem", "Shutting down post-processing subsystem...");

    // Efektleri temizle
    m_effects.clear();
    m_effectNameMap.clear();

    // Vulkan kaynaklarını ters başlatma sırasında temizle
    DestroyFullScreenPipeline();
    DestroyDescriptorSets();
    DestroyFullScreenQuadBuffers();
    DestroyFramebuffers();
    DestroyRenderPass();

    m_initialized = false;
    Logger::Info("PostProcessingSubsystem", "Post-processing subsystem shutdown complete");
}

void PostProcessingSubsystem::SetInputTexture(VulkanTexture* sceneColorTexture) {
    m_inputTexture = sceneColorTexture;
    if (!m_inputTexture && m_initialized) {
        Logger::Warning("PostProcessingSubsystem", "Input texture is null!");
    }
}

void PostProcessingSubsystem::AddEffect(std::unique_ptr<IPostProcessingEffect> effect) {
    if (!effect) {
        Logger::Warning("PostProcessingSubsystem", "Attempted to add null effect!");
        return;
    }

    // Efektin adını kontrol et
    const std::string& effectName = effect->GetName();
    if (m_effectNameMap.find(effectName) != m_effectNameMap.end()) {
        Logger::Warning("PostProcessingSubsystem", "Effect with name '{}' already exists!", effectName);
        return;
    }

    // VulkanRenderer kontrolü
    if (!m_renderer) {
        Logger::Error("PostProcessingSubsystem", "Cannot add effect '{}': VulkanRenderer is not available!", effectName);
        return;
    }

    // Efekti başlat
    if (!effect->Initialize(m_renderer)) {
        Logger::Error("PostProcessingSubsystem", "Failed to initialize effect: {}", effectName);
        return;
    }

    // Efekti ekle
    m_effectNameMap[effectName] = effect.get();
    m_effects.push_back(std::move(effect));
    
    // Tonemapping efekti için özel loglama
    if (effectName == "TonemappingEffect") {
        Logger::Info("PostProcessingSubsystem", "Tonemapping effect added and initialized successfully");
        Logger::Info("PostProcessingSubsystem", "Default tonemapping parameters: exposure=1.0, gamma=2.2, type=ACES");
    }
    // Bloom efekti için özel loglama
    else if (effectName == "BloomEffect") {
        Logger::Info("PostProcessingSubsystem", "Bloom effect added and initialized successfully");
        Logger::Info("PostProcessingSubsystem", "Default bloom parameters: threshold=1.0, knee=0.5, intensity=0.5, radius=4.0, quality=medium");
    } else {
        Logger::Info("PostProcessingSubsystem", "Added effect: {}", effectName);
    }
}

void PostProcessingSubsystem::RemoveEffect(const std::string& effectName) {
    auto it = m_effectNameMap.find(effectName);
    if (it == m_effectNameMap.end()) {
        Logger::Warning("PostProcessingSubsystem", "Effect not found: {}", effectName);
        return;
    }

    // Efekti shutdown et
    it->second->Shutdown();

    // Vector'dan bul ve kaldır
    for (auto effectIt = m_effects.begin(); effectIt != m_effects.end(); ++effectIt) {
        if (effectIt->get() == it->second) {
            m_effects.erase(effectIt);
            break;
        }
    }

    // Map'ten kaldır
    m_effectNameMap.erase(it);
    
    Logger::Info("PostProcessingSubsystem", "Removed effect: {}", effectName);
}

void PostProcessingSubsystem::EnableEffect(const std::string& effectName, bool enabled) {
    auto it = m_effectNameMap.find(effectName);
    if (it == m_effectNameMap.end()) {
        Logger::Warning("PostProcessingSubsystem", "Effect not found: {}", effectName);
        return;
    }

    it->second->SetEnabled(enabled);
    Logger::Info("PostProcessingSubsystem", "Effect '{}' {}", effectName, enabled ? "enabled" : "disabled");
}

void PostProcessingSubsystem::SetVulkanRenderer(VulkanRenderer* renderer) {
    m_renderer = renderer;
    if (m_renderer) {
        Logger::Info("PostProcessingSubsystem", "VulkanRenderer pointer set successfully");
    } else {
        Logger::Warning("PostProcessingSubsystem", "VulkanRenderer pointer set to null");
    }
}

void PostProcessingSubsystem::SetTonemappingExposure(float exposure) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetExposure(exposure);
        Logger::Info("PostProcessingSubsystem", "Tonemapping exposure set to: {}", exposure);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting exposure");
    }
}

void PostProcessingSubsystem::SetTonemappingGamma(float gamma) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetGamma(gamma);
        Logger::Info("PostProcessingSubsystem", "Tonemapping gamma set to: {}", gamma);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting gamma");
    }
}

void PostProcessingSubsystem::SetTonemappingType(int tonemapper) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetTonemapper(tonemapper);
        Logger::Info("PostProcessingSubsystem", "Tonemapping type set to: {}", tonemapper);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting tonemapper type");
    }
}

void PostProcessingSubsystem::SetTonemappingContrast(float contrast) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetContrast(contrast);
        Logger::Info("PostProcessingSubsystem", "Tonemapping contrast set to: {}", contrast);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting contrast");
    }
}

void PostProcessingSubsystem::SetTonemappingBrightness(float brightness) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetBrightness(brightness);
        Logger::Info("PostProcessingSubsystem", "Tonemapping brightness set to: {}", brightness);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting brightness");
    }
}

void PostProcessingSubsystem::SetTonemappingSaturation(float saturation) {
    auto it = m_effectNameMap.find("TonemappingEffect");
    if (it != m_effectNameMap.end()) {
        auto* tonemappingEffect = static_cast<TonemappingEffect*>(it->second);
        tonemappingEffect->SetSaturation(saturation);
        Logger::Info("PostProcessingSubsystem", "Tonemapping saturation set to: {}", saturation);
    } else {
        Logger::Warning("PostProcessingSubsystem", "TonemappingEffect not found when setting saturation");
    }
}

void PostProcessingSubsystem::SetTonemappingEnabled(bool enabled) {
    EnableEffect("TonemappingEffect", enabled);
    Logger::Info("PostProcessingSubsystem", "Tonemapping effect {}", enabled ? "enabled" : "disabled");
}

void PostProcessingSubsystem::SetBloomThreshold(float threshold) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetThreshold(threshold);
        Logger::Info("PostProcessingSubsystem", "Bloom threshold set to: {}", threshold);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting threshold");
    }
}

void PostProcessingSubsystem::SetBloomKnee(float knee) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetKnee(knee);
        Logger::Info("PostProcessingSubsystem", "Bloom knee set to: {}", knee);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting knee");
    }
}

void PostProcessingSubsystem::SetBloomIntensity(float intensity) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetIntensity(intensity);
        Logger::Info("PostProcessingSubsystem", "Bloom intensity set to: {}", intensity);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting intensity");
    }
}

void PostProcessingSubsystem::SetBloomRadius(float radius) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetRadius(radius);
        Logger::Info("PostProcessingSubsystem", "Bloom radius set to: {}", radius);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting radius");
    }
}

void PostProcessingSubsystem::SetBloomQuality(int quality) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetQuality(quality);
        Logger::Info("PostProcessingSubsystem", "Bloom quality set to: {}", quality);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting quality");
    }
}

void PostProcessingSubsystem::SetBloomUseDirt(bool useDirt) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetUseDirt(useDirt);
        Logger::Info("PostProcessingSubsystem", "Bloom use dirt set to: {}", useDirt);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting use dirt");
    }
}

void PostProcessingSubsystem::SetBloomDirtIntensity(float dirtIntensity) {
    auto it = m_effectNameMap.find("BloomEffect");
    if (it != m_effectNameMap.end()) {
        auto* bloomEffect = static_cast<BloomEffect*>(it->second);
        bloomEffect->SetDirtIntensity(dirtIntensity);
        Logger::Info("PostProcessingSubsystem", "Bloom dirt intensity set to: {}", dirtIntensity);
    } else {
        Logger::Warning("PostProcessingSubsystem", "BloomEffect not found when setting dirt intensity");
    }
}

void PostProcessingSubsystem::SetBloomEnabled(bool enabled) {
    EnableEffect("BloomEffect", enabled);
    Logger::Info("PostProcessingSubsystem", "Bloom effect {}", enabled ? "enabled" : "disabled");
}

void PostProcessingSubsystem::CreateFramebuffers(uint32_t width, uint32_t height) {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create framebuffers without graphics device!");
        return;
    }

    // VulkanRenderer bağımlılık kontrolü
    if (!m_renderer) {
        Logger::Error("PostProcessingSubsystem", "VulkanRenderer is not available! Cannot create framebuffers.");
        return;
    }

    auto* graphicsDevice = m_owner->GetGraphicsDevice();
    auto* vulkanDevice = graphicsDevice->GetVulkanDevice();
    
    // HDR texture formatı
    VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    // Ping texture'ını oluştur
    VulkanTexture::Config pingConfig;
    pingConfig.width = width;
    pingConfig.height = height;
    pingConfig.format = hdrFormat;
    pingConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    pingConfig.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pingConfig.name = "PostProcessing_Ping";
    
    auto pingTexture = std::make_unique<VulkanTexture>();
    if (!pingTexture->Initialize(vulkanDevice, pingConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create ping texture for post-processing");
        return;
    }
    
    // Pong texture'ını oluştur
    VulkanTexture::Config pongConfig;
    pongConfig.width = width;
    pongConfig.height = height;
    pongConfig.format = hdrFormat;
    pongConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    pongConfig.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pongConfig.name = "PostProcessing_Pong";
    
    auto pongTexture = std::make_unique<VulkanTexture>();
    if (!pongTexture->Initialize(vulkanDevice, pongConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create pong texture for post-processing");
        return;
    }
    
    // Ping framebuffer'ını oluştur
    VulkanFramebuffer::Config pingFbConfig;
    pingFbConfig.device = vulkanDevice;
    pingFbConfig.renderPass = m_postProcessRenderPass;
    pingFbConfig.width = width;
    pingFbConfig.height = height;
    pingFbConfig.layers = 1;
    pingFbConfig.attachments = { pingTexture->GetImageView() };
    pingFbConfig.name = "PostProcessing_Ping_Framebuffer";
    
    m_pingFramebuffer = std::make_unique<VulkanFramebuffer>();
    if (!m_pingFramebuffer->Initialize(pingFbConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create ping framebuffer for post-processing: {}",
                     m_pingFramebuffer->GetLastError());
        return;
    }
    
    // Pong framebuffer'ını oluştur
    VulkanFramebuffer::Config pongFbConfig;
    pongFbConfig.device = vulkanDevice;
    pongFbConfig.renderPass = m_postProcessRenderPass;
    pongFbConfig.width = width;
    pongFbConfig.height = height;
    pongFbConfig.layers = 1;
    pongFbConfig.attachments = { pongTexture->GetImageView() };
    pongFbConfig.name = "PostProcessing_Pong_Framebuffer";
    
    m_pongFramebuffer = std::make_unique<VulkanFramebuffer>();
    if (!m_pongFramebuffer->Initialize(pongFbConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create pong framebuffer for post-processing: {}",
                     m_pongFramebuffer->GetLastError());
        return;
    }
    
    // Texture'ları framebuffer'lara taşı
    m_pingTexture = std::move(pingTexture);
    m_pongTexture = std::move(pongTexture);
    
    Logger::Info("PostProcessingSubsystem", "Created ping-pong framebuffers for post-processing ({0}x{1})", width, height);
}

void PostProcessingSubsystem::CreateRenderPass() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create render pass without graphics device!");
        return;
    }

    auto device = m_owner->GetGraphicsDevice()->GetVulkanDevice();
    if (!device) {
        Logger::Error("PostProcessingSubsystem", "Vulkan device not available!");
        return;
    }

    // Color attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Color attachment reference
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Render pass create info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_postProcessRenderPass) != VK_SUCCESS) {
        Logger::Error("PostProcessingSubsystem", "Failed to create render pass!");
        m_postProcessRenderPass = VK_NULL_HANDLE;
        return;
    }

    Logger::Info("PostProcessingSubsystem", "Created post-processing render pass");
}

void PostProcessingSubsystem::CreateFullScreenPipeline() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create pipeline without graphics device!");
        return;
    }

    auto device = m_owner->GetGraphicsDevice();
    auto vulkanDevice = device->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("PostProcessingSubsystem", "Vulkan device not available!");
        return;
    }

    // VulkanRenderer bağımlılık kontrolü
    if (!m_renderer) {
        Logger::Error("PostProcessingSubsystem", "VulkanRenderer is not available! Cannot create full-screen pipeline.");
        return;
    }

    Logger::Info("PostProcessingSubsystem", "Creating full-screen pipeline...");

    // 1. Descriptor set layout oluştur (texture sampling için)
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    // Input texture binding (binding = 0)
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    textureBinding.pImmutableSamplers = nullptr;
    bindings.push_back(textureBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkanDevice, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        Logger::Error("PostProcessingSubsystem", "Failed to create descriptor set layout!");
        return;
    }

    // 2. Pipeline layout oluştur
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutInfo, nullptr, &m_fullScreenPipelineLayout) != VK_SUCCESS) {
        Logger::Error("PostProcessingSubsystem", "Failed to create pipeline layout!");
        return;
    }

    // 3. Shader modüllerini yükle
    // Bloom shader'larını genel amaçlı tam ekran quad shader'ı olarak kullanacağız
    m_vertexShader = std::make_unique<VulkanShader>();
    m_fragmentShader = std::make_unique<VulkanShader>();

    // Shader SPIR-V kodlarını dosyalardan yükle
    std::vector<uint32_t> vertexSpirvCode;
    std::vector<uint32_t> fragmentSpirvCode;
    
    // Base path'i al (shader dosyalarını bulmak için)
    // Not: RenderSubsystem üzerinden executable path'e erişim gerekebilir
    // Şimdilik boş bir path kullanıyoruz, ileride RenderSubsystem'e bu metot eklenebilir
    std::filesystem::path basePath = std::filesystem::current_path();
    
    // Vertex shader dosyasını yükle
    std::filesystem::path vertexShaderPath = basePath / "Assets/Shaders/PostProcessing/bloom.spv";
    if (!LoadShaderCode(vertexShaderPath.string(), vertexSpirvCode)) {
        Logger::Error("PostProcessingSubsystem", "Failed to load vertex shader from: {}", vertexShaderPath.string());
        return;
    }
    
    // Fragment shader dosyasını yükle
    std::filesystem::path fragmentShaderPath = basePath / "Assets/Shaders/PostProcessing/bloom_frag.spv";
    if (!LoadShaderCode(fragmentShaderPath.string(), fragmentSpirvCode)) {
        Logger::Error("PostProcessingSubsystem", "Failed to load fragment shader from: {}", fragmentShaderPath.string());
        return;
    }

    if (!m_vertexShader->Initialize(vulkanDevice, vertexSpirvCode, VK_SHADER_STAGE_VERTEX_BIT)) {
        Logger::Error("PostProcessingSubsystem", "Failed to initialize vertex shader!");
        return;
    }

    if (!m_fragmentShader->Initialize(vulkanDevice, fragmentSpirvCode, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        Logger::Error("PostProcessingSubsystem", "Failed to initialize fragment shader!");
        return;
    }

    // 4. Tam ekran quad için vertex ve index buffer'ları oluştur
    CreateFullScreenQuadBuffers();

    // 5. Descriptor pool ve set'leri oluştur
    if (!CreateDescriptorPoolAndSets()) {
        Logger::Error("PostProcessingSubsystem", "Failed to create descriptor pool and sets!");
        return;
    }

    // 6. Descriptor set'i input texture ile güncelle
    if (m_inputTexture) {
        UpdateDescriptorSet(m_inputTexture);
    }

    // 7. Graphics pipeline oluştur
    if (!CreateGraphicsPipeline()) {
        Logger::Error("PostProcessingSubsystem", "Failed to create graphics pipeline!");
        return;
    }

    Logger::Info("PostProcessingSubsystem", "Full-screen pipeline created successfully");
}

bool PostProcessingSubsystem::CreateDescriptorPoolAndSets() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create descriptor pool without graphics device!");
        return false;
    }

    auto device = m_owner->GetGraphicsDevice();
    auto vulkanDevice = device->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("PostProcessingSubsystem", "Vulkan device not available!");
        return false;
    }

    Logger::Info("PostProcessingSubsystem", "Creating descriptor pool and sets...");

    // Descriptor pool oluşturma
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    // Combined image sampler için pool size
    VkDescriptorPoolSize samplerPoolSize{};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = 1; // Sadece input texture için
    poolSizes.push_back(samplerPoolSize);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1; // Sadece bir descriptor set

    if (vkCreateDescriptorPool(vulkanDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        Logger::Error("PostProcessingSubsystem", "Failed to create descriptor pool!");
        return false;
    }

    // Descriptor set allocation
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        Logger::Error("PostProcessingSubsystem", "Failed to allocate descriptor sets!");
        return false;
    }

    Logger::Info("PostProcessingSubsystem", "Descriptor pool and sets created successfully");
    return true;
}

bool PostProcessingSubsystem::UpdateDescriptorSet(VulkanTexture* inputTexture) {
    if (!m_owner || !m_owner->GetGraphicsDevice() || !inputTexture) {
        Logger::Error("PostProcessingSubsystem", "Cannot update descriptor set without valid parameters!");
        return false;
    }

    auto device = m_owner->GetGraphicsDevice();
    auto vulkanDevice = device->GetVulkanDevice();
    if (!vulkanDevice || m_descriptorSet == VK_NULL_HANDLE) {
        Logger::Error("PostProcessingSubsystem", "Invalid descriptor set or device!");
        return false;
    }

    // Input texture için descriptor info
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = inputTexture->GetImageView();
    imageInfo.sampler = inputTexture->GetSampler();

    // Write descriptor set
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0; // Input texture binding
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    // Descriptor set'i güncelle
    vkUpdateDescriptorSets(vulkanDevice, 1, &descriptorWrite, 0, nullptr);

    Logger::Info("PostProcessingSubsystem", "Descriptor set updated successfully");
    return true;
}

void PostProcessingSubsystem::CreateFullScreenQuadBuffers() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create quad buffers without graphics device!");
        return;
    }

    auto device = m_owner->GetGraphicsDevice();
    auto vulkanDevice = device->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("PostProcessingSubsystem", "Vulkan device not available!");
        return;
    }

    Logger::Info("PostProcessingSubsystem", "Creating full-screen quad buffers...");

    // Tam ekran quad vertex verileri - AssetData.h'daki Vertex yapısını kullan
    std::vector<Vertex> vertices = {
        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)},  // Sol alt
        {glm::vec3( 1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)},  // Sağ alt
        {glm::vec3( 1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)},  // Sağ üst
        {glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)}   // Sol üst
    };

    std::vector<uint16_t> indices = {
        0, 1, 2,  // İlk üçgen
        0, 2, 3   // İkinci üçgen
    };

    m_indexCount = static_cast<uint32_t>(indices.size());

    // Vertex buffer'ı oluştur
    VulkanBuffer::Config vertexBufferConfig{};
    vertexBufferConfig.size = sizeof(Vertex) * vertices.size();
    vertexBufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexBufferConfig.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_vertexBuffer = std::make_unique<VulkanBuffer>();
    if (!m_vertexBuffer->Initialize(vulkanDevice, vertexBufferConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create vertex buffer!");
        return;
    }

    // Index buffer'ı oluştur
    VulkanBuffer::Config indexBufferConfig{};
    indexBufferConfig.size = sizeof(uint16_t) * indices.size();
    indexBufferConfig.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexBufferConfig.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_indexBuffer = std::make_unique<VulkanBuffer>();
    if (!m_indexBuffer->Initialize(vulkanDevice, indexBufferConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create index buffer!");
        return;
    }

    // Vertex verilerini buffer'a kopyala (staging buffer ile)
    // Not: Gerçek implementasyonda burada staging buffer kullanılmalı
    // Şimdilik doğrudan map ile kopyalıyoruz (basitlik için)
    void* vertexData = m_vertexBuffer->Map();
    if (vertexData) {
        memcpy(vertexData, vertices.data(), sizeof(Vertex) * vertices.size());
        m_vertexBuffer->Unmap();
    }

    // Index verilerini buffer'a kopyala
    void* indexData = m_indexBuffer->Map();
    if (indexData) {
        memcpy(indexData, indices.data(), sizeof(uint16_t) * indices.size());
        m_indexBuffer->Unmap();
    }

    Logger::Info("PostProcessingSubsystem", "Full-screen quad buffers created successfully ({} vertices, {} indices)",
                vertices.size(), indices.size());
}

bool PostProcessingSubsystem::CreateGraphicsPipeline() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        Logger::Error("PostProcessingSubsystem", "Cannot create graphics pipeline without graphics device!");
        return false;
    }

    auto device = m_owner->GetGraphicsDevice();
    auto vulkanDevice = device->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("PostProcessingSubsystem", "Vulkan device not available!");
        return false;
    }

    Logger::Info("PostProcessingSubsystem", "Creating graphics pipeline...");

    try {
        // Shader aşamalarını oluştur
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        // Vertex shader stage
        VkPipelineShaderStageCreateInfo vertexStageInfo{};
        vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStageInfo.module = m_vertexShader->GetModule();
        vertexStageInfo.pName = "main";
        shaderStages.push_back(vertexStageInfo);

        // Fragment shader stage
        VkPipelineShaderStageCreateInfo fragmentStageInfo{};
        fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStageInfo.module = m_fragmentShader->GetModule();
        fragmentStageInfo.pName = "main";
        shaderStages.push_back(fragmentStageInfo);

        // Vertex input state (tam ekran quad için)
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2); // position + texcoord
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        // Position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;
        // TexCoord attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(glm::vec3);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Input assembly state
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport ve scissor (dynamic state)
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Viewport state
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr; // Dynamic state
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr; // Dynamic state

        // Rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Tam ekran quad için culling yok
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // Multisample state
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;

        // Depth stencil state (post-processing'te depth test genellikle kapalıdır)
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depthStencil.stencilTestEnable = VK_FALSE;

        // Color blend state
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // Başlangıçta blending kapalı

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Graphics pipeline create info
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_fullScreenPipelineLayout;
        pipelineInfo.renderPass = m_postProcessRenderPass;
        pipelineInfo.subpass = 0;

        // VulkanPipeline sınıfını kullanarak pipeline oluştur
        VulkanPipeline::Config pipelineConfig;
        pipelineConfig.shaders = { m_vertexShader.get(), m_fragmentShader.get() };
        pipelineConfig.swapchain = m_owner->GetGraphicsDevice()->GetSwapchain();
        pipelineConfig.extent = { m_width, m_height };
        pipelineConfig.descriptorSetLayout = m_descriptorSetLayout;
        pipelineConfig.useMinimalVertexInput = false;
        
        m_fullScreenPipeline = std::make_unique<VulkanPipeline>();
        if (!m_fullScreenPipeline->Initialize(vulkanDevice, pipelineConfig)) {
            Logger::Error("PostProcessingSubsystem", "Failed to initialize VulkanPipeline: {}", m_fullScreenPipeline->GetLastError());
            return false;
        }

        Logger::Info("PostProcessingSubsystem", "Graphics pipeline created successfully");
        return true;

    } catch (const std::exception& e) {
        Logger::Error("PostProcessingSubsystem", "Exception during graphics pipeline creation: {}", e.what());
        return false;
    }
}

void PostProcessingSubsystem::DestroyFramebuffers() {
    if (m_pingFramebuffer) {
        m_pingFramebuffer->Shutdown();
        m_pingFramebuffer.reset();
    }
    
    if (m_pongFramebuffer) {
        m_pongFramebuffer->Shutdown();
        m_pongFramebuffer.reset();
    }
    
    if (m_pingTexture) {
        m_pingTexture->Shutdown();
        m_pingTexture.reset();
    }
    
    if (m_pongTexture) {
        m_pongTexture->Shutdown();
        m_pongTexture.reset();
    }
    
    Logger::Info("PostProcessingSubsystem", "Destroyed ping-pong framebuffers for post-processing");
}

void PostProcessingSubsystem::DestroyRenderPass() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        return;
    }

    auto device = m_owner->GetGraphicsDevice()->GetVulkanDevice();
    if (device && m_postProcessRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_postProcessRenderPass, nullptr);
        m_postProcessRenderPass = VK_NULL_HANDLE;
        Logger::Info("PostProcessingSubsystem", "Destroyed post-processing render pass");
    }
}

void PostProcessingSubsystem::DestroyFullScreenQuadBuffers() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        return;
    }

    // Vertex buffer'ı temizle
    if (m_vertexBuffer) {
        m_vertexBuffer->Shutdown();
        m_vertexBuffer.reset();
    }
    
    // Index buffer'ı temizle
    if (m_indexBuffer) {
        m_indexBuffer->Shutdown();
        m_indexBuffer.reset();
    }
    
    m_indexCount = 0;
    
    Logger::Info("PostProcessingSubsystem", "Destroyed full-screen quad buffers");
}

void PostProcessingSubsystem::DestroyDescriptorSets() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        return;
    }

    auto device = m_owner->GetGraphicsDevice()->GetVulkanDevice();
    if (device) {
        // Descriptor pool'u temizle
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        
        // Descriptor set'leri temizle (otomatik olarak pool ile birlikte silinir)
        m_descriptorSet = VK_NULL_HANDLE;
        
        Logger::Info("PostProcessingSubsystem", "Destroyed descriptor sets and pool");
    }
}

void PostProcessingSubsystem::DestroyFullScreenPipeline() {
    if (!m_owner || !m_owner->GetGraphicsDevice()) {
        return;
    }

    auto device = m_owner->GetGraphicsDevice()->GetVulkanDevice();
    if (device) {
        // Pipeline'ı temizle
        if (m_fullScreenPipeline) {
            m_fullScreenPipeline->Shutdown();
            m_fullScreenPipeline.reset();
        }
        
        // Pipeline layout'ı temizle
        if (m_fullScreenPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_fullScreenPipelineLayout, nullptr);
            m_fullScreenPipelineLayout = VK_NULL_HANDLE;
        }
        
        // Shader'ları temizle
        if (m_vertexShader) {
            m_vertexShader->Shutdown();
            m_vertexShader.reset();
        }
        
        if (m_fragmentShader) {
            m_fragmentShader->Shutdown();
            m_fragmentShader.reset();
        }
        
        // Descriptor set layout'ı temizle
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        Logger::Info("PostProcessingSubsystem", "Destroyed full-screen pipeline");
    }
}

void PostProcessingSubsystem::ProcessEffectChain(uint32_t frameIndex) {
    if (!m_inputTexture || m_effects.empty()) {
        return;
    }

    // Aktif efektleri filtrele
    std::vector<IPostProcessingEffect*> activeEffects;
    for (const auto& effect : m_effects) {
        if (effect->IsEnabled()) {
            activeEffects.push_back(effect.get());
        }
    }

    if (activeEffects.empty()) {
        return;
    }

    // Not: Bu metod ileride implemente edilecek
    // Command buffer kaydetme ve ping-pong mantığı burada olacak
    // Şimdilik placeholder implementation
    
    Logger::Debug("PostProcessingSubsystem", "Processing {} active effects", activeEffects.size());
}

void PostProcessingSubsystem::RecreateFramebuffers() {
    // Önceki framebuffer'ları temizle
    DestroyFramebuffers();
    
    // Yeni boyutlarla framebuffer'ları yeniden oluştur
    auto* swapchain = m_owner->GetGraphicsDevice()->GetSwapchain();
    CreateFramebuffers(swapchain->GetWidth(), swapchain->GetHeight());
}

void PostProcessingSubsystem::BlitToSwapchain(VkCommandBuffer commandBuffer, VulkanTexture* sourceTexture) {
    auto* graphicsDevice = m_owner->GetGraphicsDevice();
    auto* swapchain = graphicsDevice->GetSwapchain();
    
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
    VkImage swapchainImage = swapchain->GetCurrentImage();
    
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
}

bool PostProcessingSubsystem::LoadShaderCode(const std::string& filePath, std::vector<uint32_t>& spirvCode) {
    Logger::Info("PostProcessingSubsystem", "Loading shader code from: {}", filePath);
    
    // Dosyayı binary modda aç
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        Logger::Error("PostProcessingSubsystem", "Failed to open shader file: {}", filePath);
        return false;
    }
    
    // Dosya boyutunu al
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);
    
    // Dosya boyutu 4 byte'ın katı olmalı (SPIR-V 32-bit word'lerden oluşur)
    if (fileSize % sizeof(uint32_t) != 0) {
        Logger::Error("PostProcessingSubsystem", "Invalid SPIR-V file size: {} (must be multiple of 4)", fileSize);
        return false;
    }
    
    // Buffer'ı ayır ve veriyi oku
    spirvCode.resize(fileSize / sizeof(uint32_t));
    if (!file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize)) {
        Logger::Error("PostProcessingSubsystem", "Failed to read shader file: {}", filePath);
        return false;
    }
    
    // SPIR-V magic number'ını kontrol et
    if (spirvCode[0] != 0x07230203) {
        Logger::Error("PostProcessingSubsystem", "Invalid SPIR-V magic number in file: {}", filePath);
        return false;
    }
    
    Logger::Info("PostProcessingSubsystem", "Successfully loaded shader code ({} words)", spirvCode.size());
    return true;
}

/* Test için örnek kullanım kodu:
   
   // Tonemapping efektini kullanmak için örnek kod:
   
   // 1. Tonemapping efektini etkinleştir/devre dışı bırak
   postProcessingSubsystem->SetTonemappingEnabled(true);
   
   // 2. Tonemapping parametrelerini ayarla
   postProcessingSubsystem->SetTonemappingExposure(1.2f);      // Varsayılan: 1.0f
   postProcessingSubsystem->SetTonemappingGamma(2.4f);        // Varsayılan: 2.2f
   postProcessingSubsystem->SetTonemappingType(0);            // 0: ACES, 1: Reinhard, 2: Filmic, 3: Custom
   postProcessingSubsystem->SetTonemappingContrast(1.1f);     // Varsayılan: 1.0f
   postProcessingSubsystem->SetTonemappingBrightness(0.1f);   // Varsayılan: 0.0f
   postProcessingSubsystem->SetTonemappingSaturation(1.2f);   // Varsayılan: 1.0f
   
   // 3. Farklı tonemapping operatörlerini test et
   // ACES (varsayılan)
   postProcessingSubsystem->SetTonemappingType(0);
   
   // Reinhard
   postProcessingSubsystem->SetTonemappingType(1);
   
   // Filmic
   postProcessingSubsystem->SetTonemappingType(2);
   
   // Custom
   postProcessingSubsystem->SetTonemappingType(3);
   
   // 4. HDR içeriği için ayarlar
   postProcessingSubsystem->SetTonemappingExposure(1.5f);  // Daha parlak görüntü
   postProcessingSubsystem->SetTonemappingGamma(2.2f);    // Standart gamma
   
   // 5. Gece sahneleri için ayarlar
   postProcessingSubsystem->SetTonemappingExposure(0.8f);  // Daha karanlık görüntü
   postProcessingSubsystem->SetTonemappingBrightness(0.2f); // Biraz daha parlaklık
   postProcessingSubsystem->SetTonemappingContrast(1.3f);  // Daha yüksek kontrast
   
   // Bloom efektini kullanmak için örnek kod:
   
   // 1. Bloom efektini etkinleştir/devre dışı bırak
   postProcessingSubsystem->SetBloomEnabled(true);
   
   // 2. Bloom parametrelerini ayarla
   postProcessingSubsystem->SetBloomThreshold(1.0f);        // Varsayılan: 1.0f - Parlaklık eşiği
   postProcessingSubsystem->SetBloomKnee(0.5f);             // Varsayılan: 0.5f - Eşik yumuşatma
   postProcessingSubsystem->SetBloomIntensity(0.5f);        // Varsayılan: 0.5f - Bloom yoğunluğu
   postProcessingSubsystem->SetBloomRadius(4.0f);           // Varsayılan: 4.0f - Bulanıklık yarıçapı
   postProcessingSubsystem->SetBloomQuality(1);             // Varsayılan: 1 (medium) - 0: low, 1: medium, 2: high
   postProcessingSubsystem->SetBloomUseDirt(false);         // Varsayılan: false - Lens dirt kullanımı
   postProcessingSubsystem->SetBloomDirtIntensity(1.0f);    // Varsayılan: 1.0f - Lens dirt yoğunluğu
   
   // 3. Farklı Bloom kalite seviyeleri test et
   // Düşük kalite (performans odaklı)
   postProcessingSubsystem->SetBloomQuality(0);
   
   // Orta kalite (varsayılan)
   postProcessingSubsystem->SetBloomQuality(1);
   
   // Yüksek kalite (görsel kalite odaklı)
   postProcessingSubsystem->SetBloomQuality(2);
   
   // 4. Farklı sahneler için Bloom ayarları
   // Parlak gün ışığı sahneleri
   postProcessingSubsystem->SetBloomThreshold(1.2f);  // Daha yüksek eşik
   postProcessingSubsystem->SetBloomIntensity(0.3f);  // Daha düşük yoğunluk
   postProcessingSubsystem->SetBloomRadius(3.0f);     // Daha az bulanıklık
   
   // Karanlık gece sahneleri (ışık kaynakleri için)
   postProcessingSubsystem->SetBloomThreshold(0.8f);  // Daha düşük eşik
   postProcessingSubsystem->SetBloomIntensity(0.8f);  // Daha yüksek yoğunluk
   postProcessingSubsystem->SetBloomRadius(5.0f);     // Daha fazla bulanıklık
   
   // Neon ve parlak objeler
   postProcessingSubsystem->SetBloomThreshold(0.5f);  // Çok düşük eşik
   postProcessingSubsystem->SetBloomIntensity(1.2f);  // Yüksek yoğunluk
   postProcessingSubsystem->SetBloomRadius(6.0f);     // Geniş bulanıklık
   
   // 5. Bloom ve Tonemapping birlikte kullanım
   // Bloom ve tonemapping efektlerini birlikte etkinleştir
   postProcessingSubsystem->SetBloomEnabled(true);
   postProcessingSubsystem->SetTonemappingEnabled(true);
   
   // HDR sahneleri için optimizasyon
   postProcessingSubsystem->SetBloomThreshold(1.1f);
   postProcessingSubsystem->SetBloomIntensity(0.6f);
   postProcessingSubsystem->SetTonemappingExposure(1.3f);
   postProcessingSubsystem->SetTonemappingType(0); // ACES
   
   // 6. Lens dirt efekti ile Bloom
   postProcessingSubsystem->SetBloomUseDirt(true);
   postProcessingSubsystem->SetBloomDirtIntensity(1.5f);
   
   // 7. Bloom efektini dinamik olarak değiştirme (örneğin gün/gece döngüsü için)
   // Gündüz ayarları
   postProcessingSubsystem->SetBloomThreshold(1.2f);
   postProcessingSubsystem->SetBloomIntensity(0.4f);
   
   // Gece ayarları
   postProcessingSubsystem->SetBloomThreshold(0.7f);
   postProcessingSubsystem->SetBloomIntensity(0.9f);
   
   // 8. Performans testi için
   // Düşük kalite ve minimum yoğunluk
   postProcessingSubsystem->SetBloomQuality(0);
   postProcessingSubsystem->SetBloomIntensity(0.2f);
   
   // Yüksek kalite ve maksimum yoğunluk
   postProcessingSubsystem->SetBloomQuality(2);
   postProcessingSubsystem->SetBloomIntensity(1.5f);
*/

} // namespace AstralEngine