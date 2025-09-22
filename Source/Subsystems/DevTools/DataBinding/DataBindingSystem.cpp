#include "DataBindingSystem.h"
#include "../Events/DevToolsEvent.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

DataBindingSystem& DataBindingSystem::GetInstance() {
    static DataBindingSystem instance;
    return instance;
}

void DataBindingSystem::RegisterDataSource(const std::string& name, std::function<std::any()> getter, 
                                         std::function<void(const std::any&)> setter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!getter) {
        Logger::Error("DataBinding", "Getter fonksiyonu boş olamaz: {}", name);
        return;
    }
    
    m_dataSources[name] = DataSource(getter, setter);
    Logger::Info("DataBinding", "Veri kaynağı kaydedildi: {}", name);
}

void DataBindingSystem::UnregisterDataSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dataSources.find(name);
    if (it != m_dataSources.end()) {
        // İlgili tüm binding'leri kaldır
        UnbindAll(name);
        m_dataSources.erase(it);
        Logger::Info("DataBinding", "Veri kaynağı kaldırıldı: {}", name);
    }
}

void DataBindingSystem::Bind(const std::string& sourceName, const std::string& targetName, 
                           DataBindingType type, std::function<std::any(const std::any&)> converter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Source ve target'ın var olduğunu kontrol et
    if (m_dataSources.find(sourceName) == m_dataSources.end()) {
        Logger::Error("DataBinding", "Source veri kaynağı bulunamadı: {}", sourceName);
        return;
    }
    
    if (m_dataSources.find(targetName) == m_dataSources.end()) {
        Logger::Error("DataBinding", "Target veri kaynağı bulunamadı: {}", targetName);
        return;
    }
    
    // Aynı binding zaten var mı kontrol et
    for (const auto& binding : m_bindings) {
        if (binding->sourceName == sourceName && binding->targetName == targetName) {
            Logger::Warning("DataBinding", "Binding zaten mevcut: {} -> {}", sourceName, targetName);
            return;
        }
    }
    
    // Yeni binding oluştur
    auto binding = std::make_shared<DataBinding>(sourceName, targetName, type, converter);
    m_bindings.push_back(binding);
    
    // Source ve target binding listelerine ekle
    m_sourceBindings[sourceName].push_back(binding);
    m_targetBindings[targetName].push_back(binding);
    
    Logger::Info("DataBinding", "Binding oluşturuldu: {} -> {} (type: {})", 
                sourceName, targetName, static_cast<int>(type));
    
    // İlk güncellemeyi yap
    UpdateBinding(binding);
}

void DataBindingSystem::Unbind(const std::string& sourceName, const std::string& targetName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Binding'i bul ve kaldır
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
            [&sourceName, &targetName](const std::shared_ptr<DataBinding>& binding) {
                return binding->sourceName == sourceName && binding->targetName == targetName;
            }),
        m_bindings.end()
    );
    
    // Source binding listesini temizle
    auto sourceIt = m_sourceBindings.find(sourceName);
    if (sourceIt != m_sourceBindings.end()) {
        sourceIt->second.erase(
            std::remove_if(sourceIt->second.begin(), sourceIt->second.end(),
                [&targetName](const std::weak_ptr<DataBinding>& weakBinding) {
                    auto binding = weakBinding.lock();
                    return !binding || binding->targetName == targetName;
                }),
            sourceIt->second.end()
        );
    }
    
    // Target binding listesini temizle
    auto targetIt = m_targetBindings.find(targetName);
    if (targetIt != m_targetBindings.end()) {
        targetIt->second.erase(
            std::remove_if(targetIt->second.begin(), targetIt->second.end(),
                [&sourceName](const std::weak_ptr<DataBinding>& weakBinding) {
                    auto binding = weakBinding.lock();
                    return !binding || binding->sourceName == sourceName;
                }),
            targetIt->second.end()
        );
    }
    
    Logger::Info("DataBinding", "Binding kaldırıldı: {} -> {}", sourceName, targetName);
}

void DataBindingSystem::UnbindAll(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Bu isimle ilgili tüm binding'leri kaldır
    std::vector<std::string> targetsToRemove;
    
    // Source olarak kullanılan binding'leri kaldır
    auto sourceIt = m_sourceBindings.find(name);
    if (sourceIt != m_sourceBindings.end()) {
        for (const auto& weakBinding : sourceIt->second) {
            auto binding = weakBinding.lock();
            if (binding) {
                targetsToRemove.push_back(binding->targetName);
            }
        }
        m_sourceBindings.erase(sourceIt);
    }
    
    // Target olarak kullanılan binding'leri kaldır
    auto targetIt = m_targetBindings.find(name);
    if (targetIt != m_targetBindings.end()) {
        for (const auto& weakBinding : targetIt->second) {
            auto binding = weakBinding.lock();
            if (binding) {
                Unbind(binding->sourceName, binding->targetName);
            }
        }
        m_targetBindings.erase(targetIt);
    }
    
    // Tüm binding listesini temizle
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
            [&name](const std::shared_ptr<DataBinding>& binding) {
                return binding->sourceName == name || binding->targetName == name;
            }),
        m_bindings.end()
    );
    
    Logger::Info("DataBinding", "{} ile ilgili tüm binding'ler kaldırıldı", name);
}

