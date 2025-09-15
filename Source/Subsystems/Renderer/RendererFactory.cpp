#include "RendererFactory.h"
#include "VulkanRenderer.h"
#include "Core/Logger.h"
#include <vector>

namespace AstralEngine {

std::unique_ptr<IRenderer> RendererFactory::CreateRenderer(RendererAPI api) {
    switch (api) {
        case RendererAPI::Vulkan:
            Logger::Info("RendererFactory", "Creating Vulkan renderer...");
            return std::make_unique<VulkanRenderer>();
            
        case RendererAPI::DirectX12:
            Logger::Error("RendererFactory", "DirectX 12 renderer not implemented yet");
            return nullptr;
            
        case RendererAPI::Metal:
            Logger::Error("RendererFactory", "Metal renderer not implemented yet");
            return nullptr;
            
        case RendererAPI::OpenGL:
            Logger::Error("RendererFactory", "OpenGL renderer not implemented yet");
            return nullptr;
            
        default:
            Logger::Error("RendererFactory", "Unknown renderer API requested");
            return nullptr;
    }
}

std::unique_ptr<IRenderer> RendererFactory::CreateDefaultRenderer() {
    // Şimdilik varsayılan olarak Vulkan renderer'ı oluştur
    Logger::Info("RendererFactory", "Creating default renderer (Vulkan)...");
    return CreateRenderer(RendererAPI::Vulkan);
}

std::vector<RendererAPI> RendererFactory::GetSupportedAPIs() {
    return {
        RendererAPI::Vulkan
        // RendererAPI::DirectX12,  // Gelecekte eklenecek
        // RendererAPI::Metal,      // Gelecekte eklenecek
        // RendererAPI::OpenGL      // Gelecekte eklenecek
    };
}

bool RendererFactory::IsAPISupported(RendererAPI api) {
    auto supportedAPIs = GetSupportedAPIs();
    return std::find(supportedAPIs.begin(), supportedAPIs.end(), api) != supportedAPIs.end();
}

} // namespace AstralEngine
