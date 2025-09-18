#include "RenderSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Platform/PlatformSubsystem.h"
#include "../ECS/ECSSubsystem.h"
#include "../Asset/AssetSubsystem.h"
#include "../Material/Material.h"
#include "VulkanMeshManager.h"
#include "VulkanTextureManager.h"
#include <map>

namespace AstralEngine {

RenderSubsystem::RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem created");
}

RenderSubsystem::~RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem destroyed");
}

void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("RenderSubsystem", "Initializing render subsystem...");
    
    // Platform subsystem'den window'u al
    auto platformSubsystem = m_owner->GetSubsystem<PlatformSubsystem>();
    if (!platformSubsystem) {
        Logger::Error("RenderSubsystem", "PlatformSubsystem not found!");
        return;
    }
    
    m_window = platformSubsystem->GetWindow();
    if (!m_window) {
        Logger::Error("RenderSubsystem", "Window not found!");
        return;
    }
    
    // ECS subsystem'e eriş
    m_ecsSubsystem = m_owner->GetSubsystem<ECSSubsystem>();
    if (!m_ecsSubsystem) {
        Logger::Error("RenderSubsystem", "ECSSubsystem not found!");
        return;
    }
    
    // Graphics device oluştur
    m_graphicsDevice = std::make_unique<GraphicsDevice>();
    if (!m_graphicsDevice->Initialize(m_window, m_owner)) {
        Logger::Error("RenderSubsystem", "Failed to initialize GraphicsDevice!");
        m_graphicsDevice.reset();
        return;
    }
    
    // AssetSubsystem'i al
    m_assetSubsystem = m_owner->GetSubsystem<AssetSubsystem>();
    if (!m_assetSubsystem) {
        Logger::Error("RenderSubsystem", "AssetSubsystem not found!");
        return;
    }
    
    // Vulkan Mesh Manager oluştur
    m_vulkanMeshManager = std::make_unique<VulkanMeshManager>();
    if (!m_vulkanMeshManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem)) {
        Logger::Error("RenderSubsystem", "Failed to initialize VulkanMeshManager!");
        return;
    }
    
    // Vulkan Texture Manager oluştur
    m_vulkanTextureManager = std::make_unique<VulkanTextureManager>();
    if (!m_vulkanTextureManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem)) {
        Logger::Error("RenderSubsystem", "Failed to initialize VulkanTextureManager!");
        return;
    }
    
    // Material Manager oluştur (AssetSubsystem kullanarak)
    m_materialManager = std::make_unique<MaterialManager>();
    if (!m_materialManager->Initialize(m_graphicsDevice->GetVulkanDevice(), m_assetSubsystem->GetAssetManager())) {
        Logger::Error("RenderSubsystem", "Failed to initialize MaterialManager!");
        return;
    }
    
    // Note: ShaderManager is now handled by MaterialManager in the new architecture
    // Shaders are loaded and managed through materials
    
    // Kamera oluştur ve yapılandır
    m_camera = std::make_unique<Camera>();
    Camera::Config cameraConfig;
    cameraConfig.position = glm::vec3(0.0f, 0.0f, 2.0f);
    cameraConfig.target = glm::vec3(0.0f, 0.0f, 0.0f);
    cameraConfig.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cameraConfig.fov = 45.0f;
    // Dinamik olarak pencere boyutundan aspect ratio hesapla
    cameraConfig.aspectRatio = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
    cameraConfig.nearPlane = 0.1f;
    cameraConfig.farPlane = 100.0f;
    
    m_camera->Initialize(cameraConfig);
    
    Logger::Info("RenderSubsystem", "Render subsystem initialized successfully");
}

