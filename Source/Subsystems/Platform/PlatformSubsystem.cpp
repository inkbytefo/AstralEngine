#include "PlatformSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../../Events/EventManager.h"
#include "../../Events/Event.h"

#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL.h>
#endif

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
    
    // EventManager'a input event'leri için abone ol
    auto& eventManager = EventManager::GetInstance();
    
    // KeyPressedEvent handler
    auto keyPressedHandler = [this](Event& event) -> bool {
        KeyPressedEvent& keyEvent = static_cast<KeyPressedEvent&>(event);
#ifdef ASTRAL_USE_SDL3
        // SDL keycode'ı Astral Engine KeyCode'ına çevir
        KeyCode engineKey = static_cast<KeyCode>(keyEvent.GetKeyCode());
        if (engineKey != KeyCode::Unknown) {
            m_inputManager->OnKeyEvent(engineKey, true);
        }
#endif
        return false; // Event'i diğer sistemlere de gönder
    };
    m_eventSubscriptions.push_back(eventManager.Subscribe<KeyPressedEvent>(keyPressedHandler));
    
    // KeyReleasedEvent handler
    auto keyReleasedHandler = [this](Event& event) -> bool {
        KeyReleasedEvent& keyEvent = static_cast<KeyReleasedEvent&>(event);
#ifdef ASTRAL_USE_SDL3
        // SDL keycode'ı Astral Engine KeyCode'ına çevir
        KeyCode engineKey = static_cast<KeyCode>(keyEvent.GetKeyCode());
        if (engineKey != KeyCode::Unknown) {
            m_inputManager->OnKeyEvent(engineKey, false);
        }
#endif
        return false; // Event'i diğer sistemlere de gönder
    };
    m_eventSubscriptions.push_back(eventManager.Subscribe<KeyReleasedEvent>(keyReleasedHandler));
    
    // MouseButtonPressedEvent handler
    auto mouseButtonPressedHandler = [this](Event& event) -> bool {
        MouseButtonPressedEvent& mouseEvent = static_cast<MouseButtonPressedEvent&>(event);
#ifdef ASTRAL_USE_SDL3
        // SDL button'ı Astral Engine MouseButton'ına çevir
        MouseButton engineButton = static_cast<MouseButton>(mouseEvent.GetMouseButton());
        if (engineButton < MouseButton::MAX_BUTTONS) {
            m_inputManager->OnMouseButtonEvent(engineButton, true);
        }
#endif
        return false; // Event'i diğer sistemlere de gönder
    };
    m_eventSubscriptions.push_back(eventManager.Subscribe<MouseButtonPressedEvent>(mouseButtonPressedHandler));
    
    // MouseButtonReleasedEvent handler
    auto mouseButtonReleasedHandler = [this](Event& event) -> bool {
        MouseButtonReleasedEvent& mouseEvent = static_cast<MouseButtonReleasedEvent&>(event);
#ifdef ASTRAL_USE_SDL3
        // SDL button'ı Astral Engine MouseButton'ına çevir
        MouseButton engineButton = static_cast<MouseButton>(mouseEvent.GetMouseButton());
        if (engineButton < MouseButton::MAX_BUTTONS) {
            m_inputManager->OnMouseButtonEvent(engineButton, false);
        }
#endif
        return false; // Event'i diğer sistemlere de gönder
    };
    m_eventSubscriptions.push_back(eventManager.Subscribe<MouseButtonReleasedEvent>(mouseButtonReleasedHandler));
    
    // MouseMovedEvent handler
    auto mouseMovedHandler = [this](Event& event) -> bool {
        MouseMovedEvent& mouseEvent = static_cast<MouseMovedEvent&>(event);
        m_inputManager->OnMouseMoveEvent(mouseEvent.GetX(), mouseEvent.GetY());
        return false; // Event'i diğer sistemlere de gönder
    };
    m_eventSubscriptions.push_back(eventManager.Subscribe<MouseMovedEvent>(mouseMovedHandler));
    
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
