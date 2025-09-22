#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <any>
#include <memory>
#include <mutex>

namespace AstralEngine {

enum class DataBindingType {
    OneWay,         // Source -> Target
    TwoWay,         // Source <-> Target
    OneWayToSource  // Target -> Source
};

struct DataBinding {
    std::string sourceName;
    std::string targetName;
    DataBindingType type;
    std::function<std::any(const std::any&)> converter;
    bool isActive;
    
    DataBinding(const std::string& src, const std::string& tgt, DataBindingType t, 
                std::function<std::any(const std::any&)> conv = nullptr)
        : sourceName(src), targetName(tgt), type(t), converter(conv), isActive(true) {}
};

class DataBindingSystem {
public:
    static DataBindingSystem& GetInstance();
    
    // Veri kaynağı yönetimi
    void RegisterDataSource(const std::string& name, std::function<std::any()> getter, 
                           std::function<void(const std::any&)> setter = nullptr);
    void UnregisterDataSource(const std::string& name);
    
    // Veri bağlama
    void Bind(const std::string& sourceName, const std::string& targetName, 
              DataBindingType type = DataBindingType::OneWay,
              std::function<std::any(const std::any&)> converter = nullptr);
    void Unbind(const std::string& sourceName, const std::string& targetName);
    void UnbindAll(const std::string& name);
    
    // Veri güncelleme
    void UpdateSource(const std::string& name, const std::any& value);
    std::any GetSourceValue(const std::string& name) const;
    
    // Bağlantı yönetimi
    void SetBindingActive(const std::string& sourceName, const std::string& targetName, bool active);
    bool IsBindingActive(const std::string& sourceName, const std::string& targetName) const;
    
    // Toplu güncelleme
    void UpdateAllBindings();
    
    // DataSource kontrolü
    bool HasDataSource(const std::string& name) const;
    std::vector<std::string> GetDataSourceNames() const;
    
    // Binding bilgileri
    std::vector<std::shared_ptr<DataBinding>> GetBindingsForSource(const std::string& sourceName) const;
    std::vector<std::shared_ptr<DataBinding>> GetBindingsForTarget(const std::string& targetName) const;
    
private:
    DataBindingSystem() = default;
    ~DataBindingSystem() = default;
    
    // Copy constructor ve assignment operator'ı sil
    DataBindingSystem(const DataBindingSystem&) = delete;
    DataBindingSystem& operator=(const DataBindingSystem&) = delete;
    
    struct DataSource {
        std::function<std::any()> getter;
        std::function<void(const std::any&)> setter;
        std::any currentValue;
        bool hasSetter;
        
        DataSource(std::function<std::any()> g, std::function<void(const std::any&)> s = nullptr)
            : getter(g), setter(s), hasSetter(s != nullptr) {
            if (getter) {
                currentValue = getter();
            }
        }
    };
    
    std::unordered_map<std::string, DataSource> m_dataSources;
    std::vector<std::shared_ptr<DataBinding>> m_bindings;
    std::unordered_map<std::string, std::vector<std::weak_ptr<DataBinding>>> m_sourceBindings;
    std::unordered_map<std::string, std::vector<std::weak_ptr<DataBinding>>> m_targetBindings;
    mutable std::mutex m_mutex;
    
    void UpdateBinding(const std::shared_ptr<DataBinding>& binding);
    void NotifyValueChanged(const std::string& name);
    void CleanupExpiredBindings();
};

} // namespace AstralEngine