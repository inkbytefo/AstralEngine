#pragma once

#include "../../Core/ISubsystem.h"
#include "AssetManager.h"
#include <memory>

namespace AstralEngine {

/**
 * @brief Oyun varlıklarının (asset) yönetiminden sorumlu alt sistem.
 * 
 * Model, texture, shader, audio dosyalarının yüklenmesi,
 * bellekte tutulması ve diğer sistemlere sunulması işlemlerini yönetir.
 */
class AssetSubsystem : public ISubsystem {
public:
    AssetSubsystem();
    ~AssetSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "AssetSubsystem"; }

    // Asset Manager erişimi
    AssetManager* GetAssetManager() const { return m_assetManager.get(); }

    // Asset dizinini ayarla
    void SetAssetDirectory(const std::string& directory);
    const std::string& GetAssetDirectory() const;

private:
    std::unique_ptr<AssetManager> m_assetManager;
    Engine* m_owner = nullptr;
    std::string m_assetDirectory = "Assets";
};

} // namespace AstralEngine
