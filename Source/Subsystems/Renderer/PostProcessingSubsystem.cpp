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
        Logger::Critical("PostProcessingSubsystem", "GraphicsDevice not available!");
        return false;
    }

    // VulkanRenderer'ı al
    m_renderer = m_owner->GetVulkanRenderer();
    if (!m_renderer) {
        Logger::Critical("PostProcessingSubsystem", "VulkanRenderer not available!");
        return false;
    }

    // Swapchain boyutlarını al
    auto swapchain = graphicsDevice->GetSwapchain();
    if (!swapchain) {
        Logger::Critical("PostProcessingSubsystem", "Swapchain not available!");
        return false;
    }

    auto extent = swapchain->GetExtent();
    m_width = extent.width;
    m_height = extent.height;
    
    // Boyutları kaydet
    m_currentWidth = m_width;
    m_currentHeight = m_height;

    // Ping-pong texture'ları oluştur
    CreateFramebuffers(m_width, m_height);
    if (!m_pingTexture || !m_pongTexture) {
        Logger::Error("PostProcessingSubsystem", "Failed to create ping-pong textures!");
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
        // Bu işlem VulkanRenderer tarafından yapılacak
        return;
    }

    // VulkanRenderer bağımlılık kontrolü
    if (!m_renderer) {
        Logger::Error("PostProcessingSubsystem", "VulkanRenderer is not available! Skipping post-processing.");
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
        // Bu işlem VulkanRenderer tarafından yapılacak
        return;
    }
    
    // Efekt zincirini uygula
    VulkanTexture* currentInput = m_inputTexture;
    VulkanTexture* currentOutput = m_pingTexture.get();
    
    for (size_t i = 0; i < activeEffects.size(); ++i) {
        auto* effect = activeEffects[i];
        
        // Son efekt mi kontrol et
        bool isLastEffect = (i == activeEffects.size() - 1);
        
        // Efekt durumunu güncelle
        effect->Update(currentInput, frameIndex);
        
        // Descriptor set'leri güncelle
        effect->UpdateDescriptorSets(currentInput, frameIndex);
        
        // Render işlemini VulkanRenderer'a delegasyon et
        if (isLastEffect) {
            // Son efekt, doğrudan swapchain'e çiz
            m_renderer->RenderPostProcessingEffect(commandBuffer, effect, currentInput, nullptr, frameIndex);
        } else {
            // Ara efekt, ping-pong texture'ına çiz
            m_renderer->RenderPostProcessingEffect(commandBuffer, effect, currentInput, currentOutput, frameIndex);
            
            // Input/output değiştir
            currentInput = (currentOutput == m_pingTexture.get()) ?
                          m_pongTexture.get() : m_pingTexture.get();
            currentOutput = (currentOutput == m_pingTexture.get()) ?
                           m_pongTexture.get() : m_pingTexture.get();
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
    DestroyFramebuffers();

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
    if (!pingTexture->Initialize(graphicsDevice, pingConfig)) {
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
    if (!pongTexture->Initialize(graphicsDevice, pongConfig)) {
        Logger::Error("PostProcessingSubsystem", "Failed to create pong texture for post-processing");
        return;
    }
    
    // Texture'ları sınıf üyelerine taşı
    m_pingTexture = std::move(pingTexture);
    m_pongTexture = std::move(pongTexture);
    
    Logger::Info("PostProcessingSubsystem", "Created ping-pong textures for post-processing ({0}x{1})", width, height);
}







void PostProcessingSubsystem::DestroyFramebuffers() {
    if (m_pingTexture) {
        m_pingTexture->Shutdown();
        m_pingTexture.reset();
    }
    
    if (m_pongTexture) {
        m_pongTexture->Shutdown();
        m_pongTexture.reset();
    }
    
    Logger::Info("PostProcessingSubsystem", "Destroyed ping-pong textures for post-processing");
}






void PostProcessingSubsystem::RecreateFramebuffers() {
    // Önceki framebuffer'ları temizle
    DestroyFramebuffers();
    
    // Yeni boyutlarla framebuffer'ları yeniden oluştur
    auto* swapchain = m_owner->GetGraphicsDevice()->GetSwapchain();
    CreateFramebuffers(swapchain->GetWidth(), swapchain->GetHeight());
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
   postProcessingSubsystem->EnableEffect("TonemappingEffect", true);
   
   // 2. Tonemapping parametrelerini ayarla
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetExposure(1.2f);      // Varsayılan: 1.0f
       tonemapping->SetGamma(2.4f);        // Varsayılan: 2.2f
       tonemapping->SetTonemapper(0);       // 0: ACES, 1: Reinhard, 2: Filmic, 3: Custom
       tonemapping->SetContrast(1.1f);     // Varsayılan: 1.0f
       tonemapping->SetBrightness(0.1f);   // Varsayılan: 0.0f
       tonemapping->SetSaturation(1.2f);   // Varsayılan: 1.0f
   }
   
   // 3. Farklı tonemapping operatörlerini test et
   // ACES (varsayılan)
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetTonemapper(0);
   }
   
   // Reinhard
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetTonemapper(1);
   }
   
   // Filmic
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetTonemapper(2);
   }
   
   // Custom
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetTonemapper(3);
   }
   
   // 4. HDR içeriği için ayarlar
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetExposure(1.5f);  // Daha parlak görüntü
       tonemapping->SetGamma(2.2f);    // Standart gamma
   }
   
   // 5. Gece sahneleri için ayarlar
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetExposure(0.8f);  // Daha karanlık görüntü
       tonemapping->SetBrightness(0.2f); // Biraz daha parlaklık
       tonemapping->SetContrast(1.3f);  // Daha yüksek kontrast
   }
   
   // Bloom efektini kullanmak için örnek kod:
   
   // 1. Bloom efektini etkinleştir/devre dışı bırak
   postProcessingSubsystem->EnableEffect("BloomEffect", true);
   
   // 2. Bloom parametrelerini ayarla
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(1.0f);        // Varsayılan: 1.0f - Parlaklık eşiği
       bloom->SetKnee(0.5f);             // Varsayılan: 0.5f - Eşik yumuşatma
       bloom->SetIntensity(0.5f);        // Varsayılan: 0.5f - Bloom yoğunluğu
       bloom->SetRadius(4.0f);           // Varsayılan: 4.0f - Bulanıklık yarıçapı
       bloom->SetQuality(1);             // Varsayılan: 1 (medium) - 0: low, 1: medium, 2: high
       bloom->SetUseDirt(false);         // Varsayılan: false - Lens dirt kullanımı
       bloom->SetDirtIntensity(1.0f);    // Varsayılan: 1.0f - Lens dirt yoğunluğu
   }
   
   // 3. Farklı Bloom kalite seviyeleri test et
   // Düşük kalite (performans odaklı)
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetQuality(0);
   }
   
   // Orta kalite (varsayılan)
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetQuality(1);
   }
   
   // Yüksek kalite (görsel kalite odaklı)
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetQuality(2);
   }
   
   // 4. Farklı sahneler için Bloom ayarları
   // Parlak gün ışığı sahneleri
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(1.2f);  // Daha yüksek eşik
       bloom->SetIntensity(0.3f);  // Daha düşük yoğunluk
       bloom->SetRadius(3.0f);     // Daha az bulanıklık
   }
   
   // Karanlık gece sahneleri (ışık kaynakleri için)
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(0.8f);  // Daha düşük eşik
       bloom->SetIntensity(0.8f);  // Daha yüksek yoğunluk
       bloom->SetRadius(5.0f);     // Daha fazla bulanıklık
   }
   
   // Neon ve parlak objeler
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(0.5f);  // Çok düşük eşik
       bloom->SetIntensity(1.2f);  // Yüksek yoğunluk
       bloom->SetRadius(6.0f);     // Geniş bulanıklık
   }
   
   // 5. Bloom ve Tonemapping birlikte kullanım
   // Bloom ve tonemapping efektlerini birlikte etkinleştir
   postProcessingSubsystem->EnableEffect("BloomEffect", true);
   postProcessingSubsystem->EnableEffect("TonemappingEffect", true);
   
   // HDR sahneleri için optimizasyon
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(1.1f);
       bloom->SetIntensity(0.6f);
   }
   if (auto* tonemapping = postProcessingSubsystem->GetEffect<TonemappingEffect>()) {
       tonemapping->SetExposure(1.3f);
       tonemapping->SetTonemapper(0); // ACES
   }
   
   // 6. Lens dirt efekti ile Bloom
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetUseDirt(true);
       bloom->SetDirtIntensity(1.5f);
   }
   
   // 7. Bloom efektini dinamik olarak değiştirme (örneğin gün/gece döngüsü için)
   // Gündüz ayarları
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(1.2f);
       bloom->SetIntensity(0.4f);
   }
   
   // Gece ayarları
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetThreshold(0.7f);
       bloom->SetIntensity(0.9f);
   }
   
   // 8. Performans testi için
   // Düşük kalite ve minimum yoğunluk
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetQuality(0);
       bloom->SetIntensity(0.2f);
   }
   
   // Yüksek kalite ve maksimum yoğunluk
   if (auto* bloom = postProcessingSubsystem->GetEffect<BloomEffect>()) {
       bloom->SetQuality(2);
       bloom->SetIntensity(1.5f);
   }
*/

} // namespace AstralEngine
