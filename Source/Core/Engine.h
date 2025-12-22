#pragma once

#include "ISubsystem.h"
#include "IApplication.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <typeindex>
#include <filesystem>

// Forward declaration
namespace AstralEngine {
    class EventManager;

/**
 * @brief Motorun çekirdek orkestratörü.
 *
 * Tüm alt sistemlerin yaşam döngüsünü yönetir ve ana döngüyü çalıştırır.
 * Subsystem'lerin kaydedilmesi, başlatılması, güncellenmesi ve kapatılması
 * işlemlerinden sorumludur.
 */
class Engine {
public:
    Engine();
    ~Engine();

    // Motorun ana yaşam döngüsünü başlatır.
    void Run(IApplication* application);

    // Bir alt sistemi kaydeder. Initialize sırasında çağrılır.
    template<typename T, typename... Args>
    void RegisterSubsystem(Args&&... args);

    // Kayıtlı bir alt sisteme erişim sağlar.
    template<typename T>
    T* GetSubsystem() const;

    // Motorun kapatılması için sinyal gönderir.
    void RequestShutdown();

    // Motor çalışıyor mu?
    bool IsRunning() const { return m_isRunning; }

    // Uygulamanın temel yolunu ayarlar
    void SetBasePath(const std::filesystem::path& path) { m_basePath = path; }

    // Uygulamanın temel yolunu döndürür
    const std::filesystem::path& GetBasePath() const { return m_basePath; }

private:
    void Initialize();
    void Shutdown();
    void Update();

    // Subsystem'leri güncelleme aşamalarına göre organize et
    std::map<UpdateStage, std::vector<std::unique_ptr<ISubsystem>>> m_subsystemsByStage;
    // Hızlı erişim için subsystem'leri tip indeksine göre haritala
    std::unordered_map<std::type_index, ISubsystem*> m_subsystemMap;
    // List of subsystems in order of registration (for correct shutdown)
    std::vector<ISubsystem*> m_registrationOrder;
    
    std::filesystem::path m_basePath;
    bool m_isRunning = false;
    bool m_initialized = false;
    IApplication* m_application = nullptr;
};

// Template implementations
template<typename T, typename... Args>
void Engine::RegisterSubsystem(Args&&... args) {
    static_assert(std::is_base_of_v<ISubsystem, T>, "T must derive from ISubsystem");
    
    auto subsystem = std::make_unique<T>(std::forward<Args>(args)...);
    UpdateStage stage = subsystem->GetUpdateStage();
    m_subsystemMap[typeid(T)] = subsystem.get();
    m_registrationOrder.push_back(subsystem.get());
    m_subsystemsByStage[stage].push_back(std::move(subsystem));
}

template<typename T>
T* Engine::GetSubsystem() const {
    auto it = m_subsystemMap.find(typeid(T));
    if (it != m_subsystemMap.end()) {
        return static_cast<T*>(it->second);
    }
    return nullptr;
}

} // namespace AstralEngine
