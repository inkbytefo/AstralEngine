#include "Astral/Core/InputSystem.hpp"

#include <algorithm>

namespace Astral {

namespace {

bool IsValidKey(int key) {
    return key >= 0 && key <= GLFW_KEY_LAST;
}

bool IsValidMouseButton(int button) {
    return button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST;
}

} // namespace

void InputSystem::BeginFrame() {
    m_PreviousKeys = m_Keys;
    m_Keys = m_LiveKeys;
    m_PreviousMouseButtons = m_MouseButtons;
    m_MouseButtons = m_LiveMouseButtons;

    m_PreviousMousePosition = m_MousePosition;
    m_MousePosition = m_LiveMousePosition;
    m_MouseDelta = m_MousePosition - m_PreviousMousePosition;

    m_ScrollDelta = m_PendingScrollDelta;
    m_PendingScrollDelta = glm::dvec2(0.0);
}

bool InputSystem::IsKeyPressed(int key) const {
    return IsValidKey(key) && m_Keys[static_cast<size_t>(key)];
}

bool InputSystem::IsKeyJustPressed(int key) const {
    return IsValidKey(key) && m_Keys[static_cast<size_t>(key)] && !m_PreviousKeys[static_cast<size_t>(key)];
}

bool InputSystem::IsKeyJustReleased(int key) const {
    return IsValidKey(key) && !m_Keys[static_cast<size_t>(key)] && m_PreviousKeys[static_cast<size_t>(key)];
}

bool InputSystem::IsMouseButtonPressed(int button) const {
    return IsValidMouseButton(button) && m_MouseButtons[static_cast<size_t>(button)];
}

bool InputSystem::IsMouseButtonJustPressed(int button) const {
    return IsValidMouseButton(button) && m_MouseButtons[static_cast<size_t>(button)] &&
           !m_PreviousMouseButtons[static_cast<size_t>(button)];
}

bool InputSystem::IsMouseButtonJustReleased(int button) const {
    return IsValidMouseButton(button) && !m_MouseButtons[static_cast<size_t>(button)] &&
           m_PreviousMouseButtons[static_cast<size_t>(button)];
}

void InputSystem::OnKey(int key, int action) {
    if (IsValidKey(key)) {
        m_LiveKeys[static_cast<size_t>(key)] = action != GLFW_RELEASE;
    }
}

void InputSystem::OnMouseButton(int button, int action) {
    if (IsValidMouseButton(button)) {
        m_LiveMouseButtons[static_cast<size_t>(button)] = action != GLFW_RELEASE;
    }
}

void InputSystem::OnCursorPosition(double x, double y) {
    m_LiveMousePosition = glm::dvec2(x, y);
    if (!m_HasMousePosition) {
        m_MousePosition = m_LiveMousePosition;
        m_PreviousMousePosition = m_LiveMousePosition;
        m_HasMousePosition = true;
    }
}

void InputSystem::OnScroll(double xOffset, double yOffset) {
    m_PendingScrollDelta += glm::dvec2(xOffset, yOffset);
}

void InputSystem::OnFocusChanged(bool focused) {
    if (!focused) {
        m_LiveKeys.fill(false);
        m_LiveMouseButtons.fill(false);
    }
}

} // namespace Astral
