#include "AssetSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
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

        // Hot reload kontrolü
        m_assetManager->CheckForAssetChanges();
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
