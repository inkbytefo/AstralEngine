#include "Engine.h"
#include "Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/UI/UISubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Events/EventManager.h"
#include <chrono>
#include <thread>

namespace AstralEngine {

Engine::Engine() {
    Logger::Info("Engine", "Engine instance created");

    // Core Subsystems Registration
    RegisterSubsystem<PlatformSubsystem>();
    RegisterSubsystem<RenderSubsystem>();
    RegisterSubsystem<AssetSubsystem>();
    
#ifdef ASTRAL_USE_IMGUI
    RegisterSubsystem<UISubsystem>();
#endif
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
    try {
        Initialize();
    } catch (const std::exception& e) {
        Logger::Critical("Engine", "Engine initialization failed: {}", e.what());
        return;
    }
    
    Logger::Info("Engine", "Starting application...");
    try {
        m_application->OnStart(this);
    } catch (const std::exception& e) {
        Logger::Error("Engine", "Application OnStart failed: {}", e.what());
        Shutdown();
        return;
    }

    m_isRunning = true;
    
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (m_isRunning) {
        try {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;
            
            // 1. Update engine-level systems
            try {
                Update();
            } catch (const std::exception& e) {
                Logger::Warning("Engine", "Engine update failed: {}", e.what());
            }
            
            // 2. PreUpdate aşaması (Input, Platform Events)
            auto preUpdateIt = m_subsystemsByStage.find(UpdateStage::PreUpdate);
            if (preUpdateIt != m_subsystemsByStage.end()) {
                for (auto& subsystem : preUpdateIt->second) {
                    try {
                        subsystem->OnUpdate(deltaTime);
                    } catch (const std::exception& e) {
                        Logger::Error("Engine", "PreUpdate failed for subsystem {}: {}", subsystem->GetName(), e.what());
                        // Continue with other subsystems
                    }
                }
            }
            
            // 3. Event processing
            try {
                EventManager::GetInstance().ProcessEvents();
            } catch (const std::exception& e) {
                Logger::Warning("Engine", "Event processing failed: {}", e.what());
            }
            
            // 4. Main Update aşaması (Game Logic, ECS Systems)
            auto updateIt = m_subsystemsByStage.find(UpdateStage::Update);
            if (updateIt != m_subsystemsByStage.end()) {
                for (auto& subsystem : updateIt->second) {
                    try {
                        subsystem->OnUpdate(deltaTime);
                    } catch (const std::exception& e) {
                        Logger::Error("Engine", "Update failed for subsystem {}: {}", subsystem->GetName(), e.what());
                        // Continue with other subsystems
                    }
                }
            }
            
            // 5. Application Logic Update
            try {
                m_application->OnUpdate(deltaTime);
            } catch (const std::exception& e) {
                Logger::Error("Engine", "Application update failed: {}", e.what());
                // Continue running, don't crash the engine
            }
            
            // 6. PostUpdate aşaması (Physics, etc.)
            auto postUpdateIt = m_subsystemsByStage.find(UpdateStage::PostUpdate);
            if (postUpdateIt != m_subsystemsByStage.end()) {
                for (auto& subsystem : postUpdateIt->second) {
                    try {
                        subsystem->OnUpdate(deltaTime);
                    } catch (const std::exception& e) {
                        Logger::Error("Engine", "PostUpdate failed for subsystem {}: {}", subsystem->GetName(), e.what());
                        // Continue with other subsystems
                    }
                }
            }

            // 7. UI aşaması (ImGui updates, command list generation)
            auto uiIt = m_subsystemsByStage.find(UpdateStage::UI);
            if (uiIt != m_subsystemsByStage.end()) {
                for (auto& subsystem : uiIt->second) {
                    try {
                        subsystem->OnUpdate(deltaTime);
                    } catch (const std::exception& e) {
                        Logger::Error("Engine", "UI update failed for subsystem {}: {}", subsystem->GetName(), e.what());
                        // Continue with other subsystems
                    }
                }
            }

            // 8. Render aşaması
            auto renderIt = m_subsystemsByStage.find(UpdateStage::Render);
            if (renderIt != m_subsystemsByStage.end()) {
                for (auto& subsystem : renderIt->second) {
                    try {
                        subsystem->OnUpdate(deltaTime);
                    } catch (const std::exception& e) {
                        Logger::Error("Engine", "Render failed for subsystem {}: {}", subsystem->GetName(), e.what());
                        // Continue with other subsystems
                    }
                }
            }
            
            // 9. Shutdown check
            auto* platformSubsystem = GetSubsystem<PlatformSubsystem>();
            if (platformSubsystem && platformSubsystem->GetWindow()->ShouldClose()) {
                RequestShutdown();
            }
            
        } catch (const std::exception& e) {
            Logger::Error("Engine", "Frame update failed: {}", e.what());
            // Continue running, don't crash the engine
            // The error might be transient
        }
    }
    
    Logger::Info("Engine", "Shutting down application...");
    try {
        m_application->OnShutdown();
    } catch (const std::exception& e) {
        Logger::Error("Engine", "Application OnShutdown failed: {}", e.what());
        // Continue with engine shutdown
    }
    
    try {
        Shutdown();
    } catch (const std::exception& e) {
        Logger::Error("Engine", "Engine shutdown failed: {}", e.what());
        // Log the error but don't throw
    }
    
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
            try {
                Logger::Info("Engine", "Initializing subsystem: {}", subsystem->GetName());
                subsystem->OnInitialize(this);
                Logger::Info("Engine", "Successfully initialized subsystem: {}", subsystem->GetName());
            } catch (const std::exception& e) {
                Logger::Error("Engine", "Failed to initialize subsystem {}: {}", subsystem->GetName(), e.what());
                throw std::runtime_error(std::string("Subsystem initialization failed: ") + subsystem->GetName() + " - " + e.what());
            }
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
    
    // Subsystem'leri kayıt sırasının tersine göre kapat (LIFO)
    // Bu, bağımlılıkların (AssetManager, RenderDevice) en son kapanmasını garanti eder.
    for (auto it = m_registrationOrder.rbegin(); it != m_registrationOrder.rend(); ++it) {
        try {
            Logger::Info("Engine", "Shutting down subsystem: {}", (*it)->GetName());
            (*it)->OnShutdown();
            Logger::Info("Engine", "Successfully shutdown subsystem: {}", (*it)->GetName());
        } catch (const std::exception& e) {
            Logger::Error("Engine", "Failed to shutdown subsystem {}: {}", (*it)->GetName(), e.what());
        }
    }
    
    m_initialized = false;
    m_isRunning = false;
    
    Logger::Info("Engine", "Engine shutdown complete");
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
