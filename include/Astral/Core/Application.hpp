#pragma once

#include <string>
#include <memory>
#include "Astral/Core/Window.hpp"
#include "Astral/Core/BenchmarkLogger.hpp"
#include "Astral/Core/SystemManager.hpp"
#include "Astral/Core/Events/EventBus.hpp"
#include "Astral/Core/Input/ActionMap.hpp"
#include "Astral/Core/Threading/JobSystem.hpp"
#include "Astral/Scene/SceneManager.hpp"

#include "Astral/Scene/Entity.hpp"

namespace Astral {

class Window;
class VulkanContext;
class BenchmarkLogger;
class SDFRenderer;
class PhysicsSubsystem;
class RenderExtractionSubsystem;

/// Komut satiri argumanlarini saklayan ve motora/istemciye aktaran veri yapisi.
struct CommandLineArgs {
    int argc = 0;
    char** argv = nullptr;
};

inline CommandLineArgs g_CommandLineArgs;

inline const CommandLineArgs& GetCommandLineArgs() {
    return g_CommandLineArgs;
}

inline void SetCommandLineArgs(int argc, char** argv) {
    g_CommandLineArgs = { argc, argv };
}

struct AppConfig {
    bool benchMode = false;
    int benchFrames = 200;
    std::string benchOutputFile = "";
    int width = 1280;
    int height = 720;
    uint32_t normalMode = 0; // 0 = Central Differences, 1 = Tetrahedron
    bool legacyMap = false;  // true: her kare vkMapMemory/vkUnmapMemory cagirir (benchmark karsilastirmasi)
    bool useGrid = true;     // PR-6: Two-Level BrickGrid Empty Space Skipping aktif
    bool stressTest = false; // PR-6: 32 dinamik nesneli karmasik sahne stres testi
    bool optShadow = true;   // PR-7: Golge erken terk ve back-face culling optimizasyonu aktif
    bool enableTAA = true;   // PR-8: Sub-Pixel Jitter & Temporal Anti-Aliasing (TAA) aktif
    std::string shaderPath = "";
    int maxFrames = -1;      // Belirtilen kare sayisina ulasildiginda otomatik sonlanma (-1 = sonsuz dongu)
};

/// Uygulamanin temel yasam dongusunu temsil eden cekirdek sinif.
/// Renderer, Window, SceneManager ve SystemManager alt sistemlerini koordine eder.
/// Bu sinif dogrudan orneklenemez (instantiate edilemez); oyun/istemci projeleri
/// bu siniftan turer (or. class SandboxApp : public Astral::Application).
class Application {
public:
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Ana donguyu baslatir. maxFrames > 0 ise belirtilen kare kadar calisip otomatik cikar.
    /// BILINCLI KISITLAMA: Run() metodu bilerek virtual YAPILMAMISTIR. Ana olay dongusu sabittir;
    /// motor butunlugunu, Vulkan senkronizasyonunu ve yasam dongusu siralamasini korumak amaciyla
    /// oyun tarafindan override edilemez.
    void Run(int maxFrames = -1);

    /// ISubsystem turevi bir sistemi SystemManager'a kaydeder.
    /// SystemManager nesnesi dogrudan disariya acilmaz; sistem yonetimi Application uzerinden forward edilir.
    template<typename T, typename... Args>
        requires std::derived_from<T, ISubsystem>
    T& PushSystem(Args&&... args) {
        return m_SystemManager.PushSystem<T>(std::forward<Args>(args)...);
    }

    static std::string GetName();
    static std::string GetVersion();

    SceneManager& GetSceneManager() { return m_SceneManager; }
    const SceneManager& GetSceneManager() const { return m_SceneManager; }
    std::shared_ptr<Scene> GetActiveScene() const { return m_SceneManager.GetActiveScene(); }

    const AppConfig& GetConfig() const { return m_Config; }

    // Motorun temel nesnelerine guvenli erisim saglayan public API getter'lari
    [[nodiscard]] Window* GetWindow() const noexcept { return m_Window.get(); }
    [[nodiscard]] VulkanContext* GetVulkanContext() const noexcept { return m_VulkanContext.get(); }
    [[nodiscard]] SDFRenderer* GetRenderer() const noexcept { return m_SDFRenderer.get(); }
    [[nodiscard]] BenchmarkLogger* GetBenchmarkLogger() const noexcept { return m_BenchmarkLogger.get(); }

    [[nodiscard]] EventBus& GetEventBus() noexcept { return m_EventBus; }
    [[nodiscard]] const EventBus& GetEventBus() const noexcept { return m_EventBus; }
    [[nodiscard]] ActionMap& GetActionMap() noexcept { return m_ActionMap; }
    [[nodiscard]] const ActionMap& GetActionMap() const noexcept { return m_ActionMap; }

    [[nodiscard]] JobSystem& GetJobSystem() noexcept { return m_JobSystem; }
    [[nodiscard]] const JobSystem& GetJobSystem() const noexcept { return m_JobSystem; }

    [[nodiscard]] Entity& GetSelectedEntity() noexcept { return m_SelectedEntity; }
    [[nodiscard]] const Entity& GetSelectedEntity() const noexcept { return m_SelectedEntity; }
    void SetSelectedEntity(const Entity& entity) { m_SelectedEntity = entity; }

protected:
    /// Base Application yapicisi yalnizca turetilmis siniflar tarafindan cagrilabilir.
    /// 
    /// PIPELINE SIRALAMASI:
    /// Motorun ZORUNLU cekirdek sistemleri (InputSubsystem, PhysicsSubsystem, TransformSubsystem,
    /// RenderExtractionSubsystem) base Application yapicisinda oncelikli olarak kaydedilir.
    /// Oyuna ozel alt sistemler ise turetilmis sinifin KENDI yapicisinda (base yapicidan SONRA)
    /// PushSystem<T>() cagirilarak eklenir.
    /// Boylece sistem guncelleme sirasi her zaman:
    /// [Motor Cekirdek Sistemleri] -> [Oyuna Ozel Sistemler]
    /// seklinde gerceklesir. Bu siralama mimari acidan dogru ve gereklidir, cunku oyun mantigi
    /// (gameplay kurallari, ozel tetikleyiciler vb.) motorun sagladigi temel girdi, fizik ve
    /// dunya donusum verileri uzerine insa edilir.
    Application();
    explicit Application(const AppConfig& config);

    AppConfig& GetConfig() { return m_Config; }

private:
    AppConfig m_Config;
    JobSystem m_JobSystem;
    SceneManager m_SceneManager;
    SystemManager m_SystemManager;
    EventBus m_EventBus;
    ActionMap m_ActionMap;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<VulkanContext> m_VulkanContext;
    std::unique_ptr<SDFRenderer> m_SDFRenderer;
    std::unique_ptr<BenchmarkLogger> m_BenchmarkLogger;
    PhysicsSubsystem* m_PhysicsSubsystem = nullptr;
    RenderExtractionSubsystem* m_RenderExtractionSubsystem = nullptr;
    Entity m_SelectedEntity;
    bool m_Running = false;
};

/// Her oyun projesinin (istemci) kendi Application turevini olusturup dondurmesi
/// icin tanimlamasi ZORUNLU olan fabrika fonksiyonu sozlesmesi.
/// Motor bu fonksiyonun implementasyonunu saglamaz; sozlesme istemci tarafina aittir.
Application* CreateApplication();

} // namespace Astral

