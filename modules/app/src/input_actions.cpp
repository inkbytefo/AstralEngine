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

bool pressed_any(const Input& in, const std::vector<Key>& keys) {
    return std::any_of(keys.begin(), keys.end(), [&](Key k) { return in.down(k); });
}

} // namespace

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
        if (!ok) return false;
        if (const Json* mouse = spec.find("mouse")) {
            for (const Json& b : mouse->as_array()) {
                const std::string& n = b.as_string();
                if (n == "Left") a.mouse.push_back(MouseButton::Left);
                else if (n == "Right") a.mouse.push_back(MouseButton::Right);
                else if (n == "Middle") a.mouse.push_back(MouseButton::Middle);
                else {
                    if (error) *error = where + ": unknown mouse button " + b.dump() + " (Left, Right, Middle)";
                    return false;
                }
            }
        }
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
            a.is_down = d;
            a.current = {d ? 1.0f : 0.0f, 0.0f};
            break;
        }
        case Action::Type::Axis: {
            const float v = (pressed_any(in, a.x.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.x.negative) ? 1.0f : 0.0f);
            a.current = {v, 0.0f};
            a.is_down = v != 0.0f;
            break;
        }
        case Action::Type::Axis2: {
            Vec2 v{(pressed_any(in, a.x.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.x.negative) ? 1.0f : 0.0f),
                   (pressed_any(in, a.y.positive) ? 1.0f : 0.0f) - (pressed_any(in, a.y.negative) ? 1.0f : 0.0f)};
            if (a.use_mouse_delta) {
                const Vec2 d = in.mouse_delta();
                v = {d.x * a.mouse_delta_scale.x, d.y * a.mouse_delta_scale.y};
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
