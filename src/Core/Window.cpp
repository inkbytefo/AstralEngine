#include "Astral/Core/Window.hpp"
#include <iostream>
#include <stdexcept>

namespace Astral {

static void GLFWErrorCallback(int error, const char* description) {
    std::cerr << "[GLFW Error (" << error << ")]: " << description << "\n";
}

Window::Window(int width, int height, const std::string& title)
    : m_Width(width), m_Height(height), m_Title(title) {
    Init();
}

Window::~Window() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SetTitle(const std::string& title) {
    m_Title = title;
    if (m_Window) {
        glfwSetWindowTitle(m_Window, m_Title.c_str());
    }
}

void Window::Init() {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit()) {
        throw std::runtime_error("GLFW baslatilamadi!");
    }

    // Vulkan icin OpenGL baglamini devre disi birak
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        glfwTerminate();
        throw std::runtime_error("GLFW penceresi olusturulamadi!");
    }

    glfwSetWindowUserPointer(m_Window, this);

    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self) {
            self->m_Width = width;
            self->m_Height = height;
        }
    });

    glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int, int action, int) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_InputSystem.OnKey(key, action);
        }
    });
    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_InputSystem.OnMouseButton(button, action);
        }
    });
    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_InputSystem.OnCursorPosition(x, y);
        }
    });
    glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_InputSystem.OnScroll(xOffset, yOffset);
        }
    });
    glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focused) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_InputSystem.OnFocusChanged(focused == GLFW_TRUE);
        }
    });

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);
    m_InputSystem.OnCursorPosition(mouseX, mouseY);

    std::cout << "[Astral::Window] Pencere basariyla olusturuldu (" << m_Width << "x" << m_Height << "): " << m_Title << "\n";
}

std::vector<const char*> Window::GetRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

} // namespace Astral
