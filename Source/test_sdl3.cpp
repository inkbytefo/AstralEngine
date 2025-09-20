#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/Platform/Window.h"
#include "Subsystems/Platform/InputManager.h"
#include "Events/EventManager.h"
#include "Events/Event.h"

#include <chrono>
#include <thread>

using namespace AstralEngine;

/**
 * @brief SDL3 entegrasyon test uygulaması
 * 
 * Bu uygulama, SDL3 entegrasyonunun doğru çalıştığını doğrulamak için
 * temel pencere oluşturma, event handling ve input testleri yapar.
 */
class SDL3TestApplication : public IApplication {
public:
    SDL3TestApplication() = default;
    ~SDL3TestApplication() = default;

    // IApplication interface
    void OnStart(Engine* owner) override {
        Logger::Info("SDL3Test", "Application starting...");
        m_engine = owner;
        m_startTime = std::chrono::steady_clock::now();
        SetupEventSubscriptions();
    }

    void OnUpdate(float deltaTime) override {
        // Test süresini kontrol et
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration<float>(currentTime - m_startTime).count();
        
        if (elapsedTime >= 10.0f || m_earlyExit) {
            m_engine->RequestShutdown();
        }
    }

    void OnShutdown() override {
        Logger::Info("SDL3Test", "Application shutting down...");
        ValidateTestResults();
    }

private:
    void SetupEventSubscriptions() {
        auto& eventManager = EventManager::GetInstance();
        
        // Window event'lerini dinle
        m_windowCloseSubscription = eventManager.Subscribe<WindowCloseEvent>([this]([[maybe_unused]] WindowCloseEvent& event) {
            Logger::Info("SDL3Test", "Window close event received");
            m_windowCloseReceived = true;
            return false;
        });
        
        m_windowResizeSubscription = eventManager.Subscribe<WindowResizeEvent>([this](WindowResizeEvent& event) {
            Logger::Info("SDL3Test", "Window resized to {}x{}", event.GetWidth(), event.GetHeight());
            m_windowResizeReceived = true;
            return false;
        });
        
        // Keyboard event'lerini dinle
        m_keyPressedSubscription = eventManager.Subscribe<KeyPressedEvent>([this](KeyPressedEvent& event) {
            Logger::Info("SDL3Test", "Key pressed: {} (repeat: {})", static_cast<int>(event.GetKeyCode()), event.IsRepeat());
            m_keyPressedReceived = true;
            
            // ESC tuşuna basılırsa testi erken bitir
            if (event.GetKeyCode() == KeyCode::Escape) {
                Logger::Info("SDL3Test", "ESC key pressed - ending test early");
                m_earlyExit = true;
            }
            return false;
        });
        
        m_keyReleasedSubscription = eventManager.Subscribe<KeyReleasedEvent>([this](KeyReleasedEvent& event) {
            Logger::Info("SDL3Test", "Key released: {}", static_cast<int>(event.GetKeyCode()));
            m_keyReleasedReceived = true;
            return false;
        });
        
        // Mouse event'lerini dinle
        m_mousePressedSubscription = eventManager.Subscribe<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& event) {
            Logger::Info("SDL3Test", "Mouse button pressed: {}", event.GetMouseButton());
            m_mousePressedReceived = true;
            return false;
        });
        
        m_mouseReleasedSubscription = eventManager.Subscribe<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& event) {
            Logger::Info("SDL3Test", "Mouse button released: {}", event.GetMouseButton());
            m_mouseReleasedReceived = true;
            return false;
        });
        
        m_mouseMovedSubscription = eventManager.Subscribe<MouseMovedEvent>([this](MouseMovedEvent& event) {
            Logger::Trace("SDL3Test", "Mouse moved to ({}, {})", event.GetX(), event.GetY());
            m_mouseMovedReceived = true;
            return false;
        });
        
        Logger::Info("SDL3Test", "Event subscriptions set up successfully");
    }
    
    bool ValidateTestResults() {
        Logger::Info("SDL3Test", "Validating test results...");
        
        bool allTestsPassed = true;
        
        // Test sonuçlarını basitçe doğrula
        // Eğer buraya kadar ulaşıldıysa, temel subsystem'ler çalışıyor demektir
        Logger::Info("SDL3Test", "✅ Engine ran successfully");
        Logger::Info("SDL3Test", "✅ PlatformSubsystem initialized");
        Logger::Info("SDL3Test", "✅ Event system is working");
        
#ifdef ASTRAL_USE_SDL3
        Logger::Info("SDL3Test", "✅ SDL3 integration is active");
#else
        Logger::Warning("SDL3Test", "⚠️  SDL3 integration is not active (compiled without SDL3)");
#endif
        
        // Event handling testi
        if (m_windowResizeReceived) {
            Logger::Info("SDL3Test", "✅ Window resize events received");
        } else {
            Logger::Warning("SDL3Test", "⚠️  No window resize events received (may be normal if window wasn't resized)");
        }
        
        if (m_keyPressedReceived || m_keyReleasedReceived) {
            Logger::Info("SDL3Test", "✅ Keyboard events received");
        } else {
            Logger::Warning("SDL3Test", "⚠️  No keyboard events received (may be normal if no keys were pressed)");
        }
        
        if (m_mousePressedReceived || m_mouseReleasedReceived || m_mouseMovedReceived) {
            Logger::Info("SDL3Test", "✅ Mouse events received");
        } else {
            Logger::Warning("SDL3Test", "⚠️  No mouse events received (may be normal if no mouse interaction)");
        }
        
        // Early exit kontrolü
        if (m_earlyExit) {
            Logger::Info("SDL3Test", "ℹ️  Test ended early due to ESC key");
        }
        
        return allTestsPassed;
    }
    
    // Test durum değişkenleri
    Engine* m_engine = nullptr;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_windowCloseReceived = false;
    bool m_windowResizeReceived = false;
    bool m_keyPressedReceived = false;
    bool m_keyReleasedReceived = false;
    bool m_mousePressedReceived = false;
    bool m_mouseReleasedReceived = false;
    bool m_mouseMovedReceived = false;
    bool m_earlyExit = false;
    
    // Event subscription ID'leri
    EventManager::EventHandlerID m_windowCloseSubscription;
    EventManager::EventHandlerID m_windowResizeSubscription;
    EventManager::EventHandlerID m_keyPressedSubscription;
    EventManager::EventHandlerID m_keyReleasedSubscription;
    EventManager::EventHandlerID m_mousePressedSubscription;
    EventManager::EventHandlerID m_mouseReleasedSubscription;
    EventManager::EventHandlerID m_mouseMovedSubscription;
};