void DataBindingSystem::UpdateSource(const std::string& name, const std::any& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dataSources.find(name);
    if (it != m_dataSources.end()) {
        auto& dataSource = it->second;
        
        // Değer değişti mi kontrol et
        bool valueChanged = true;
        try {
            if (dataSource.currentValue.type() == value.type()) {
                // Basit karşılaştırma (tüm tipler için çalışmayabilir)
                if (dataSource.currentValue.type() == typeid(int)) {
                    valueChanged = std::any_cast<int>(dataSource.currentValue) != std::any_cast<int>(value);
                } else if (dataSource.currentValue.type() == typeid(float)) {
                    valueChanged = std::any_cast<float>(dataSource.currentValue) != std::any_cast<float>(value);
                } else if (dataSource.currentValue.type() == typeid(std::string)) {
                    valueChanged = std::any_cast<std::string>(dataSource.currentValue) != std::any_cast<std::string>(value);
                }
                // Diğer tipler için her zaman değişmiş say
            }
        }
        catch (const std::exception& e) {
            Logger::Warning("DataBinding", "Değer karşılaştırma hatası: {}", e.what());
        }
        
        if (valueChanged) {
            dataSource.currentValue = value;
            
            // Event yayınla
            auto& eventSystem = DevToolsEventSystem::GetInstance();
            eventSystem.PublishDataUpdated(name, value);
            
            // İlgili binding'leri güncelle
            NotifyValueChanged(name);
            
            Logger::Debug("DataBinding", "Veri kaynağı güncellendi: {}", name);
        }
    }
}

std::any DataBindingSystem::GetSourceValue(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dataSources.find(name);
    if (it != m_dataSources.end()) {
        const auto& dataSource = it->second;
        if (dataSource.getter) {
            return dataSource.getter();
        }
        return dataSource.currentValue;
    }
    
    Logger::Warning("DataBinding", "Veri kaynağı bulunamadı: {}", name);
    return std::any();
}

void DataBindingSystem::SetBindingActive(const std::string& sourceName, const std::string& targetName, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& binding : m_bindings) {
        if (binding->sourceName == sourceName && binding->targetName == targetName) {
            binding->isActive = active;
            Logger::Info("DataBinding", "Binding durumu güncellendi: {} -> {} (active: {})", 
                        sourceName, targetName, active);
            return;
        }
    }
    
    Logger::Warning("DataBinding", "Binding bulunamadı: {} -> {}", sourceName, targetName);
}

bool DataBindingSystem::IsBindingActive(const std::string& sourceName, const std::string& targetName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& binding : m_bindings) {
        if (binding->sourceName == sourceName && binding->targetName == targetName) {
            return binding->isActive;
        }
    }
    
    return false;
}

void DataBindingSystem::UpdateAllBindings() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    CleanupExpiredBindings();
    
    for (const auto& binding : m_bindings) {
        if (binding->isActive) {
            UpdateBinding(binding);
        }
    }
}

bool DataBindingSystem::HasDataSource(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dataSources.find(name) != m_dataSources.end();
}

std::vector<std::string> DataBindingSystem::GetDataSourceNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_dataSources.size());
    
    for (const auto& pair : m_dataSources) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::vector<std::shared_ptr<DataBinding>> DataBindingSystem::GetBindingsForSource(const std::string& sourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<DataBinding>> result;
    
    auto it = m_sourceBindings.find(sourceName);
    if (it != m_sourceBindings.end()) {
        for (const auto& weakBinding : it->second) {
            auto binding = weakBinding.lock();
            if (binding) {
                result.push_back(binding);
            }
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<DataBinding>> DataBindingSystem::GetBindingsForTarget(const std::string& targetName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<DataBinding>> result;
    
    auto it = m_targetBindings.find(targetName);
    if (it != m_targetBindings.end()) {
        for (const auto& weakBinding : it->second) {
            auto binding = weakBinding.lock();
            if (binding) {
                result.push_back(binding);
            }
        }
    }
    
    return result;
}

void DataBindingSystem::UpdateBinding(const std::shared_ptr<DataBinding>& binding) {
    if (!binding || !binding->isActive) {
        return;
    }
    
    try {
        auto sourceIt = m_dataSources.find(binding->sourceName);
        auto targetIt = m_dataSources.find(binding->targetName);
        
        if (sourceIt != m_dataSources.end() && targetIt != m_dataSources.end()) {
            auto& source = sourceIt->second;
            auto& target = targetIt->second;
            
            // Source değerini al
            std::any sourceValue = source.getter ? source.getter() : source.currentValue;
            
            // Converter varsa uygula
            std::any finalValue = binding->converter ? binding->converter(sourceValue) : sourceValue;
            
            // Target'a set et
            if (target.setter) {
                target.setter(finalValue);
                target.currentValue = finalValue;
            }
        }
    }
    catch (const std::exception& e) {
        Logger::Error("DataBinding", "Binding güncellenirken hata: {} -> {} : {}", 
                     binding->sourceName, binding->targetName, e.what());
    }
}

void DataBindingSystem::NotifyValueChanged(const std::string& name) {
    // Bu veri kaynağını kullanan tüm binding'leri güncelle
    auto it = m_sourceBindings.find(name);
    if (it != m_sourceBindings.end()) {
        for (const auto& weakBinding : it->second) {
            auto binding = weakBinding.lock();
            if (binding && binding->isActive) {
                UpdateBinding(binding);
            }
        }
    }
}

void DataBindingSystem::CleanupExpiredBindings() {
    // Süresi dolan weak_ptr'ları temizle
    for (auto& pair : m_sourceBindings) {
        pair.second.erase(
            std::remove_if(pair.second.begin(), pair.second.end(),
                [](const std::weak_ptr<DataBinding>& weakBinding) {
                    return weakBinding.expired();
                }),
            pair.second.end()
        );
    }
    
    for (auto& pair : m_targetBindings) {
        pair.second.erase(
            std::remove_if(pair.second.begin(), pair.second.end(),
                [](const std::weak_ptr<DataBinding>& weakBinding) {
                    return weakBinding.expired();
                }),
            pair.second.end()
        );
    }
}

} // namespace AstralEngine