void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice || !m_graphicsDevice->IsInitialized()) {
        Logger::Error("RenderSubsystem", "Cannot update - GraphicsDevice not initialized");
        return;
    }

    // Frame sayaçlarını güncelle
    m_framesProcessed++;

    // Kamera aspect ratio değerini dinamik olarak güncelle
    if (m_camera && m_window) {
        float currentAspectRatio = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
        if (currentAspectRatio != m_camera->GetAspectRatio()) {
            m_camera->SetAspectRatio(currentAspectRatio);
        }
    }

    // Asenkron yükleme aktifse upload tamamlamalarını kontrol et
    if (m_enableAsyncLoading) {
        if (m_vulkanMeshManager) {
            m_vulkanMeshManager->CheckUploadCompletions();
        }
        if (m_vulkanTextureManager) {
            m_vulkanTextureManager->CheckUploadCompletions();
        }
    }

    // Material Manager'ı güncelle
    if (m_materialManager) {
        m_materialManager->Update();
    }
    
    // Note: Shader updates are now handled by MaterialManager

    // Debug loglama için asset durumlarını logla (her 60 frame'de bir)
    if (m_framesProcessed % 60 == 0) {
        LogAssetStatus();
    }

    // ECS'den render verilerini al
    if (m_ecsSubsystem) {
        auto renderPacket = m_ecsSubsystem->GetRenderData();
        
        // RenderQueue oluştur ve doldur - GPU kaynakları çözülmüş olarak
        using RenderQueue = std::map<VkPipeline, std::vector<VulkanRenderer::ResolvedRenderItem>>;
        RenderQueue sortedRenderQueue;

        // Debug sayaçlarını sıfırla
        m_meshesReady = 0;
        m_texturesReady = 0;
        m_meshesPending = 0;
        m_texturesPending = 0;

        for (const auto& item : renderPacket.renderItems) {
            if (!item.visible) {
                continue; // Görünür olmayan öğeleri atla
            }

            AssetHandle materialHandle = item.materialHandle;
            AssetHandle modelHandle = item.modelHandle;
            
            if (!materialHandle.IsValid() || !modelHandle.IsValid()) {
                Logger::Warning("RenderSubsystem", "RenderItem has invalid handles (material: {}, model: {}). Skipping.",
                               materialHandle.IsValid() ? materialHandle.GetID() : 0,
                               modelHandle.IsValid() ? modelHandle.GetID() : 0);
                continue;
            }

            // Asset readiness kontrolü yap
            if (!CheckAssetReadiness(modelHandle, materialHandle)) {
                continue; // Asset'ler hazır değilse atla
            }

            // Material objesini MaterialManager'dan al
            auto material = m_materialManager->GetMaterial(materialHandle);
            if (!material) {
                Logger::Warning("RenderSubsystem", "Material not found for handle: {}. Skipping item.", materialHandle.GetID());
                continue;
            }

            // Material'dan VkPipeline'ı al
            VkPipeline pipeline = material->GetPipeline();
            if (pipeline == VK_NULL_HANDLE) {
                Logger::Debug("RenderSubsystem", "Material '{}' is not yet ready for rendering (shaders still loading). Skipping item.", material->GetName());
                continue;
            }

            // GPU mesh kaynağını al veya oluştur
            auto mesh = m_vulkanMeshManager->GetOrCreateMesh(modelHandle);
            if (!mesh) {
                // Bu, mesh verisinin hala yükleniyor olduğu veya bir hata oluştuğu anlamına gelebilir.
                // Her iki durumda da bu karede çizimini atlamak doğru bir yaklaşımdır.
                Logger::Debug("RenderSubsystem", "Mesh data for model handle {} is not yet ready. Skipping render for this item.", modelHandle.GetID());
                m_meshesPending++;
                continue;
            }

            // Mesh'in hazır olduğunu kontrol et (asenkron yükleme için)
            if (m_enableAsyncLoading && !mesh->IsReady()) {
                Logger::Debug("RenderSubsystem", "Mesh for model handle {} is still uploading. Skipping render for this item.", modelHandle.GetID());
                m_meshesPending++;
                continue;
            }

            // Material'in texture'larının hazır olduğunu kontrol et
            if (m_enableAsyncLoading && !material->AreTexturesReady()) {
                Logger::Debug("RenderSubsystem", "Textures for material '{}' are still uploading. Skipping render for this item.", material->GetName());
                m_texturesPending++;
                continue;
            }

            // Debug sayaçlarını güncelle
            m_meshesReady++;
            m_texturesReady++;

            // Çözülmüş render item'ı kuyruğa ekle
            // Material zaten texture'ları yönettiği için ek texture çözümlemesi gerekmiyor
            sortedRenderQueue[pipeline].push_back({
                item.transform,
                mesh,
                material
            });
        }
        
        Logger::Debug("RenderSubsystem", "Sorted {} render items into {} pipelines. Ready meshes: {}, Pending meshes: {}, Ready textures: {}, Pending textures: {}",
                     renderPacket.renderItems.size(), sortedRenderQueue.size(), m_meshesReady, m_meshesPending, m_texturesReady, m_texturesPending);

        // GraphicsDevice frame'i başlatsın (fence bekleme ve image alma)
        if (m_graphicsDevice->BeginFrame()) {
            // VulkanRenderer sadece komutları kaydetsin
            auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
            if (vulkanRenderer) {
                // Kamera referansını güncelle
                vulkanRenderer->SetCamera(m_camera.get());
                // GraphicsDevice'den güncel frame indeksini al
                uint32_t currentFrameIndex = m_graphicsDevice->GetCurrentFrameIndex();
                // Sıralanmış render verilerini VulkanRenderer'a gönder
                vulkanRenderer->RecordCommands(currentFrameIndex, sortedRenderQueue); // Sadece komut kaydı, submit/present yapma
            }
            
            // GraphicsDevice frame'i bitirsin (submit ve present)
            m_graphicsDevice->EndFrame();
        }
        
    } else {
        Logger::Error("RenderSubsystem", "ECSSubsystem not available for render data");
    }
}

void RenderSubsystem::OnShutdown() {
    Logger::Info("RenderSubsystem", "Shutting down render subsystem...");
    
    // Note: ShaderManager is now handled by MaterialManager
    
    if (m_materialManager) {
        m_materialManager->Shutdown();
        m_materialManager.reset();
    }
    
    if (m_vulkanTextureManager) {
        m_vulkanTextureManager->Shutdown();
        m_vulkanTextureManager.reset();
    }
    
    if (m_vulkanMeshManager) {
        m_vulkanMeshManager->Shutdown();
        m_vulkanMeshManager.reset();
    }
    
    if (m_camera) {
        m_camera->Shutdown();
        m_camera.reset();
    }
    
    if (m_graphicsDevice) {
        m_graphicsDevice->Shutdown();
        m_graphicsDevice.reset();
    }
    
    m_window = nullptr;
    Logger::Info("RenderSubsystem", "Render subsystem shutdown complete");
}

