#pragma once

#include <string>
#include <memory>
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/SystemManager.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneManager.hpp"

namespace Astral {

class Window;
class VulkanContext;
class BenchmarkLogger;
class SDFRenderer;
class EditorUI;
class PhysicsSubsystem;
class RenderExtractionSubsystem;

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
};

/// Uygulamanin temel yasam dongusunu temsil eden cekirdek sinif.
/// Renderer, Window ve Benchmark alt sistemlerini koordine eder.
class Application {
public:
    Application();
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Ana donguyu baslatir. maxFrames > 0 ise belirtilen kare kadar calisip otomatik cikar.
    void Run(int maxFrames = -1);

    static std::string GetName();
    static std::string GetVersion();

    SceneManager& GetSceneManager() { return m_SceneManager; }
    const SceneManager& GetSceneManager() const { return m_SceneManager; }
    std::shared_ptr<Scene> GetActiveScene() const { return m_SceneManager.GetActiveScene(); }

private:
    AppConfig m_Config;
    SceneManager m_SceneManager;
    SystemManager m_SystemManager;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<VulkanContext> m_VulkanContext;
    std::unique_ptr<SDFRenderer> m_SDFRenderer;
    std::unique_ptr<BenchmarkLogger> m_BenchmarkLogger;
    std::unique_ptr<EditorUI> m_EditorUI;
    PhysicsSubsystem* m_PhysicsSubsystem = nullptr;
    RenderExtractionSubsystem* m_RenderExtractionSubsystem = nullptr;
    bool m_Running = false;
};

} // namespace Astral
