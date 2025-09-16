#include "RenderSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Platform/PlatformSubsystem.h"
#include "../ECS/ECSSubsystem.h"

namespace AstralEngine {

RenderSubsystem::RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem created");
}

RenderSubsystem::~RenderSubsystem() {
    Logger::Debug("RenderSubsystem", "RenderSubsystem destroyed");
}

void RenderSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("RenderSubsystem", "Initializing render subsystem...");
    
    // Platform subsystem'den window'u al
    auto platformSubsystem = m_owner->GetSubsystem<PlatformSubsystem>();
    if (!platformSubsystem) {
        Logger::Error("RenderSubsystem", "PlatformSubsystem not found!");
        return;
    }
    
    m_window = platformSubsystem->GetWindow();
    if (!m_window) {
        Logger::Error("RenderSubsystem", "Window not found!");
        return;
    }
    
    // ECS subsystem'e eriş
    m_ecsSubsystem = m_owner->GetSubsystem<ECSSubsystem>();
    if (!m_ecsSubsystem) {
        Logger::Error("RenderSubsystem", "ECSSubsystem not found!");
        return;
    }
    
    // Graphics device oluştur
    m_graphicsDevice = std::make_unique<GraphicsDevice>();
    if (!m_graphicsDevice->Initialize(m_window, m_owner)) {
        Logger::Error("RenderSubsystem", "Failed to initialize GraphicsDevice!");
        m_graphicsDevice.reset();
        return;
    }
    
    // Kamera oluştur ve yapılandır
    m_camera = std::make_unique<Camera>();
    Camera::Config cameraConfig;
    cameraConfig.position = glm::vec3(0.0f, 0.0f, 2.0f);
    cameraConfig.target = glm::vec3(0.0f, 0.0f, 0.0f);
    cameraConfig.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cameraConfig.fov = 45.0f;
    // Dinamik olarak pencere boyutundan aspect ratio hesapla
    cameraConfig.aspectRatio = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
    cameraConfig.nearPlane = 0.1f;
    cameraConfig.farPlane = 100.0f;
    
    m_camera->Initialize(cameraConfig);
    
    Logger::Info("RenderSubsystem", "Render subsystem initialized successfully");
}

void RenderSubsystem::OnUpdate(float deltaTime) {
    if (!m_graphicsDevice || !m_graphicsDevice->IsInitialized()) {
        Logger::Error("RenderSubsystem", "Cannot update - GraphicsDevice not initialized");
        return;
    }

    // Kamera aspect ratio değerini dinamik olarak güncelle
    if (m_camera && m_window) {
        float currentAspectRatio = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
        if (currentAspectRatio != m_camera->GetAspectRatio()) {
            m_camera->SetAspectRatio(currentAspectRatio);
        }
    }

    // ECS'den render verilerini al
    if (m_ecsSubsystem) {
        auto renderPacket = m_ecsSubsystem->GetRenderData();
        
        // GraphicsDevice frame'i başlatsın (fence bekleme ve image alma)
        if (m_graphicsDevice->BeginFrame()) {
            // VulkanRenderer sadece komutları kaydetsin
            auto vulkanRenderer = m_graphicsDevice->GetVulkanRenderer();
            if (vulkanRenderer) {
                // Kamera referansını güncelle
                vulkanRenderer->SetCamera(m_camera.get());
                // GraphicsDevice'den güncel frame indeksini al
                uint32_t currentFrameIndex = m_graphicsDevice->GetCurrentFrameIndex();
                // Render verilerini VulkanRenderer'a gönder
                vulkanRenderer->RecordCommands(currentFrameIndex, renderPacket); // Sadece komut kaydı, submit/present yapma
            }
            
            // GraphicsDevice frame'i bitirsin (submit ve present)
            m_graphicsDevice->EndFrame();
        }
        
        Logger::Debug("RenderSubsystem", "Rendering {} items from ECS with deltaTime: {}", 
                     renderPacket.renderItems.size(), deltaTime);
    } else {
        Logger::Error("RenderSubsystem", "ECSSubsystem not available for render data");
    }
}

void RenderSubsystem::OnShutdown() {
    Logger::Info("RenderSubsystem", "Shutting down render subsystem...");
    
    if (m_camera) {
        m_camera->Shutdown();
        m_camera.reset();
    }
    
    if (m_graphicsDevice) {
        m_graphicsDevice->Shutdown();
        m_graphicsDevice.reset();
    }
    
    m_window = nullptr;
    Logger::Info("RenderSubsystem", "Render subsystem shutdown complete");
}

void RenderSubsystem::BeginFrame() {
    if (!m_graphicsDevice) {
        Logger::Error("RenderSubsystem", "Cannot begin frame - GraphicsDevice not initialized");
        return;
    }
    
    Logger::Debug("RenderSubsystem", "Beginning frame");
    
    // Frame başlangıç işlemleri
    // - Clear color ayarla
    // - Viewport ayarla
    // - Command buffer begin
    // - Render pass begin
    
    // Şimdilik sadece loglama yapıyoruz, gerçek implementasyon VulkanRenderer'da
}

void RenderSubsystem::EndFrame() {
    if (!m_graphicsDevice) {
        Logger::Error("RenderSubsystem", "Cannot end frame - GraphicsDevice not initialized");
        return;
    }
    
    Logger::Debug("RenderSubsystem", "Ending frame");
    
    // Frame bitirme işlemleri
    // - Render pass end
    // - Command buffer submit
    // - Present
    
    // Şimdilik sadece loglama yapıyoruz, gerçek implementasyon VulkanRenderer'da
}

} // namespace AstralEngine
