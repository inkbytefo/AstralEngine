// filepath: AstralEngine/src/Core/Application.cpp
#include "Astral/Core/Application.hpp"

#include <iostream>

namespace Astral {

Application::Application() {
    std::cout << "[Astral] Application olusturuldu.\n";
}

Application::~Application() {
    std::cout << "[Astral] Application yok edildi.\n";
}

void Application::Run() {
    std::cout << "[Astral] " << GetName() << " v" << GetVersion()
              << " calistiriliyor...\n";

    // TODO (ileride): Ana dongu (event loop) burada yer alacak.
    // - Pencere olusturma (GLFW/Win32)
    // - Vulkan cihaz baslatma
    // - ECS dunyasi (EnTT) kurulumu
    // - Input / Renderer / SceneManager entegrasyonu
}

std::string Application::GetName() {
    return "AstralEngine";
}

std::string Application::GetVersion() {
    return "1.0.0";
}

} // namespace Astral