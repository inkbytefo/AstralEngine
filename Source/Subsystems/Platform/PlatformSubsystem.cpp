#include "PlatformSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"

namespace AstralEngine {

PlatformSubsystem::PlatformSubsystem() {
    Logger::Debug("PlatformSubsystem", "PlatformSubsystem created");
}

PlatformSubsystem::~PlatformSubsystem() {
    Logger::Debug("PlatformSubsystem", "PlatformSubsystem destroyed");
}

void PlatformSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("PlatformSubsystem", "Initializing platform subsystem...");
    
    // SDL3 version bilgisini logla
#ifdef ASTRAL_USE_SDL3
    Logger::Info("PlatformSubsystem", "Using SDL3 for platform abstraction");
#else
    Logger::Warning("PlatformSubsystem", "SDL3 not available - using placeholder implementation");
#endif
    
    // Window oluştur
    m_window = std::make_unique<Window>();
    if (!m_window->Initialize("Astral Engine v0.1.0-alpha", 1280, 720)) {
        Logger::Error("PlatformSubsystem", "Failed to create window!");
        throw std::runtime_error("Window initialization failed");
    }
    
    // Input Manager'ı başlat ve window ile bağla
    m_inputManager = std::make_unique<InputManager>();
    if (!m_inputManager->Initialize(m_window.get())) {
        Logger::Error("PlatformSubsystem", "Failed to initialize input manager!");
        throw std::runtime_error("InputManager initialization failed");
    }
    
    // Render context oluştur
    if (!m_window->CreateRenderContext()) {
        Logger::Warning("PlatformSubsystem", "Failed to create render context - continuing without it");
    }
    
    Logger::Info("PlatformSubsystem", "Platform subsystem initialized successfully");
    Logger::Info("PlatformSubsystem", "Window: {}x{}, VSync: enabled", 
               m_window->GetWidth(), m_window->GetHeight());
}

void PlatformSubsystem::OnUpdate(float deltaTime) {
    // Pencere olaylarını işle
    if (m_window) {
        m_window->PollEvents();
        
        // Pencere kapatma kontrolü
        if (m_window->ShouldClose()) {
            if (m_owner) {
                m_owner->RequestShutdown();
            }
        }
    }
    
    // Input durumunu güncelle
    if (m_inputManager) {
        m_inputManager->Update();
    }
}

void PlatformSubsystem::OnShutdown() {
    Logger::Info("PlatformSubsystem", "Shutting down platform subsystem...");
    
    // Event subscription'larını temizle
    auto& eventManager = EventManager::GetInstance();
    for (auto handlerId : m_eventSubscriptions) {
        eventManager.Unsubscribe(handlerId);
    }
    m_eventSubscriptions.clear();
    Logger::Debug("PlatformSubsystem", "Event subscriptions cleared");
    
    if (m_inputManager) {
        m_inputManager->Shutdown();
        m_inputManager.reset();
    }
    
    if (m_window) {
        m_window->Shutdown();
        m_window.reset();
    }
    
    Logger::Info("PlatformSubsystem", "Platform subsystem shutdown complete");
}

bool PlatformSubsystem::IsWindowOpen() const {
    return m_window && !m_window->ShouldClose();
}

void PlatformSubsystem::CloseWindow() {
    if (m_window) {
        m_window->Close();
    }
}

} // namespace AstralEngine
