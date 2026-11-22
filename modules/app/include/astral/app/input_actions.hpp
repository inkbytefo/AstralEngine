#pragma once
// Named input actions: "move", "fire", "jump" instead of keys. Games and scripts ask for actions; a
// .actions file maps keys and mouse to them, so rebinding never touches game code.
//
//   {"format": "astral.actions", "version": 1, "actions": {
//     "fire": {"type": "button", "keys": ["Space"], "mouse": ["Left"]},
//     "throttle": {"type": "axis", "negative": ["S"], "positive": ["W"]},
//     "move": {"type": "axis2", "x": {"negative": ["A", "Left"], "positive": ["D", "Right"]},
//                               "y": {"negative": ["S", "Down"], "positive": ["W", "Up"]}},
//     "look": {"type": "axis2", "mouse_delta": [0.012, -0.012]}
//   }}
//
// Gamepads are not supported yet (the platform layer has none).

#include <astral/core/json.hpp>
#include <astral/core/math.hpp>
#include <astral/platform/input.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace astral {

class InputActions {
public:
    /// Replaces every action with the document's. False with *error (and nothing changed) if invalid.
    bool load(const Json& document, std::string* error = nullptr);
    /// Reads this frame's state of every action from `input`. Call once per frame.
    void update(const Input& input);

    [[nodiscard]] bool has(std::string_view action) const noexcept;
    /// Held now / became held this frame / stopped being held this frame. Axes count as held when not zero.
    [[nodiscard]] bool down(std::string_view action) const noexcept;
    [[nodiscard]] bool pressed(std::string_view action) const noexcept;
    [[nodiscard]] bool released(std::string_view action) const noexcept;
    /// Buttons: 0 or 1. Axes: -1..1 (axis2: x).
    [[nodiscard]] float value(std::string_view action) const noexcept;
    [[nodiscard]] Vec2 axis(std::string_view action) const noexcept;
    [[nodiscard]] std::vector<std::string> names() const;

private:
    struct Axis {
        std::vector<Key> negative, positive;
    };
    struct Action {
        std::string name;
        enum class Type { Button, Axis, Axis2 } type = Type::Button;
        std::vector<Key> keys;
        std::vector<MouseButton> mouse;
        Axis x, y;
        Vec2 mouse_delta_scale{};
        bool use_mouse_delta = false;
        // State of this frame.
        Vec2 current{};
        bool is_down = false, was_down = false;
    };
    [[nodiscard]] const Action* find(std::string_view name) const noexcept;
    std::vector<Action> actions_;
};

/// Key by its name in .actions files ("A", "Space", "Left", "F5", "LeftShift"...); Key::Unknown if unknown.
[[nodiscard]] Key key_from_name(std::string_view name) noexcept;

} // namespace astral
