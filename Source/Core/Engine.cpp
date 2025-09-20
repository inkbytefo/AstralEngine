#include "Engine.h"
#include "Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Events/EventManager.h"
#include <chrono>
#include <thread>

namespace AstralEngine {

Engine::Engine() {
    Logger::Info("Engine", "Engine instance created");
}

Engine::~Engine() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Info("Engine", "Engine instance destroyed");
}

void Engine::Run(IApplication* application) {
    if (m_isRunning) {
        Logger::Warning("Engine", "Engine is already running!");
        return;
    }

    m_application = application;
    if (!m_application) {
        Logger::Critical("Engine", "Application instance is null!");
        return;
    }

    Logger::Info("Engine", "Starting engine...");
    Initialize();
    
    Logger::Info("Engine", "Starting application...");
    m_application->OnStart(this);

    m_isRunning = true;
    
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (m_isRunning) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        // 1. Update engine-level systems
        Update();
        
        // 2. PreUpdate aşaması (Input, Platform Events)
        auto preUpdateIt = m_subsystemsByStage.find(UpdateStage::PreUpdate);
        if (preUpdateIt != m_subsystemsByStage.end()) {
            for (auto& subsystem : preUpdateIt->second) {
                subsystem->OnUpdate(deltaTime);
            }
        }
        
        // 3. Event processing
        EventManager::GetInstance().ProcessEvents();
        
        // 4. Main Update aşaması (Game Logic, ECS Systems)
        auto updateIt = m_subsystemsByStage.find(UpdateStage::Update);
        if (updateIt != m_subsystemsByStage.end()) {
            for (auto& subsystem : updateIt->second) {
                subsystem->OnUpdate(deltaTime);
            }
        }
        
        // 5. Application Logic Update
        m_application->OnUpdate(deltaTime);
        
        // 6. PostUpdate aşaması (Physics, etc.)
        auto postUpdateIt = m_subsystemsByStage.find(UpdateStage::PostUpdate);
        if (postUpdateIt != m_subsystemsByStage.end()) {
            for (auto& subsystem : postUpdateIt->second) {
                subsystem->OnUpdate(deltaTime);
            }
        }

        // 7. UI aşaması (ImGui updates, command list generation)
        auto uiIt = m_subsystemsByStage.find(UpdateStage::UI);
        if (uiIt != m_subsystemsByStage.end()) {
            for (auto& subsystem : uiIt->second) {
                subsystem->OnUpdate(deltaTime);
            }
        }

        // 8. Render aşaması
        auto renderIt = m_subsystemsByStage.find(UpdateStage::Render);
        if (renderIt != m_subsystemsByStage.end()) {
            for (auto& subsystem : renderIt->second) {
                subsystem->OnUpdate(deltaTime);
            }
        }
        
        // 8. Shutdown check
        auto* platformSubsystem = GetSubsystem<PlatformSubsystem>();
        if (platformSubsystem && platformSubsystem->GetWindow()->ShouldClose()) {
            RequestShutdown();
        }
    }
    
    Logger::Info("Engine", "Shutting down application...");
    m_application->OnShutdown();
    
    Shutdown();
    Logger::Info("Engine", "Engine shutdown complete");
}

void Engine::Initialize() {
    if (m_initialized) {
        Logger::Warning("Engine", "Engine already initialized!");
        return;
    }
    
    Logger::Info("Engine", "Initializing engine and subsystems...");
    
    // Tüm subsystem'leri başlat
    for (auto& [stage, subsystems] : m_subsystemsByStage) {
        for (auto& subsystem : subsystems) {
            Logger::Info("Engine", "Initializing subsystem: {}", subsystem->GetName());
            subsystem->OnInitialize(this);
        }
    }
    
    m_initialized = true;
    Logger::Info("Engine", "Engine initialization complete");
}

void Engine::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("Engine", "Shutting down engine and subsystems...");
    
    // Subsystem'leri ters sırada kapat (LIFO)
    for (auto it = m_subsystemsByStage.rbegin(); it != m_subsystemsByStage.rend(); ++it) {
        for (auto subsystem_it = it->second.rbegin(); subsystem_it != it->second.rend(); ++subsystem_it) {
            Logger::Info("Engine", "Shutting down subsystem: {}", (*subsystem_it)->GetName());
            (*subsystem_it)->OnShutdown();
        }
    }
    
    m_initialized = false;
    m_isRunning = false;
    
    Logger::Info("Engine", "Engine shutdown initiated");
}

void Engine::Update() {
    // Engine seviyesinde frame başına yapılacak işlemler
    // Örneğin: performans metrikleri, bellek yönetimi vb.
}

void Engine::RequestShutdown() {
    Logger::Info("Engine", "Shutdown requested");
    m_isRunning = false;
}

} // namespace AstralEngine
