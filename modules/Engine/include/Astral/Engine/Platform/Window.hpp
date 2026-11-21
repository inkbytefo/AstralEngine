#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct GLFWwindow;

namespace Astral {

class Input;

// Desktop window (GLFW). Feeds keyboard/mouse events into an Input object.
class Window {
public:
    Window(const std::string& title, uint32_t width, uint32_t height, Input& input);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void RequestClose();
    void PollEvents();
    void WaitEvents();
    glm::uvec2 FramebufferSize() const;
    void SetTitle(const std::string& title);
    void SetCursorCaptured(bool captured); // hides and locks the cursor (FPS-style mouse look)

    GLFWwindow* Native() const { return m_Window; }

    // Vulkan instance extensions the window system needs.
    static std::vector<std::string> RequiredVulkanExtensions();

private:
    GLFWwindow* m_Window = nullptr;
    Input& m_Input;
};

} // namespace Astral
