#include "Astral/Engine/Platform/Window.hpp"
#include "Astral/Engine/Platform/Input.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Astral {

namespace {
int g_GlfwUsers = 0;

void EnsureGlfw() {
    if (g_GlfwUsers++ == 0 && !glfwInit()) throw std::runtime_error("GLFW initialisation failed");
}

void ReleaseGlfw() {
    if (--g_GlfwUsers == 0) glfwTerminate();
}

Input& InputOf(GLFWwindow* w) { return *static_cast<Input*>(glfwGetWindowUserPointer(w)); }
} // namespace

Window::Window(const std::string& title, uint32_t width, uint32_t height, Input& input) : m_Input(input) {
    EnsureGlfw();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(int(width), int(height), title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        ReleaseGlfw();
        throw std::runtime_error("could not create a window");
    }
    glfwSetWindowUserPointer(m_Window, &m_Input);
    glfwSetKeyCallback(m_Window, [](GLFWwindow* w, int key, int, int action, int) {
        if (action != GLFW_REPEAT) InputOf(w).SetKey(key, action == GLFW_PRESS);
    });
    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* w, int button, int action, int) {
        InputOf(w).SetMouseButton(button, action == GLFW_PRESS);
    });
    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        InputOf(w).SetMousePosition(glm::vec2(float(x), float(y)));
    });
    glfwSetScrollCallback(m_Window, [](GLFWwindow* w, double, double y) { InputOf(w).AddScroll(float(y)); });
    glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* w, int focused) {
        if (!focused) InputOf(w).Reset(); // avoid keys stuck down after alt-tab
    });
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
    ReleaseGlfw();
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Window); }
void Window::RequestClose() { glfwSetWindowShouldClose(m_Window, GLFW_TRUE); }
void Window::PollEvents() { glfwPollEvents(); }
void Window::WaitEvents() { glfwWaitEvents(); }
void Window::SetTitle(const std::string& title) { glfwSetWindowTitle(m_Window, title.c_str()); }

void Window::SetCursorCaptured(bool captured) {
    glfwSetInputMode(m_Window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

glm::uvec2 Window::FramebufferSize() const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return {uint32_t(w), uint32_t(h)};
}

std::vector<std::string> Window::RequiredVulkanExtensions() {
    EnsureGlfw();
    uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    std::vector<std::string> out(names, names + count);
    ReleaseGlfw();
    return out;
}

} // namespace Astral
