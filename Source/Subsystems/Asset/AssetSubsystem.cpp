#include "AssetSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../ECS/ECSSubsystem.h"
#include "../Renderer/RenderSubsystem.h"
#include "../../ECS/Components.h"
#include <filesystem>

namespace AstralEngine {

AssetSubsystem::AssetSubsystem() {
    Logger::Debug("AssetSubsystem", "AssetSubsystem created");
}

AssetSubsystem::~AssetSubsystem() {
    Logger::Debug("AssetSubsystem", "AssetSubsystem destroyed");
}

void AssetSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("AssetSubsystem", "Initializing asset subsystem...");
    
    // Asset dizinini çalışma dizinine göre ayarla
    // Program build/bin/Debug dizininden çalıştığı için Assets kullanmalıyız
    m_assetDirectory = (owner->GetBasePath() / "Assets").string();
    
    // Asset Manager oluştur
    m_assetManager = std::make_unique<AssetManager>();
    if (!m_assetManager->Initialize(m_assetDirectory)) {
        Logger::Error("AssetSubsystem", "Failed to initialize AssetManager!");
        return;
    }
    
    Logger::Info("AssetSubsystem", "Asset subsystem initialized successfully. Asset directory: '{}'", m_assetDirectory);
}

void AssetSubsystem::OnUpdate(float deltaTime) {
    (void)deltaTime; // Suppress unused parameter warning
    
    // Asset Manager'ın background yükleme işlemlerini kontrol et
    if (m_assetManager) {
        m_assetManager->Update();
    }
    
    // ECS'den RenderComponent'i olan ama isReady = false olan entity'leri bul ve asset'lerini yükle
    // NOT: Artık asset'ler açıkça kaydediliyor, bu yüzden otomatik kayıt kodları kaldırıldı
    if (m_owner) {
        auto ecsSubsystem = m_owner->GetSubsystem<ECSSubsystem>();
        if (ecsSubsystem && m_assetManager) {
            // RenderComponent'i olan tüm entity'leri sorgula
            auto renderableEntities = ecsSubsystem->QueryEntities<TransformComponent, RenderComponent>();
            
            for (uint32_t entity : renderableEntities) {
                auto& renderComponent = ecsSubsystem->GetComponent<RenderComponent>(entity);
                
                // Eğer asset'ler hazır değilse yükle
                if (!renderComponent.isReady && renderComponent.HasValidHandles()) {
                    bool allAssetsLoaded = true;
                    
                    // Model'i yükle
                    AssetHandle modelHandle = renderComponent.GetModelHandle();
                    if (modelHandle.IsValid()) {
                        AssetLoadState modelState = m_assetManager->GetAssetState(modelHandle);
                        if (modelState == AssetLoadState::NotLoaded) {
                            Logger::Debug("AssetSubsystem", "Loading model for entity {}: {}", entity, modelHandle.GetID());
                            // VulkanDevice'i al
                            auto renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
                            VulkanDevice* device = renderSubsystem ? renderSubsystem->GetGraphicsDevice()->GetVulkanDevice() : nullptr;
                            m_assetManager->LoadAsset(modelHandle, device);
                        } else if (modelState == AssetLoadState::Loaded) {
                            renderComponent.model = m_assetManager->GetAsset<Model>(modelHandle);
                        } else if (modelState == AssetLoadState::Failed) {
                            Logger::Error("AssetSubsystem", "Failed to load model for entity {}: {}", entity, modelHandle.GetID());
                            allAssetsLoaded = false;
                        } else {
                            allAssetsLoaded = false; // Still loading
                        }
                    }
                    
                    // Texture'ı yükle
                    AssetHandle textureHandle = renderComponent.GetTextureHandle();
                    if (textureHandle.IsValid()) {
                        AssetLoadState textureState = m_assetManager->GetAssetState(textureHandle);
                        if (textureState == AssetLoadState::NotLoaded) {
                            Logger::Debug("AssetSubsystem", "Loading texture for entity {}: {}", entity, textureHandle.GetID());
                            // VulkanDevice'i al
                            auto renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
                            VulkanDevice* device = renderSubsystem ? renderSubsystem->GetGraphicsDevice()->GetVulkanDevice() : nullptr;
                            m_assetManager->LoadAsset(textureHandle, device);
                        } else if (textureState == AssetLoadState::Loaded) {
                            renderComponent.texture = m_assetManager->GetAsset<VulkanTexture>(textureHandle);
                        } else if (textureState == AssetLoadState::Failed) {
                            Logger::Error("AssetSubsystem", "Failed to load texture for entity {}: {}", entity, textureHandle.GetID());
                            allAssetsLoaded = false;
                        } else {
                            allAssetsLoaded = false; // Still loading
                        }
                    }
                    
                    // Tüm asset'ler yüklendiyse isReady flag'ini set et
                    if (allAssetsLoaded) {
                        renderComponent.isReady = true;
                        Logger::Info("AssetSubsystem", "All assets loaded for entity {}, marked as ready", entity);
                    }
                }
            }
        }
    }
}

void AssetSubsystem::OnShutdown() {
    Logger::Info("AssetSubsystem", "Shutting down asset subsystem...");
    
    if (m_assetManager) {
        m_assetManager->Shutdown();
        m_assetManager.reset();
    }
    
    Logger::Info("AssetSubsystem", "Asset subsystem shutdown complete");
}

void AssetSubsystem::SetAssetDirectory(const std::string& directory) {
    m_assetDirectory = directory;
    
    if (m_assetManager) {
        Logger::Warning("AssetSubsystem", "Asset directory changed after initialization. Restart may be required.");
    }
    
    Logger::Info("AssetSubsystem", "Asset directory set to: '{}'", directory);
}

const std::string& AssetSubsystem::GetAssetDirectory() const {
    return m_assetDirectory;
}

} // namespace AstralEngine
