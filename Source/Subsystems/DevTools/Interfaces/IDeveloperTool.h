#pragma once
#include <string>

namespace AstralEngine {

/**
 * @brief Tüm geliştirici araçları için temel arayüz.
 * 
 * Bu arayüz, DevToolsSubsystem tarafından yönetilen tüm geliştirici
 * araçlarının uygulaması gereken temel fonksiyonları tanımlar.
 * Her geliştirici aracı bu arayüzden türemelidir.
 */
class IDeveloperTool {
public:
    virtual ~IDeveloperTool() = default;
    
    // Tool yaşam döngüsü
    virtual void OnInitialize() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnRender() = 0;
    virtual void OnShutdown() = 0;
    
    // Tool özellikleri
    virtual const std::string& GetName() const = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
    
    // Tool ayarları
    virtual void LoadSettings(const std::string& settings) = 0;
    virtual std::string SaveSettings() const = 0;
};

} // namespace AstralEngine