void RenderSubsystem::BeginFrame() {
    if (!m_graphicsDevice) {
        Logger::Error("RenderSubsystem", "Cannot begin frame - GraphicsDevice not initialized");
        return;
    }
    
    Logger::Debug("RenderSubsystem", "Beginning frame");
    
    // Frame başlangıç işlemleri
    // - Clear color ayarla
    // - Viewport ayarla
    // - Command buffer begin
    // - Render pass begin
    
    // Şimdilik sadece loglama yapıyoruz, gerçek implementasyon VulkanRenderer'da
}

void RenderSubsystem::EndFrame() {
    if (!m_graphicsDevice) {
        Logger::Error("RenderSubsystem", "Cannot end frame - GraphicsDevice not initialized");
        return;
    }
    
    Logger::Debug("RenderSubsystem", "Ending frame");
    
    // Frame bitirme işlemleri
    // - Render pass end
    // - Command buffer submit
    // - Present
    
    // Şimdilik sadece loglama yapıyoruz, gerçek implementasyon VulkanRenderer'da
}

bool RenderSubsystem::IsMaterialReady(const std::shared_ptr<Material>& material) const {
    if (!material) {
        return false;
    }
    
    // Check if the material has a valid pipeline
    return material->GetPipeline() != VK_NULL_HANDLE;
}

bool RenderSubsystem::CheckAssetReadiness(const AssetHandle& modelHandle, const AssetHandle& materialHandle) const {
    if (!modelHandle.IsValid() || !materialHandle.IsValid()) {
        return false;
    }

    // Material objesini MaterialManager'dan al
    auto material = m_materialManager->GetMaterial(materialHandle);
    if (!material) {
        Logger::Warning("RenderSubsystem", "Material not found for handle: {}", materialHandle.GetID());
        return false;
    }

    // Material'in pipeline'ının hazır olduğunu kontrol et
    if (material->GetPipeline() == VK_NULL_HANDLE) {
        Logger::Debug("RenderSubsystem", "Material '{}' pipeline is not ready", material->GetName());
        return false;
    }

    // Asenkron yükleme aktifse mesh ve texture durumlarını kontrol et
    if (m_enableAsyncLoading) {
        // GPU mesh kaynağını al veya oluştur
        auto mesh = m_vulkanMeshManager->GetOrCreateMesh(modelHandle);
        if (!mesh) {
            Logger::Debug("RenderSubsystem", "Mesh for model handle {} is not available", modelHandle.GetID());
            return false;
        }

        // Mesh'in hazır olduğunu kontrol et
        if (!mesh->IsReady()) {
            Logger::Debug("RenderSubsystem", "Mesh for model handle {} is not ready (still uploading)", modelHandle.GetID());
            return false;
        }

        // Material'in texture'larının hazır olduğunu kontrol et
        if (!material->AreTexturesReady()) {
            Logger::Debug("RenderSubsystem", "Textures for material '{}' are not ready (still uploading)", material->GetName());
            return false;
        }
    }

    return true;
}

void RenderSubsystem::LogAssetStatus() const {
    if (!m_enableAsyncLoading) {
        return; // Asenkron yükleme kapalıysa loglama yapma
    }

    Logger::Debug("RenderSubsystem", "=== Asset Status Report (Frame {}) ===", m_framesProcessed);
    Logger::Debug("RenderSubsystem", "Ready Meshes: {}, Pending Meshes: {}", m_meshesReady, m_meshesPending);
    Logger::Debug("RenderSubsystem", "Ready Textures: {}, Pending Textures: {}", m_texturesReady, m_texturesPending);
    
    // Mesh manager'dan genel durum bilgisi al
    if (m_vulkanMeshManager) {
        uint32_t totalMeshes = m_vulkanMeshManager->GetTotalMeshCount();
        uint32_t readyMeshes = m_vulkanMeshManager->GetReadyMeshCount();
        Logger::Debug("RenderSubsystem", "Mesh Manager - Total: {}, Ready: {}, Uploading: {}",
                     totalMeshes, readyMeshes, totalMeshes - readyMeshes);
    }
    
    // Texture manager'dan genel durum bilgisi al
    if (m_vulkanTextureManager) {
        uint32_t totalTextures = m_vulkanTextureManager->GetTotalTextureCount();
        uint32_t readyTextures = m_vulkanTextureManager->GetReadyTextureCount();
        Logger::Debug("RenderSubsystem", "Texture Manager - Total: {}, Ready: {}, Uploading: {}",
                     totalTextures, readyTextures, totalTextures - readyTextures);
    }
    
    Logger::Debug("RenderSubsystem", "=========================================");
}

} // namespace AstralEngine
