#include "Astral/Core/Input/ActionMap.hpp"
#include "Astral/Core/InputSystem.hpp"
#include <algorithm>
#include <cmath>

namespace Astral {

ActionMap::ActionMap() {
    m_Contexts["Default"] = ContextData{};
    m_ContextStack.emplace_back("Default");
}

ActionMap::ContextData& ActionMap::GetActiveContextData() {
    const std::string& current = m_ContextStack.empty() ? "Default" : m_ContextStack.back();
    return m_Contexts[current];
}

const ActionMap::ContextData& ActionMap::GetActiveContextData() const {
    const std::string& current = m_ContextStack.empty() ? "Default" : m_ContextStack.back();
    auto it = m_Contexts.find(current);
    if (it != m_Contexts.end()) {
        return it->second;
    }
    static const ContextData s_Empty{};
    return s_Empty;
}

InputAction* ActionMap::FindActionMut(std::string_view name) {
    auto& activeCtx = GetActiveContextData();
    auto it = activeCtx.actions.find(std::string(name));
    if (it != activeCtx.actions.end()) {
        return &it->second;
    }

    // Default context fallback
    if (!m_ContextStack.empty() && m_ContextStack.back() != "Default") {
        auto defIt = m_Contexts.find("Default");
        if (defIt != m_Contexts.end()) {
            auto actIt = defIt->second.actions.find(std::string(name));
            if (actIt != defIt->second.actions.end()) {
                return &actIt->second;
            }
        }
    }

    return nullptr;
}

const InputAction* ActionMap::FindAction(std::string_view name) const {
    const auto& activeCtx = GetActiveContextData();
    auto it = activeCtx.actions.find(std::string(name));
    if (it != activeCtx.actions.end()) {
        return &it->second;
    }

    // Default context fallback
    if (!m_ContextStack.empty() && m_ContextStack.back() != "Default") {
        auto defIt = m_Contexts.find("Default");
        if (defIt != m_Contexts.end()) {
            auto actIt = defIt->second.actions.find(std::string(name));
            if (actIt != defIt->second.actions.end()) {
                return &actIt->second;
            }
        }
    }

    return nullptr;
}

void ActionMap::MapAction(std::string_view name, int key) {
    auto& actions = GetActiveContextData().actions;
    std::string n(name);
    auto& action = actions[n];
    action.name = n;
    action.type = ActionType::Digital;
    action.keyBindings.push_back(KeyBinding{key, 1.0f});
}

void ActionMap::MapMouseButtonAction(std::string_view name, int mouseButton) {
    auto& actions = GetActiveContextData().actions;
    std::string n(name);
    auto& action = actions[n];
    action.name = n;
    action.type = ActionType::Digital;
    action.mouseButtonBindings.push_back(MouseButtonBinding{mouseButton});
}

void ActionMap::MapAxis1D(std::string_view name, int positiveKey, int negativeKey) {
    auto& actions = GetActiveContextData().actions;
    std::string n(name);
    auto& action = actions[n];
    action.name = n;
    action.type = ActionType::Axis1D;
    action.keyBindings.push_back(KeyBinding{positiveKey, +1.0f});
    action.keyBindings.push_back(KeyBinding{negativeKey, -1.0f});
}

void ActionMap::MapAxis2D(std::string_view name, int upKey, int downKey, int leftKey, int rightKey) {
    auto& actions = GetActiveContextData().actions;
    std::string n(name);
    auto& action = actions[n];
    action.name = n;
    action.type = ActionType::Axis2D;
    action.composite2DBindings.push_back(Axis2DCompositeBinding{upKey, downKey, leftKey, rightKey});
}

void ActionMap::MapMouseAxis2D(std::string_view name, float sensitivity, bool invertY) {
    auto& actions = GetActiveContextData().actions;
    std::string n(name);
    auto& action = actions[n];
    action.name = n;
    action.type = ActionType::Axis2D;
    action.mouseAxisBindings.push_back(MouseAxis2DBinding{sensitivity, sensitivity, invertY});
}

bool ActionMap::RebindKey(std::string_view actionName, int oldKey, int newKey) {
    auto* action = FindActionMut(actionName);
    if (!action) return false;

    bool rebound = false;
    for (auto& kb : action->keyBindings) {
        if (kb.key == oldKey) {
            kb.key = newKey;
            rebound = true;
        }
    }

    for (auto& c : action->composite2DBindings) {
        if (c.upKey == oldKey)    { c.upKey = newKey;    rebound = true; }
        if (c.downKey == oldKey)  { c.downKey = newKey;  rebound = true; }
        if (c.leftKey == oldKey)  { c.leftKey = newKey;  rebound = true; }
        if (c.rightKey == oldKey) { c.rightKey = newKey; rebound = true; }
    }

    return rebound;
}

void ActionMap::ClearBindings(std::string_view actionName) {
    auto* action = FindActionMut(actionName);
    if (!action) return;

    action->keyBindings.clear();
    action->mouseButtonBindings.clear();
    action->composite2DBindings.clear();
    action->mouseAxisBindings.clear();
    action->active = false;
    action->justPressed = false;
    action->justReleased = false;
    action->axis1D = 0.0f;
    action->axis2D = glm::vec2(0.0f);
}

void ActionMap::Update(const InputSystem& rawInput) {
    for (auto& [ctxName, ctxData] : m_Contexts) {
        for (auto& [actName, act] : ctxData.actions) {
            act.previousActive = act.active;

            switch (act.type) {
                case ActionType::Digital: {
                    bool pressed = false;
                    for (const auto& kb : act.keyBindings) {
                        if (rawInput.IsKeyPressed(kb.key)) {
                            pressed = true;
                            break;
                        }
                    }
                    if (!pressed) {
                        for (const auto& mb : act.mouseButtonBindings) {
                            if (rawInput.IsMouseButtonPressed(mb.button)) {
                                pressed = true;
                                break;
                            }
                        }
                    }
                    act.active = pressed;
                    act.justPressed = act.active && !act.previousActive;
                    act.justReleased = !act.active && act.previousActive;
                    act.axis1D = act.active ? 1.0f : 0.0f;
                    act.axis2D = glm::vec2(act.axis1D, 0.0f);
                    break;
                }

                case ActionType::Axis1D: {
                    float sum = 0.0f;
                    for (const auto& kb : act.keyBindings) {
                        if (rawInput.IsKeyPressed(kb.key)) {
                            sum += kb.scale;
                        }
                    }
                    act.axis1D = std::clamp(sum, -1.0f, 1.0f);
                    act.active = std::abs(act.axis1D) > 0.001f;
                    act.justPressed = act.active && !act.previousActive;
                    act.justReleased = !act.active && act.previousActive;
                    act.axis2D = glm::vec2(act.axis1D, 0.0f);
                    break;
                }

                case ActionType::Axis2D: {
                    glm::vec2 v{0.0f};

                    // Composite WASD / yön tuşları
                    for (const auto& c : act.composite2DBindings) {
                        if (rawInput.IsKeyPressed(c.upKey))    v.y += 1.0f;
                        if (rawInput.IsKeyPressed(c.downKey))  v.y -= 1.0f;
                        if (rawInput.IsKeyPressed(c.rightKey)) v.x += 1.0f;
                        if (rawInput.IsKeyPressed(c.leftKey))  v.x -= 1.0f;
                    }

                    // Fare deltası
                    const glm::dvec2 mDelta = rawInput.GetMouseDelta();
                    for (const auto& mb : act.mouseAxisBindings) {
                        v.x += static_cast<float>(mDelta.x) * mb.sensitivityX;
                        v.y += static_cast<float>(mDelta.y) * mb.sensitivityY * (mb.invertY ? -1.0f : 1.0f);
                    }

                    act.axis2D = v;
                    act.active = glm::length(v) > 0.001f;
                    act.justPressed = act.active && !act.previousActive;
                    act.justReleased = !act.active && act.previousActive;
                    act.axis1D = v.x;
                    break;
                }
            }
        }
    }
}

bool ActionMap::IsActionActive(std::string_view name) const {
    const auto* act = FindAction(name);
    return act ? act->active : false;
}

bool ActionMap::IsActionJustPressed(std::string_view name) const {
    const auto* act = FindAction(name);
    return act ? act->justPressed : false;
}

bool ActionMap::IsActionJustReleased(std::string_view name) const {
    const auto* act = FindAction(name);
    return act ? act->justReleased : false;
}

float ActionMap::GetAxis1D(std::string_view name) const {
    const auto* act = FindAction(name);
    return act ? act->axis1D : 0.0f;
}

glm::vec2 ActionMap::GetAxis2D(std::string_view name) const {
    const auto* act = FindAction(name);
    return act ? act->axis2D : glm::vec2(0.0f);
}

void ActionMap::PushContext(std::string_view contextName) {
    std::string ctx(contextName);
    if (m_Contexts.find(ctx) == m_Contexts.end()) {
        m_Contexts[ctx] = ContextData{};
    }
    m_ContextStack.push_back(std::move(ctx));
}

void ActionMap::PopContext() {
    if (m_ContextStack.size() > 1) {
        m_ContextStack.pop_back();
    }
}

std::string_view ActionMap::GetCurrentContext() const {
    return m_ContextStack.empty() ? "Default" : std::string_view(m_ContextStack.back());
}

} // namespace Astral
