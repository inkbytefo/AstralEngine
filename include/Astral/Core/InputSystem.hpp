#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>

namespace Astral {

/// GLFW callback durumunu kare bazli, sorgulanabilir bir snapshot'a donusturur.
class InputSystem {
public:
    void BeginFrame();

    [[nodiscard]] bool IsKeyPressed(int key) const;
    [[nodiscard]] bool IsKeyJustPressed(int key) const;
    [[nodiscard]] bool IsKeyJustReleased(int key) const;

    [[nodiscard]] bool IsMouseButtonPressed(int button) const;
    [[nodiscard]] bool IsMouseButtonJustPressed(int button) const;
    [[nodiscard]] bool IsMouseButtonJustReleased(int button) const;

    [[nodiscard]] glm::dvec2 GetMousePosition() const { return m_MousePosition; }
    [[nodiscard]] glm::dvec2 GetMouseDelta() const { return m_MouseDelta; }
    [[nodiscard]] glm::dvec2 GetScrollDelta() const { return m_ScrollDelta; }

private:
    friend class Window;

    void OnKey(int key, int action);
    void OnMouseButton(int button, int action);
    void OnCursorPosition(double x, double y);
    void OnScroll(double xOffset, double yOffset);
    void OnFocusChanged(bool focused);

    std::array<bool, GLFW_KEY_LAST + 1> m_LiveKeys{};
    std::array<bool, GLFW_KEY_LAST + 1> m_Keys{};
    std::array<bool, GLFW_KEY_LAST + 1> m_PreviousKeys{};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> m_LiveMouseButtons{};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> m_MouseButtons{};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> m_PreviousMouseButtons{};

    glm::dvec2 m_LiveMousePosition{0.0};
    glm::dvec2 m_MousePosition{0.0};
    glm::dvec2 m_PreviousMousePosition{0.0};
    glm::dvec2 m_MouseDelta{0.0};
    glm::dvec2 m_PendingScrollDelta{0.0};
    glm::dvec2 m_ScrollDelta{0.0};
    bool m_HasMousePosition = false;
};

} // namespace Astral