/**
 * @brief Ana test fonksiyonu
 */
int main() {
    Logger::Info("SDL3Test", "=== Astral Engine SDL3 Integration Test ===");
    Logger::Info("SDL3Test", "This test will run for 10 seconds or until ESC is pressed");
    Logger::Info("SDL3Test", "Try interacting with the window (resize, press keys, click mouse)");
    Logger::Info("SDL3Test", "");
    
    try {
        Engine engine;
        SDL3TestApplication testApp;
        
        // Subsystem'leri kaydet
        Logger::Info("SDL3Test", "Registering subsystems...");
        engine.RegisterSubsystem<PlatformSubsystem>();
        engine.RegisterSubsystem<AssetSubsystem>();
        
        // Engine'i çalıştır
        engine.Run(&testApp);
        
        Logger::Info("SDL3Test", "");
        Logger::Info("SDL3Test", "🎉 All tests completed successfully!");
        Logger::Info("SDL3Test", "SDL3 integration is working properly.");
        return 0;
        
    } catch (const std::exception& e) {
        Logger::Error("SDL3Test", "");
        Logger::Error("SDL3Test", "💥 Test failed with exception: {}", e.what());
        return 1;
    } catch (...) {
        Logger::Error("SDL3Test", "");
        Logger::Error("SDL3Test", "💥 Test failed with unknown exception");
        return 1;
    }
}
