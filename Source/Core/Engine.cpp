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
        
        // 2. Update platform subsystem (polls for events)
        auto* platformSubsystem = GetSubsystem<PlatformSubsystem>();
        if (platformSubsystem) {
            platformSubsystem->OnUpdate(deltaTime);
        }
        
        // 3. Process all queued events from the previous frame
        EventManager::GetInstance().ProcessEvents();
        
        // 4. Update all other registered subsystems
        for (auto& subsystem : m_subsystems) {
            // Skip platform subsystem as it was already updated
            if (subsystem.get() != platformSubsystem) {
                subsystem->OnUpdate(deltaTime);
            }
        }
        
        // 5. Update application logic
        m_application->OnUpdate(deltaTime);
        
        // 6. Check for shutdown requests (e.g., window close)
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
    for (auto& subsystem : m_subsystems) {
        Logger::Info("Engine", "Initializing subsystem: {}", subsystem->GetName());
        subsystem->OnInitialize(this);
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
    for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it) {
        Logger::Info("Engine", "Shutting down subsystem: {}", (*it)->GetName());
        (*it)->OnShutdown();
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
