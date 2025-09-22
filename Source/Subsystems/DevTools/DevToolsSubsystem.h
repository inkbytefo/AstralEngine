#pragma once

#include "../../Core/ISubsystem.h"
#include "Interfaces/IDeveloperTool.h"
#include "Interfaces/IDataProvider.h"
#include "Interfaces/IDataConsumer.h"
#include "Profiler/ProfilerWindow.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace AstralEngine {

class Engine;

/**
 * @brief Geliştirici araçları yönetim sistemi.
 * 
 * Bu subsystem, tüm geliştirici araçlarının kaydedilmesi, yönetilmesi
 * ve araçlar arasında veri paylaşımının sağlanmasından sorumludur.
 * ISubsystem arayüzünü uygulayarak Engine'in yaşam döngüsüne entegre olur.
 */
class DevToolsSubsystem : public ISubsystem {
public:
    DevToolsSubsystem();
    ~DevToolsSubsystem() = default;
    
    // ISubsystem implementasyonu
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "DevToolsSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::Update; }
    
    // Tool yönetimi
    void RegisterTool(std::unique_ptr<IDeveloperTool> tool);
    void UnregisterTool(const std::string& toolName);
    void SetToolEnabled(const std::string& toolName, bool enabled);
    IDeveloperTool* GetTool(const std::string& toolName);
    
    // Veri paylaşımı
    void RegisterDataProvider(std::unique_ptr<IDataProvider> provider);
    void UnregisterDataProvider(const std::string& providerName);
    void RegisterDataConsumer(std::unique_ptr<IDataConsumer> consumer);
    void UnregisterDataConsumer(const std::string& consumerName);
    
    // Veri bağlama
    void BindProviderToConsumer(const std::string& providerName, const std::string& consumerName);

private:
    Engine* m_owner = nullptr;
    std::vector<std::unique_ptr<IDeveloperTool>> m_tools;
    std::unordered_map<std::string, IDeveloperTool*> m_toolMap;
    std::unordered_map<std::string, std::unique_ptr<IDataProvider>> m_dataProviders;
    std::unordered_map<std::string, std::unique_ptr<IDataConsumer>> m_dataConsumers;
    std::unordered_map<std::string, std::vector<std::string>> m_bindings;
};

} // namespace AstralEngine