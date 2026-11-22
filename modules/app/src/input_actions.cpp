#include <astral/app/input_actions.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace astral {

namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(Key::Count)> key_names{
    "Unknown", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9",
    "Space", "Enter", "Escape", "Tab", "Backspace",
    "Left", "Right", "Up", "Down",
    "LeftShift", "RightShift", "LeftCtrl", "RightCtrl", "LeftAlt", "RightAlt",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};

bool read_keys(const Json* list, std::vector<Key>& out, const std::string& where, std::string* error) {
    if (!list) return true;
    if (!list->is_array()) {
        if (error) *error = where + ": expected a list of key names";
        return false;
    }
    for (const Json& item : list->as_array()) {
        const Key key = key_from_name(item.as_string());
        if (!item.is_string() || key == Key::Unknown) {
            if (error) *error = where + ": unknown key " + item.dump() + " (names: A..Z, Num0..Num9, Space, Enter, Escape, Tab, Left, Right, Up, Down, LeftShift, F1..F12...)";
            return false;
        }
        out.push_back(key);
    }
    return true;
}

constexpr std::array<std::string_view, static_cast<std::size_t>(GamepadButton::Count)> pad_button_names{
    "A", "B", "X", "Y", "Back", "Start", "LeftStick", "RightStick", "LeftShoulder", "RightShoulder", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"};
constexpr std::array<std::string_view, static_cast<std::size_t>(GamepadAxis::Count)> pad_axis_names{
    "LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger"};
constexpr float deadzone = 0.15f;

float dead(float v) { return std::abs(v) < deadzone ? 0.0f : v; }

bool pressed_any(const Input& in, const std::vector<Key>& keys) {
    return std::any_of(keys.begin(), keys.end(), [&](Key k) { return in.down(k); });
}

bool read_gamepad_buttons(const Json* pad, std::vector<GamepadButton>& out, const std::string& where, std::string* error) {
    if (!pad) return true;
    for (const Json& b : pad->as_array()) {
        const auto button = gamepad_button_from_name(b.as_string());
        if (!button) {
            if (error) *error = where + ": unknown gamepad button " + b.dump() + " (A B X Y Back Start LeftStick RightStick LeftShoulder RightShoulder DpadUp DpadDown DpadLeft DpadRight)";
            return false;
        }
        out.push_back(*button);
    }
    return true;
}

bool read_mouse_buttons(const Json* mouse, std::vector<MouseButton>& out, const std::string& where, std::string* error) {
    if (!mouse) return true;
    for (const Json& b : mouse->as_array()) {
        const std::string& n = b.as_string();
        if (n == "Left") out.push_back(MouseButton::Left);
        else if (n == "Right") out.push_back(MouseButton::Right);
        else if (n == "Middle") out.push_back(MouseButton::Middle);
        else {
            if (error) *error = where + ": unknown mouse button " + b.dump() + " (Left, Right, Middle)";
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<GamepadButton> gamepad_button_from_name(std::string_view name) noexcept {
    for (std::size_t i = 0; i < pad_button_names.size(); ++i) {
        if (pad_button_names[i] == name) return static_cast<GamepadButton>(i);
    }
    return std::nullopt;
}

std::optional<GamepadAxis> gamepad_axis_from_name(std::string_view name) noexcept {
    for (std::size_t i = 0; i < pad_axis_names.size(); ++i) {
        if (pad_axis_names[i] == name) return static_cast<GamepadAxis>(i);
    }
    return std::nullopt;
}

Key key_from_name(std::string_view name) noexcept {
    for (std::size_t i = 1; i < key_names.size(); ++i) {
        if (key_names[i] == name) return static_cast<Key>(i);
    }
    return Key::Unknown;
}

bool InputActions::load(const Json& document, std::string* error) {
    const Json& list = document["actions"];
    if (!list.is_object()) {
        if (error) *error = "actions: expected {\"actions\": {...}}";
        return false;
    }
    std::vector<Action> out;
    for (const auto& [name, spec] : list.as_object()) {
        Action a;
        a.name = name;
        const std::string& type = spec["type"].as_string();
        const std::string where = "action \"" + name + "\"";
        if (type == "button") a.type = Action::Type::Button;
        else if (type == "axis") a.type = Action::Type::Axis;
        else if (type == "axis2") a.type = Action::Type::Axis2;
        else {
            if (error) *error = where + ": type must be \"button\", \"axis\" or \"axis2\"";
            return false;
        }
        bool ok = read_keys(spec.find("keys"), a.keys, where, error);
        if (a.type == Action::Type::Axis) {
            ok = ok && read_keys(spec.find("negative"), a.x.negative, where, error) && read_keys(spec.find("positive"), a.x.positive, where, error);
        } else if (a.type == Action::Type::Axis2) {
            if (const Json* x = spec.find("x")) ok = ok && read_keys(x->find("negative"), a.x.negative, where, error) && read_keys(x->find("positive"), a.x.positive, where, error);
            if (const Json* y = spec.find("y")) ok = ok && read_keys(y->find("negative"), a.y.negative, where, error) && read_keys(y->find("positive"), a.y.positive, where, error);
            if (const Json* md = spec.find("mouse_delta")) {
                if (!md->is_array() || md->size() != 2) {
                    if (error) *error = where + ": mouse_delta is [scale_x, scale_y]";
                    return false;
                }
                a.use_mouse_delta = true;
                a.mouse_delta_scale = {static_cast<float>((*md)[0].as_number()), static_cast<float>((*md)[1].as_number())};
            }
        }
        if (!ok || !read_gamepad_buttons(spec.find("gamepad"), a.pad_buttons, where, error)) return false;
        if (const Json* axis = spec.find("gamepad_axis")) {
            a.pad_axis = gamepad_axis_from_name(axis->as_string());
            if (!a.pad_axis) {
                if (error) *error = where + ": unknown gamepad axis " + axis->dump() + " (LeftX LeftY RightX RightY LeftTrigger RightTrigger)";
                return false;
            }
        }
        if (const Json* stick = spec.find("stick")) {
            if (stick->as_string() == "left") a.stick = 1;
            else if (stick->as_string() == "right") a.stick = 2;
            else {
                if (error) *error = where + ": stick is \"left\" or \"right\"";
                return false;
            }
        }
        a.dpad = spec["dpad"].as_bool();
        if (const Json* scale = spec.find("stick_scale"); scale && scale->is_array() && scale->size() == 2) {
            a.stick_scale = {static_cast<float>((*scale)[0].as_number()), static_cast<float>((*scale)[1].as_number())};
        }
        if (!read_mouse_buttons(spec.find("mouse"), a.mouse, where, error)) return false;
        out.push_back(std::move(a));
    }
    actions_ = std::move(out);
    return true;
}

void InputActions::update(const Input& in) {
    for (Action& a : actions_) {
        a.was_down = a.is_down;
        switch (a.type) {
        case Action::Type::Button: {
            bool d = pressed_any(in, a.keys);
            for (MouseButton b : a.mouse) d = d || in.down(b);
            for (GamepadButton b : a.pad_buttons) d = d || in.down(b);
            a.is_down = d;
            a.current = {d ? 1.0f : 0.0f, 0.0f};
            break;
        }
        case Action::Type::Axis: {
            float v = (pressed_any(in, a.x.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.x.negative) ? 1.0f : 0.0f);
            if (a.pad_axis) v += dead(in.axis(*a.pad_axis));
            v = std::clamp(v, -1.0f, 1.0f);
            a.current = {v, 0.0f};
            a.is_down = v != 0.0f;
            break;
        }
        case Action::Type::Axis2: {
            Vec2 v{(pressed_any(in, a.x.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.x.negative) ? 1.0f : 0.0f),
                   (pressed_any(in, a.y.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.y.negative) ? 1.0f : 0.0f)};
            if (a.dpad) {
                v.x += (in.down(GamepadButton::DpadRight) ? 1.0f : 0.0f) - (in.down(GamepadButton::DpadLeft) ? 1.0f : 0.0f);
                v.y += (in.down(GamepadButton::DpadUp) ? 1.0f : 0.0f) - (in.down(GamepadButton::DpadDown) ? 1.0f : 0.0f);
            }
            if (a.stick != 0) {
                Vec2 st{in.axis(a.stick == 1 ? GamepadAxis::LeftX : GamepadAxis::RightX), in.axis(a.stick == 1 ? GamepadAxis::LeftY : GamepadAxis::RightY)};
                if (std::sqrt(st.x * st.x + st.y * st.y) < deadzone) st = {}; // radial deadzone
                v.x += st.x * a.stick_scale.x;
                v.y += st.y * a.stick_scale.y;
            }
            if (!a.use_mouse_delta) v = {std::clamp(v.x, -1.0f, 1.0f), std::clamp(v.y, -1.0f, 1.0f)};
            if (a.use_mouse_delta) {
                const Vec2 d = in.mouse_delta();
                v = {v.x + d.x * a.mouse_delta_scale.x, v.y + d.y * a.mouse_delta_scale.y};
            }
            a.current = v;
            a.is_down = v.x != 0.0f || v.y != 0.0f;
            break;
        }
        }
    }
}

const InputActions::Action* InputActions::find(std::string_view name) const noexcept {
    for (const Action& a : actions_) {
        if (a.name == name) return &a;
    }
    return nullptr;
}

bool InputActions::has(std::string_view action) const noexcept { return find(action) != nullptr; }
bool InputActions::down(std::string_view action) const noexcept {
    const Action* a = find(action);
    return a && a->is_down;
}
bool InputActions::pressed(std::string_view action) const noexcept {
    const Action* a = find(action);
    return a && a->is_down && !a->was_down;
}
bool InputActions::released(std::string_view action) const noexcept {
    const Action* a = find(action);
    return a && !a->is_down && a->was_down;
}
float InputActions::value(std::string_view action) const noexcept {
    const Action* a = find(action);
    return a ? a->current.x : 0.0f;
}
Vec2 InputActions::axis(std::string_view action) const noexcept {
    const Action* a = find(action);
    return a ? a->current : Vec2{};
}
std::vector<std::string> InputActions::names() const {
    std::vector<std::string> out;
    for (const Action& a : actions_) out.push_back(a.name);
    return out;
}

} // namespace astral
