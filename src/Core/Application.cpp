#include "Astral/Core/Application.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Renderer/VulkanContext.hpp"

#include <iostream>

namespace Astral {

Application::Application() {
    std::cout << "[Astral::Application] " << GetName() << " v" << GetVersion() << " olusturuldu.\n";
}

Application::~Application() {
    if (m_VulkanContext) {
        m_VulkanContext->WaitIdle();
        m_VulkanContext.reset();
    }
    m_Window.reset();
    std::cout << "[Astral::Application] " << GetName() << " basariyla kapatildi.\n";
}

void Application::Run(int maxFrames) {
    std::cout << "[Astral::Application] Baslatiliyor...\n";

    try {
        // 1. Pencereyi baslat (1280x720)
        m_Window = std::make_unique<Window>(1280, 720, GetName() + " - Vulkan 1.4 SDF Sandbox");

        // 2. Vulkan 1.4 Context ve Validation Layer'i baslat
        m_VulkanContext = std::make_unique<VulkanContext>(*m_Window, true);

        m_Running = true;
        if (maxFrames > 0) {
            std::cout << "[Astral::Application] Test Modu: " << maxFrames << " kare calisip otomatik kapanacak...\n";
        } else {
            std::cout << "[Astral::Application] Ana olay dongusune giriliyor (Cikmak icin pencereyi kapatin)...\n";
        }

        int frameCount = 0;
        // 3. Ana olay dongusu
        while (m_Running && !m_Window->ShouldClose()) {
            m_Window->PollEvents();

            frameCount++;
            if (maxFrames > 0 && frameCount >= maxFrames) {
                break;
            }
        }

        m_Running = false;
        std::cout << "[Astral::Application] Ana olay dongusu sonlandi (Toplam " << frameCount << " kare).\n";

    } catch (const std::exception& e) {
        std::cerr << "[Astral::Application Kritik Hata]: " << e.what() << "\n";
    }
}

std::string Application::GetName() {
    return "AstralEngine";
}

std::string Application::GetVersion() {
    return "1.0.0";
}

} // namespace Astral