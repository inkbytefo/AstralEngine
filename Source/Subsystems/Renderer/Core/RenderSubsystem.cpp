#include "RenderSubsystem.h"
#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include <stdexcept>

namespace AstralEngine {

RenderSubsystem::RenderSubsystem() = default;
RenderSubsystem::~RenderSubsystem() = default;

void RenderSubsystem::OnInitialize(Engine* owner) {
    m_engine = owner;
    Logger::Info("RenderSubsystem", "Initializing RenderSubsystem...");

    // 1. Get Platform Subsystem for Window
    auto* platform = owner->GetSubsystem<PlatformSubsystem>();
    if (!platform) {
        throw std::runtime_error("RenderSubsystem requires PlatformSubsystem!");
    }

    Window* window = platform->GetWindow();
    if (!window) {
        throw std::runtime_error("PlatformSubsystem has no active window!");
    }

    // 2. Create RHI Device (Vulkan)
    Logger::Info("RenderSubsystem", "Creating Vulkan RHI Device...");
    m_device = CreateVulkanDevice(window);

    if (!m_device) {
        throw std::runtime_error("Failed to create RHI Device!");
    }

    // 3. Initialize Device
    if (!m_device->Initialize()) {
        throw std::runtime_error("Failed to initialize RHI Device!");
    }
    
    Logger::Info("RenderSubsystem", "RenderSubsystem initialized successfully.");
}

void RenderSubsystem::OnUpdate(float /*deltaTime*/) {
    if (!m_device) return;

    // Begin Frame
    m_device->BeginFrame();

    // Get Command List
    auto cmdList = m_device->CreateCommandList();
    cmdList->Begin();

    // Define Render Area (Full Screen)
    IRHITexture* backBuffer = m_device->GetCurrentBackBuffer();
    if (backBuffer) {
        RHIRect2D renderArea;
        renderArea.offset = {0, 0};
        renderArea.extent = {backBuffer->GetWidth(), backBuffer->GetHeight()};
        
        // Start Rendering
        // Note: Current Vulkan backend implementation uses internal swapchain state
        // ignoring the attachment arguments for the main pass.
        cmdList->BeginRendering({}, nullptr, renderArea);
        
        // Dispatch render callback if set
        if (m_renderCallback) {
            m_renderCallback(cmdList.get());
        }
        
        cmdList->EndRendering();
    }

    cmdList->End();

    // Submit
    m_device->SubmitCommandList(cmdList.get());

    // Present
    m_device->Present();
}

void RenderSubsystem::OnShutdown() {
    Logger::Info("RenderSubsystem", "Shutting down RenderSubsystem...");
    if (m_device) {
        m_device->Shutdown();
        m_device.reset();
    }
}

} // namespace AstralEngine
