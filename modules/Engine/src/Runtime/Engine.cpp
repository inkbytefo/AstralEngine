#include "Astral/Engine/Runtime/Engine.hpp"
#include "Astral/Engine/Scene/SceneSerializer.hpp"
#include "Runtime/RenderSync.hpp"

#include "Astral/Core/Log.hpp"
#include "Astral/Render/VulkanInterop.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>

namespace Astral {

Engine::Engine(const EngineConfig& config) : m_Config(config) {
    Render::RendererDesc rd;
    rd.applicationName = config.title;
    rd.enableValidation = config.validation;
    rd.sdfVolume = config.sdfVolume;
    if (!config.headless) {
        m_Window = std::make_unique<Window>(config.title, config.width, config.height, m_Input);
        rd.instanceExtensions = Window::RequiredVulkanExtensions();
    }
    m_Renderer = std::make_unique<Render::Renderer>(rd);

    glm::uvec2 size(config.width, config.height);
    if (m_Window) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(Render::GetVulkanHandles(*m_Renderer).instance, m_Window->Native(), nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("could not create a Vulkan surface for the window");
        size = m_Window->FramebufferSize();
        Render::AttachSurface(*m_Renderer, surface, size.x, size.y);
    }
    m_MainView = m_Renderer->CreateView(std::max(1u, size.x), std::max(1u, size.y));

    m_Assets = std::make_unique<AssetManager>(*m_Renderer, config.assetRoot);
    m_Physics = std::make_unique<PhysicsWorld>(config.physics);
    m_RenderSync = std::make_unique<RenderSync>(*m_Renderer, *m_Assets);
    m_Scene = std::make_unique<Scene>();
}

Engine::~Engine() {
    if (m_Playing) StopPlay();
    m_Renderer->WaitIdle();
    m_Systems.clear();
    m_RenderSync.reset();
    m_Assets.reset();
    m_Physics.reset();
    m_Renderer.reset();
    m_Window.reset();
}

void Engine::SetScene(std::unique_ptr<Scene> scene) {
    bool wasPlaying = m_Playing;
    if (wasPlaying) StopPlay();
    m_RenderSync->Clear();
    m_Scene = scene ? std::move(scene) : std::make_unique<Scene>();
    if (wasPlaying) StartPlay();
}

bool Engine::LoadScene(const std::filesystem::path& path) {
    std::string error;
    auto scene = Astral::LoadScene(path.is_absolute() ? path : m_Config.assetRoot / path, error);
    if (!scene) {
        Log::Error("engine", "cannot load scene '{}': {}", path.string(), error);
        return false;
    }
    SetScene(std::move(scene));
    return true;
}

void Engine::ClearSystems() {
    if (m_Playing)
        for (auto& s : m_Systems) s.system->OnStop(*this, *m_Scene);
    m_Systems.clear();
}

void Engine::StartPlay() {
    if (m_Playing) return;
    m_Playing = true;
    m_Accumulator = 0.0f;
    m_Time = 0.0;
    m_Physics->Start(*m_Scene);
    for (auto& s : m_Systems) s.system->OnStart(*this, *m_Scene);
}

void Engine::StopPlay() {
    if (!m_Playing) return;
    for (auto& s : m_Systems) s.system->OnStop(*this, *m_Scene);
    m_Physics->Stop(*m_Scene);
    m_Playing = false;
}

void Engine::RunSystems(Phase phase, float dt) {
    FrameContext ctx{*this, *m_Scene, m_Input, dt, m_Time, m_Frame};
    for (auto& s : m_Systems)
        if (s.phase == phase) s.system->Update(ctx);
}

void Engine::Simulate(float dt) {
    if (m_Playing) {
        const float step = m_Config.fixedTimestep;
        m_Accumulator = std::min(m_Accumulator + dt, step * float(m_Config.maxFixedStepsPerFrame));
        while (m_Accumulator >= step) {
            m_Scene->UpdateWorldTransforms();
            RunSystems(Phase::FixedUpdate, step);
            m_Physics->Step(*m_Scene, step);
            m_Accumulator -= step;
        }
        m_Scene->UpdateWorldTransforms();
        RunSystems(Phase::Update, dt);
        m_Scene->UpdateWorldTransforms();
        RunSystems(Phase::LateUpdate, dt);
        m_Time += dt;
    }
    m_Scene->UpdateWorldTransforms();
}

void Engine::SyncRender() { m_RenderSync->Sync(*m_Scene); }

Render::Camera Engine::GetGameCamera() {
    if (Entity cam = m_Scene->GetPrimaryCamera()) return MakeRenderCamera(*m_Scene, cam);
    Render::Camera fallback;
    fallback.position = glm::vec3(0.0f, 2.0f, 6.0f);
    return fallback;
}

bool Engine::RenderFrame() {
    if (m_Window) {
        glm::uvec2 size = m_Window->FramebufferSize();
        if (size.x == 0 || size.y == 0) return false; // minimised
        if (m_Renderer->GetViewSize(m_MainView) != size) {
            m_Renderer->ResizeView(m_MainView, size.x, size.y);
            m_Renderer->ResizeSurface(size.x, size.y);
        }
    }
    if (!m_Renderer->BeginFrame()) return false;
    m_Renderer->RenderView(m_MainView, GetGameCamera(), m_ViewSettings);
    m_Renderer->PresentView(m_MainView);
    m_Renderer->EndFrame();
    return true;
}

int Engine::Run(Game& game) {
    game.OnStart(*this);
    StartPlay();
    auto last = std::chrono::steady_clock::now();
    while (!m_QuitRequested && (!m_Window || !m_Window->ShouldClose())) {
        if (m_Config.maxFrames >= 0 && int64_t(m_Frame) >= m_Config.maxFrames) break;
        m_Input.BeginFrame();
        if (m_Window) m_Window->PollEvents();

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (m_Config.headless) dt = m_Config.fixedTimestep; // deterministic offscreen runs
        dt = std::min(dt, 0.25f);

        Simulate(dt);
        SyncRender();
        if (!RenderFrame() && m_Window && m_Window->FramebufferSize().x == 0) m_Window->WaitEvents(); // minimised
        ++m_Frame;
    }
    StopPlay();
    game.OnStop(*this);
    m_Renderer->WaitIdle();
    return 0;
}

int RunGame(Game& game, const EngineConfig& config) {
    try {
        Engine engine(config);
        return engine.Run(game);
    } catch (const std::exception& e) {
        Log::Error("engine", "fatal: {}", e.what());
        return 1;
    }
}

} // namespace Astral
