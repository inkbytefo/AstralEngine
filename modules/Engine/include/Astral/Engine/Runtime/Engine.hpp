#pragma once

#include "Astral/Engine/Assets/AssetManager.hpp"
#include "Astral/Engine/Platform/Input.hpp"
#include "Astral/Engine/Physics/PhysicsWorld.hpp"
#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Engine/Ecs/System.hpp"
#include "Astral/Engine/Platform/Window.hpp"
#include "Astral/Render/Renderer.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Astral {

class RenderSync;
class Engine;

struct EngineConfig {
    std::string title = "Astral";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool headless = false;                       // no window: render offscreen (tests, servers, tools)
    bool validation = false;                     // Vulkan validation layers (also ASTRAL_VALIDATION=1)
    std::filesystem::path assetRoot = "assets";
    float fixedTimestep = 1.0f / 60.0f;
    uint32_t maxFixedStepsPerFrame = 8;          // avoids the "spiral of death" after a hitch
    int64_t maxFrames = -1;                      // stop after this many frames (-1 = run until closed)
    Render::SdfVolumeSettings sdfVolume;
    PhysicsSettings physics;
};

// Your game. Build the scene and register systems in OnStart; per-frame logic lives in systems.
class Game {
public:
    virtual ~Game() = default;
    virtual void OnStart(Engine& engine) = 0;
    virtual void OnStop(Engine&) {}
};

// Owns the engine services and drives the frame:
//   input -> fixed steps (FixedUpdate systems + physics) -> Update -> LateUpdate
//   -> transforms -> render sync -> render
// Run() does all of it; tools (the editor) call the individual steps instead.
class Engine {
public:
    explicit Engine(const EngineConfig& config = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    int Run(Game& game);
    void Quit() { m_QuitRequested = true; }

    // ---- services ----
    Scene& GetScene() { return *m_Scene; }
    void SetScene(std::unique_ptr<Scene> scene);
    bool LoadScene(const std::filesystem::path& path);   // relative to the asset root
    Render::Renderer& GetRenderer() { return *m_Renderer; }
    AssetManager& GetAssets() { return *m_Assets; }
    PhysicsWorld& GetPhysics() { return *m_Physics; }
    Input& GetInput() { return m_Input; }
    Window* GetWindow() { return m_Window.get(); }
    const EngineConfig& GetConfig() const { return m_Config; }

    // ---- systems ----
    template <typename T, typename... Args>
    T& AddSystem(Phase phase, Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        m_Systems.push_back({phase, std::move(system)});
        if (m_Playing) ref.OnStart(*this, *m_Scene);
        return ref;
    }
    void ClearSystems();

    // ---- fine-grained frame control (used by Run and by tools) ----
    void StartPlay();          // starts physics and systems
    void StopPlay();
    bool IsPlaying() const { return m_Playing; }
    void Simulate(float deltaTime);   // runs systems/physics when playing; always updates transforms
    void SyncRender();                // pushes the scene into the renderer's world
    Render::ViewHandle GetMainView() const { return m_MainView; }
    Render::ViewSettings& GetViewSettings() { return m_ViewSettings; }
    // Camera of the scene's primary camera entity (or a default camera).
    Render::Camera GetGameCamera();
    double GetTime() const { return m_Time; }
    uint64_t GetFrameIndex() const { return m_Frame; }

private:
    void RunSystems(Phase phase, float dt);
    bool RenderFrame();

    EngineConfig m_Config;
    Input m_Input;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Render::Renderer> m_Renderer;
    std::unique_ptr<AssetManager> m_Assets;
    std::unique_ptr<PhysicsWorld> m_Physics;
    std::unique_ptr<RenderSync> m_RenderSync;
    std::unique_ptr<Scene> m_Scene;
    struct ScheduledSystem { Phase phase; std::unique_ptr<System> system; };
    std::vector<ScheduledSystem> m_Systems;
    Render::ViewHandle m_MainView;
    Render::ViewSettings m_ViewSettings;
    bool m_Playing = false;
    bool m_QuitRequested = false;
    float m_Accumulator = 0.0f;
    double m_Time = 0.0;
    uint64_t m_Frame = 0;
};

// Convenience entry point: `int main() { MyGame game; return Astral::RunGame(game, config); }`
int RunGame(Game& game, const EngineConfig& config = {});

} // namespace Astral
