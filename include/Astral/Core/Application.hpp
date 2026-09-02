#pragma once

#include <string>
#include <memory>

namespace Astral {

class Window;
class VulkanContext;

/// Uygulamanin temel yasam dongusunu temsil eden cekirdek sinif.
/// Renderer, Window ve ileride diger alt sistemler bu sinif uzerinden koordine edilir.
class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Ana donguyu baslatir. maxFrames > 0 ise belirtilen kare kadar calisip otomatik cikar.
    void Run(int maxFrames = -1);

    /// Motorun adi ve versiyonu.
    static std::string GetName();
    static std::string GetVersion();

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<VulkanContext> m_VulkanContext;
    bool m_Running = false;
};

} // namespace Astral