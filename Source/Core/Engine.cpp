#include "Engine.h"
#include "Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
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

void Engine::Run() {
    if (m_isRunning) {
        Logger::Warning("Engine", "Engine is already running!");
        return;
    }

    Logger::Info("Engine", "Starting engine...");
    Initialize();
    
    m_isRunning = true;
    
    // Ana döngü
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (m_isRunning) {
        Logger::Debug("Engine", "New frame started");
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        // Frame'i güncelle
        Update();
        
        // Her frame'de tüm subsystem'leri güncelle
        for (auto& subsystem : m_subsystems) {
            Logger::Debug("Engine", "Updating subsystem: {}", subsystem->GetName());
            subsystem->OnUpdate(deltaTime);
            Logger::Debug("Engine", "Subsystem {} updated", subsystem->GetName());
        }
        
        // PlatformSubsystem'den pencere durumunu kontrol et
        auto* platformSubsystem = GetSubsystem<PlatformSubsystem>();
        if (platformSubsystem) {
            auto* window = platformSubsystem->GetWindow();
            if (window && window->ShouldClose()) {
                Logger::Info("Engine", "Window close requested, shutting down...");
                m_isRunning = false;
                break;
            }
        }
        
        // CPU kullanımını azaltmak için minimal bir sleep
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
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
