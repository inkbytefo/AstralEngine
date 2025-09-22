#include "DevToolsSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "Profiler/ProfilerWindow.h"
#include "DebugRenderer/DebugRenderer.h"
#include "MaterialEditor/MaterialEditorWindow.h"

namespace AstralEngine {

DevToolsSubsystem::DevToolsSubsystem() {
    Logger::Info("DevTools", "DevToolsSubsystem oluşturuluyor");
}

void DevToolsSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("DevTools", "DevToolsSubsystem başlatılıyor");
    
    // Varsayılan geliştirici araçlarını kaydet
    auto profilerWindow = std::make_unique<ProfilerWindow>();
    RegisterTool(std::move(profilerWindow));
    
    // DebugRenderer'ı kaydet
    auto debugRenderer = std::make_unique<DebugRenderer>();
    RegisterTool(std::move(debugRenderer));
    
    // MaterialEditor'ı kaydet
    auto materialEditor = std::make_unique<MaterialEditorWindow>();
    RegisterTool(std::move(materialEditor));
    
    // Kayıtlı tüm araçları başlat
    for (auto& tool : m_tools) {
        try {
            tool->OnInitialize();
            Logger::Info("DevTools", "{} aracı başlatıldı", tool->GetName());
        }
        catch (const std::exception& e) {
            Logger::Error("DevTools", "{} aracı başlatılırken hata: {}", tool->GetName(), e.what());
        }
    }
    
    Logger::Info("DevTools", "DevToolsSubsystem başarıyla başlatıldı");
}

void DevToolsSubsystem::OnUpdate(float deltaTime) {
    // Sadece etkin olan araçları güncelle
    for (auto& tool : m_tools) {
        if (tool->IsEnabled()) {
            try {
                tool->OnUpdate(deltaTime);
            }
            catch (const std::exception& e) {
                Logger::Error("DevTools", "{} aracı güncellenirken hata: {}", tool->GetName(), e.what());
            }
        }
    }
}

void DevToolsSubsystem::OnShutdown() {
    Logger::Info("DevTools", "DevToolsSubsystem kapatılıyor");
    
    // Tüm araçları kapat (ters sırada)
    for (auto it = m_tools.rbegin(); it != m_tools.rend(); ++it) {
        try {
            (*it)->OnShutdown();
            Logger::Info("DevTools", "{} aracı kapatıldı", (*it)->GetName());
        }
        catch (const std::exception& e) {
            Logger::Error("DevTools", "{} aracı kapatılırken hata: {}", (*it)->GetName(), e.what());
        }
    }
    
    m_tools.clear();
    m_toolMap.clear();
    m_dataProviders.clear();
    m_dataConsumers.clear();
    m_bindings.clear();
    
    Logger::Info("DevTools", "DevToolsSubsystem başarıyla kapatıldı");
}

void DevToolsSubsystem::RegisterTool(std::unique_ptr<IDeveloperTool> tool) {
    if (!tool) {
        Logger::Warning("DevTools", "Boş tool kaydedilmeye çalışıldı");
        return;
    }
    
    const std::string& toolName = tool->GetName();
    
    // Aynı isimde tool zaten var mı kontrol et
    if (m_toolMap.find(toolName) != m_toolMap.end()) {
        Logger::Warning("DevTools", "{} isminde bir tool zaten kayıtlı", toolName);
        return;
    }
    
    IDeveloperTool* toolPtr = tool.get();
    m_tools.push_back(std::move(tool));
    m_toolMap[toolName] = toolPtr;
    
    Logger::Info("DevTools", "{} aracı kaydedildi", toolName);
}

void DevToolsSubsystem::UnregisterTool(const std::string& toolName) {
    auto it = m_toolMap.find(toolName);
    if (it == m_toolMap.end()) {
        Logger::Warning("DevTools", "{} isminde bir tool bulunamadı", toolName);
        return;
    }
    
    // Vector'den bul ve sil
    for (auto toolIt = m_tools.begin(); toolIt != m_tools.end(); ++toolIt) {
        if (toolIt->get() == it->second) {
            try {
                (*toolIt)->OnShutdown();
            }
            catch (const std::exception& e) {
                Logger::Error("DevTools", "{} aracı kaldırılırken hata: {}", toolName, e.what());
            }
            m_tools.erase(toolIt);
            break;
        }
    }
    
    m_toolMap.erase(it);
    Logger::Info("DevTools", "{} aracı kaldırıldı", toolName);
}

