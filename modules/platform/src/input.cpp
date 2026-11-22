#include <astral/platform/input.hpp>

namespace astral {

namespace {
template <class Array, class E> auto& at(Array& a, E e) { return a[static_cast<std::size_t>(e)]; }
} // namespace

void Input::apply(ButtonState& state, bool down) noexcept {
    if (down && !state.down) state.pressed = true;
    if (!down && state.down) state.released = true;
    state.down = down;
}

void Input::begin_frame() noexcept {
    for (auto& k : keys_) k.pressed = k.released = false;
    for (auto& b : mouse_) b.pressed = b.released = false;
    for (auto& b : pad_buttons_) b.pressed = b.released = false;
    mouse_delta_ = {};
    scroll_ = {};
    text_.clear();
}

void Input::add_text(std::string_view utf8) { text_ += utf8; }

void Input::clear_keyboard() noexcept {
    keys_ = {};
    text_.clear();
}

void Input::clear_mouse() noexcept {
    mouse_ = {};
    mouse_delta_ = {};
    scroll_ = {};
}

void Input::set_gamepad_connected(bool connected) noexcept {
    gamepad_connected_ = connected;
    if (!connected) {
        pad_buttons_ = {};
        pad_axes_ = {};
    }
}

void Input::set_gamepad_button(GamepadButton button, bool down) noexcept {
    if (button == GamepadButton::Count) return;
    apply(at(pad_buttons_, button), down);
}

void Input::set_gamepad_axis(GamepadAxis axis, float value) noexcept {
    if (axis == GamepadAxis::Count) return;
    at(pad_axes_, axis) = value < -1.0f ? -1.0f : value > 1.0f ? 1.0f : value;
}

bool Input::down(GamepadButton b) const noexcept { return at(pad_buttons_, b).down; }
bool Input::pressed(GamepadButton b) const noexcept { return at(pad_buttons_, b).pressed; }
bool Input::released(GamepadButton b) const noexcept { return at(pad_buttons_, b).released; }
float Input::axis(GamepadAxis a) const noexcept { return at(pad_axes_, a); }

void Input::set_key(Key key, bool down) noexcept {
    if (key == Key::Unknown || key == Key::Count) return;
    apply(at(keys_, key), down);
}

void Input::set_mouse_button(MouseButton button, bool down) noexcept {
    if (button == MouseButton::Count) return;
    apply(at(mouse_, button), down);
}

void Input::add_mouse_motion(Vec2 position, Vec2 delta) noexcept {
    mouse_position_ = position;
    mouse_delta_ = mouse_delta_ + delta;
}

void Input::add_scroll(Vec2 amount) noexcept { scroll_ = scroll_ + amount; }

bool Input::down(Key key) const noexcept { return key < Key::Count && at(keys_, key).down; }
bool Input::pressed(Key key) const noexcept { return key < Key::Count && at(keys_, key).pressed; }
bool Input::released(Key key) const noexcept { return key < Key::Count && at(keys_, key).released; }
bool Input::down(MouseButton b) const noexcept { return b < MouseButton::Count && at(mouse_, b).down; }
bool Input::pressed(MouseButton b) const noexcept { return b < MouseButton::Count && at(mouse_, b).pressed; }
bool Input::released(MouseButton b) const noexcept { return b < MouseButton::Count && at(mouse_, b).released; }

Vec2 Input::axis(Key negative_x, Key positive_x, Key negative_y, Key positive_y) const noexcept {
    auto value = [this](Key neg, Key pos) {
        return (down(pos) ? 1.0f : 0.0f) - (down(neg) ? 1.0f : 0.0f);
    };
    return {value(negative_x, positive_x), value(negative_y, positive_y)};
}

} // namespace astral
