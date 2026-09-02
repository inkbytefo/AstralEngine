#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

namespace Astral {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const { return glfwWindowShouldClose(m_Window); }
    void PollEvents() { glfwPollEvents(); }

    GLFWwindow* GetNativeWindow() const { return m_Window; }
    void GetFramebufferSize(int& width, int& height) const {
        glfwGetFramebufferSize(m_Window, &width, &height);
    }

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    static std::vector<const char*> GetRequiredExtensions();

private:
    GLFWwindow* m_Window = nullptr;
    int m_Width;
    int m_Height;
    std::string m_Title;

    void Init();
};

} // namespace Astral
