#include "Astral/Engine/Platform/Input.hpp"

#include <algorithm>

namespace Astral {

void Input::BeginFrame() {
    m_Prev = m_Down;
    m_MousePrev = m_Mouse;
    m_MousePrevPos = m_MousePos;
    m_Scroll = 0.0f;
}

void Input::Reset() {
    m_Down.fill(false);
    m_Prev.fill(false);
    m_Mouse.fill(false);
    m_MousePrev.fill(false);
    m_Scroll = 0.0f;
}

bool Input::IsActionDown(const std::string& action) const {
    auto it = m_Actions.find(action);
    if (it == m_Actions.end()) return false;
    return std::any_of(it->second.begin(), it->second.end(), [&](const ActionBinding& b) { return b.Down(*this); });
}

bool Input::WasActionPressed(const std::string& action) const {
    auto it = m_Actions.find(action);
    if (it == m_Actions.end()) return false;
    return std::any_of(it->second.begin(), it->second.end(), [&](const ActionBinding& b) { return b.Pressed(*this); });
}

float Input::GetAxis(const std::string& axis) const {
    auto it = m_Axes.find(axis);
    if (it == m_Axes.end()) return 0.0f;
    float value = 0.0f;
    for (const auto& b : it->second) value += (IsDown(b.positive) ? 1.0f : 0.0f) - (IsDown(b.negative) ? 1.0f : 0.0f);
    return std::clamp(value, -1.0f, 1.0f);
}

} // namespace Astral
