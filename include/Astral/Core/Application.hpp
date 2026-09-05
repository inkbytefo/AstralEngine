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
    bool headless = false;   // true: GPU'suz, penceresiz saf CPU simülasyon modu (CI, Server ve Headless Test)
    bool benchMode = false;
    int benchFrames = 200;
    std::string benchOutputFile = "";
    int width = 1280;
    int height = 720;
    uint32_t normalMode = 0; // 0 = Central Differences, 1 = Tetrahedron
    bool legacyMap = false;  // true: her kare vkMapMemory/vkUnmapMemory cagirir (benchmark karsilastirmasi)
    bool useGrid = true;     // PR-6: Two-Level BrickGrid Empty Space Skipping aktif
    bool simulatePhysics = true; // Client policy; independent of benchmark/frame limits.
    bool isPaused = false;       // true: simulasyon duraklatildi (fizik ve gameplay ilerlemez)
    float fixedTimeStep = 1.0f / 60.0f; // 60 Hz sabit simulasyon adimi
    float fixedDeltaTime = 0.0f; // 0.0f = gercek zaman sayaci; > 0.0f = deterministik kare delta suresi (Headless testler ve simülasyon)
    float maxFrameDelta = 0.25f; // Spiral of death onleyici maksimum kare biriktirme suresi
    uint32_t maxSubSteps = 8;    // Bir karede calistirilabilecek en fazla sabit alt-adim
    bool optShadow = true;   // PR-7: Golge erken terk ve back-face culling optimizasyonu aktif
    bool enableTAA = true;   // PR-8: Sub-Pixel Jitter & Temporal Anti-Aliasing (TAA) aktif
    bool useGBuffer = true;  // Faz 1: Deferred G-Buffer & Motion Vectors hattı (Varsayılan aktif)
    int debugMode = 0;       // G-Buffer Debug: 0=Shaded, 1=Albedo, 2=Normal, 3=Depth, 4=Motion, 5=Material
    std::string shaderPath = "";
    int maxFrames = -1;      // Belirtilen kare sayisina ulasildiginda otomatik sonlanma (-1 = sonsuz dongu)
};

/// @brief Donanimsal veya yazilimsal nesne secim (picking / raycast) sonucu genel veri yapisi
struct RuntimePickResult {
    bool hasHit = false;
    EntityHandle hitEntity = NullEntityHandle;
    int32_t hitIndex = -1;
    glm::vec3 hitPoint{0.0f};
    float hitDistance = 0.0f;
};

/// @brief Donanim picking islemi tamamlandiginda EventBus uzerinden firlatilan genel olay
struct RuntimePickEvent {
    RuntimePickResult result;
    Scene* scene = nullptr;
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
    void SetActiveScene(std::shared_ptr<Scene> scene) { m_SceneManager.SetActiveScene(std::move(scene)); }

    const AppConfig& GetConfig() const { return m_Config; }

    [[nodiscard]] bool IsHeadless() const noexcept { return m_Config.headless; }
    void SetHeadless(bool headless) noexcept { m_Config.headless = headless; }

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

    // Genel Runtime Picking ve Shader Vurgulama API'si
    [[nodiscard]] const RuntimePickResult& GetLastPickResult() const noexcept { return m_LastPickResult; }
    void RequestPick(int screenX, int screenY);
    void SetHighlightEntity(EntityHandle entity) noexcept { m_HighlightEntity = entity; }
    [[nodiscard]] EntityHandle GetHighlightEntity() const noexcept { return m_HighlightEntity; }

    [[nodiscard]] uint32_t GetTotalFramesRendered() const noexcept { return m_TotalFramesRendered; }

    void SetPaused(bool paused) noexcept { m_Config.isPaused = paused; }
    [[nodiscard]] bool IsPaused() const noexcept { return m_Config.isPaused; }
    void SetPhysicsSimulated(bool simulate) noexcept { m_Config.simulatePhysics = simulate; }
    [[nodiscard]] bool IsPhysicsSimulated() const noexcept { return m_Config.simulatePhysics; }

    void SetFixedTimeStep(float dt) noexcept { m_Config.fixedTimeStep = dt; }
    [[nodiscard]] float GetFixedTimeStep() const noexcept { return m_Config.fixedTimeStep; }

    void SetMaxSubSteps(uint32_t steps) noexcept { m_Config.maxSubSteps = steps; }
    [[nodiscard]] uint32_t GetMaxSubSteps() const noexcept { return m_Config.maxSubSteps; }

    [[nodiscard]] float GetAccumulator() const noexcept { return m_Accumulator; }
    [[nodiscard]] float GetInterpolationAlpha() const noexcept { return m_InterpolationAlpha; }

protected:
    /// Base Application yapicisi yalnizca turetilmis siniflar tarafindan cagrilabilir.
    /// 
    /// OnUpdate runs in the gameplay phase, before core physics/transform/extraction.
    /// Systems registered with PushSystem run in their declared SystemStage.
    Application();
    explicit Application(const AppConfig& config);

    /// Called once by Run, after GPU setup and before system initialization.
    /// The default is a genuinely empty scene (no implicit geometry or camera).
    [[nodiscard]] virtual std::shared_ptr<Scene> CreateInitialScene();
    /// Register client systems here. The initial scene and rendering services are ready.
    virtual void OnInitialize() {}
    /// Before core simulation/transform/extraction. GPU submission remains engine-owned.
    virtual void OnUpdate(FrameContext&, uint32_t /*frameIndex*/) {}

    AppConfig& GetConfig() { return m_Config; }

private:
    void Cleanup();

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
    RuntimePickResult m_LastPickResult{};
    EntityHandle m_HighlightEntity = NullEntityHandle;
    InputSystem m_HeadlessInput;
    bool m_Running = false;
    uint32_t m_TotalFramesRendered = 0;
    float m_Accumulator = 0.0f;
    float m_InterpolationAlpha = 1.0f;
};

/// Her oyun projesinin (istemci) kendi Application turevini olusturup dondurmesi
/// icin tanimlamasi ZORUNLU olan fabrika fonksiyonu sozlesmesi.
/// Motor bu fonksiyonun implementasyonunu saglamaz; sozlesme istemci tarafina aittir.
Application* CreateApplication();

} // namespace Astral