void DevToolsSubsystem::SetToolEnabled(const std::string& toolName, bool enabled) {
    auto it = m_toolMap.find(toolName);
    if (it == m_toolMap.end()) {
        Logger::Warning("DevTools", "{} isminde bir tool bulunamadı", toolName);
        return;
    }
    
    it->second->SetEnabled(enabled);
    Logger::Info("DevTools", "{} aracı {}", toolName, enabled ? "etkinleştirildi" : "devre dışı bırakıldı");
}

IDeveloperTool* DevToolsSubsystem::GetTool(const std::string& toolName) {
    auto it = m_toolMap.find(toolName);
    if (it != m_toolMap.end()) {
        return it->second;
    }
    Logger::Warning("DevTools", "{} isminde bir tool bulunamadı", toolName);
    return nullptr;
}

void DevToolsSubsystem::RegisterDataProvider(std::unique_ptr<IDataProvider> provider) {
    if (!provider) {
        Logger::Warning("DevTools", "Boş data provider kaydedilmeye çalışıldı");
        return;
    }
    
    const std::string& providerName = provider->GetProviderName();
    
    if (m_dataProviders.find(providerName) != m_dataProviders.end()) {
        Logger::Warning("DevTools", "{} isminde bir data provider zaten kayıtlı", providerName);
        return;
    }
    
    m_dataProviders[providerName] = std::move(provider);
    Logger::Info("DevTools", "{} data provider'ı kaydedildi", providerName);
}

void DevToolsSubsystem::UnregisterDataProvider(const std::string& providerName) {
    auto it = m_dataProviders.find(providerName);
    if (it == m_dataProviders.end()) {
        Logger::Warning("DevTools", "{} isminde bir data provider bulunamadı", providerName);
        return;
    }
    
    // Bağlantıları temizle
    m_bindings.erase(providerName);
    
    m_dataProviders.erase(it);
    Logger::Info("DevTools", "{} data provider'ı kaldırıldı", providerName);
}

void DevToolsSubsystem::RegisterDataConsumer(std::unique_ptr<IDataConsumer> consumer) {
    if (!consumer) {
        Logger::Warning("DevTools", "Boş data consumer kaydedilmeye çalışıldı");
        return;
    }
    
    const std::string& consumerName = consumer->GetConsumerName();
    
    if (m_dataConsumers.find(consumerName) != m_dataConsumers.end()) {
        Logger::Warning("DevTools", "{} isminde bir data consumer zaten kayıtlı", consumerName);
        return;
    }
    
    m_dataConsumers[consumerName] = std::move(consumer);
    Logger::Info("DevTools", "{} data consumer'ı kaydedildi", consumerName);
}

void DevToolsSubsystem::UnregisterDataConsumer(const std::string& consumerName) {
    auto it = m_dataConsumers.find(consumerName);
    if (it == m_dataConsumers.end()) {
        Logger::Warning("DevTools", "{} isminde bir data consumer bulunamadı", consumerName);
        return;
    }
    
    // Bağlantıları temizle
    for (auto& binding : m_bindings) {
        auto& consumers = binding.second;
        consumers.erase(std::remove(consumers.begin(), consumers.end(), consumerName), consumers.end());
    }
    
    m_dataConsumers.erase(it);
    Logger::Info("DevTools", "{} data consumer'ı kaldırıldı", consumerName);
}

void DevToolsSubsystem::BindProviderToConsumer(const std::string& providerName, const std::string& consumerName) {
    auto providerIt = m_dataProviders.find(providerName);
    if (providerIt == m_dataProviders.end()) {
        Logger::Warning("DevTools", "{} isminde bir data provider bulunamadı", providerName);
        return;
    }
    
    auto consumerIt = m_dataConsumers.find(consumerName);
    if (consumerIt == m_dataConsumers.end()) {
        Logger::Warning("DevTools", "{} isminde bir data consumer bulunamadı", consumerName);
        return;
    }
    
    // Bağlantıyı ekle
    m_bindings[providerName].push_back(consumerName);
    
    // Veriyi hemen gönder
    try {
        void* data = providerIt->second->GetData();
        size_t size = providerIt->second->GetDataSize();
        consumerIt->second->SetData(data, size);
        
        Logger::Info("DevTools", "{} provider'ı {} consumer'ına bağlandı ve veri gönderildi", providerName, consumerName);
    }
    catch (const std::exception& e) {
        Logger::Error("DevTools", "{} provider'ı {} consumer'ına bağlanırken hata: {}", providerName, consumerName, e.what());
    }
}

} // namespace AstralEngine