#pragma once

#include "IApplication.h"
#include "ISubsystem.h"
#include "Logger.h"
#include <filesystem>
#include <map>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

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
  void Run(IApplication *application);

  // Bir alt sistemi kaydeder.
  template <typename T, typename... Args>
  void RegisterSubsystem(Args &&...args);

  // Kayıtlı bir alt sisteme erişim sağlar.
  template <typename T> T *GetSubsystem() const;

  // Motorun kapatılması için sinyal gönderir.
  void RequestShutdown();

  // Motor çalışıyor mu?
  bool IsRunning() const { return m_isRunning; }

  // Uygulamanın temel yolunu ayarlar
  void SetBasePath(const std::filesystem::path &path) { m_basePath = path; }

  // Uygulamanın temel yolunu döndürür
  const std::filesystem::path &GetBasePath() const { return m_basePath; }

private:
  void Initialize();
  void Shutdown();
  void Update();

  // Owner of all subsystems in the order they were registered
  std::vector<std::unique_ptr<ISubsystem>> m_subsystemsOwned;
  // Fast access to subsystems by stage (cached pointers)
  std::unordered_map<UpdateStage, std::vector<ISubsystem *>>
      m_subsystemsByStage;
  // Fast access by type
  std::unordered_map<std::type_index, ISubsystem *> m_subsystemMap;

  std::filesystem::path m_basePath;
  bool m_isRunning = false;
  bool m_initialized = false;
  IApplication *m_application = nullptr;
};

// Template implementations
template <typename T, typename... Args>
void Engine::RegisterSubsystem(Args &&...args) {
  static_assert(std::is_base_of_v<ISubsystem, T>,
                "T must derive from ISubsystem");

  // Check if system already exists
  if (m_subsystemMap.find(typeid(T)) != m_subsystemMap.end()) {
    Logger::Warning("Engine", "Subsystem of type '{}' is already registered!",
                    typeid(T).name());
    return;
  }

  auto subsystem = std::make_unique<T>(std::forward<Args>(args)...);
  ISubsystem *ptr = subsystem.get();
  UpdateStage stage = ptr->GetUpdateStage();

  m_subsystemMap[typeid(T)] = ptr;
  m_subsystemsByStage[stage].push_back(ptr);
  m_subsystemsOwned.push_back(std::move(subsystem));

  Logger::Debug("Engine", "Registered subsystem: {}", ptr->GetName());
}

template <typename T> T *Engine::GetSubsystem() const {
  auto it = m_subsystemMap.find(typeid(T));
  if (it != m_subsystemMap.end()) {
    return static_cast<T *>(it->second);
  }
  return nullptr;
}

} // namespace AstralEngine
