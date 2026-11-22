// Input actions (M11.5): keys and mouse behind names.
#include <astral/app/input_actions.hpp>

#include <doctest/doctest.h>

using namespace astral;

namespace {
InputActions make() {
    InputActions actions;
    std::string error;
    const auto doc = Json::parse(R"({"format": "astral.actions", "version": 1, "actions": {
        "fire": {"type": "button", "keys": ["Space"], "mouse": ["Left"]},
        "throttle": {"type": "axis", "negative": ["S"], "positive": ["W"]},
        "move": {"type": "axis2", "x": {"negative": ["A", "Left"], "positive": ["D", "Right"]}, "y": {"negative": ["S"], "positive": ["W"]}},
        "look": {"type": "axis2", "mouse_delta": [0.5, -0.5]}
    }})", &error);
    REQUIRE_MESSAGE(doc, error);
    REQUIRE_MESSAGE(actions.load(*doc, &error), error);
    return actions;
}
} // namespace

TEST_CASE("buttons: held, pressed and released edges from keys or the mouse") {
    InputActions actions = make();
    Input in;
    in.begin_frame();
    in.set_key(Key::Space, true);
    actions.update(in);
    CHECK(actions.down("fire"));
    CHECK(actions.pressed("fire"));
    CHECK(actions.value("fire") == 1.0f);
    in.begin_frame();
    actions.update(in);
    CHECK(actions.down("fire"));
    CHECK_FALSE(actions.pressed("fire"));
    in.begin_frame();
    in.set_key(Key::Space, false);
    in.set_mouse_button(MouseButton::Left, true); // another source keeps it held
    actions.update(in);
    CHECK(actions.down("fire"));
    in.begin_frame();
    in.set_mouse_button(MouseButton::Left, false);
    actions.update(in);
    CHECK(actions.released("fire"));
    CHECK_FALSE(actions.down("fire"));
}

TEST_CASE("axes: opposite keys cancel, several keys per direction, mouse delta is scaled") {
    InputActions actions = make();
    Input in;
    in.begin_frame();
    in.set_key(Key::W, true);
    in.set_key(Key::Right, true);
    actions.update(in);
    CHECK(actions.axis("move").x == 1.0f);
    CHECK(actions.axis("move").y == 1.0f);
    CHECK(actions.value("throttle") == 1.0f);
    in.set_key(Key::S, true); // S and W cancel
    actions.update(in);
    CHECK(actions.axis("move").y == 0.0f);
    in.add_mouse_motion({10, 10}, {4, 2});
    actions.update(in);
    CHECK(actions.axis("look").x == doctest::Approx(2.0f));
    CHECK(actions.axis("look").y == doctest::Approx(-1.0f));
}

TEST_CASE("unknown actions are idle, bad files are refused with a reason and change nothing") {
    InputActions actions = make();
    CHECK_FALSE(actions.down("nothing"));
    CHECK(actions.axis("nothing").x == 0.0f);
    std::string error;
    CHECK_FALSE(actions.load(Json::parse(R"({"actions": {"jump": {"type": "button", "keys": ["Spacebar"]}}})").value(), &error));
    CHECK(error.find("Spacebar") != std::string::npos);
    CHECK_FALSE(actions.load(Json::parse(R"({"actions": {"x": {"type": "stick"}}})").value(), &error));
    CHECK(actions.has("fire")); // the old set survived
    CHECK(key_from_name("F5") == Key::F5);
    CHECK(key_from_name("nope") == Key::Unknown);
}

TEST_CASE("gamepad: buttons, triggers, sticks with a deadzone, the d-pad") {
    InputActions actions;
    std::string error;
    const auto doc = Json::parse(R"({"actions": {
        "jump": {"type": "button", "keys": ["Space"], "gamepad": ["A"]},
        "zoom": {"type": "axis", "gamepad_axis": "RightTrigger"},
        "move": {"type": "axis2", "x": {"negative": ["A"], "positive": ["D"]}, "y": {"negative": ["S"], "positive": ["W"]}, "stick": "left", "dpad": true},
        "look": {"type": "axis2", "stick": "right", "stick_scale": [2, 2], "mouse_delta": [1, 1]}
    }})", &error);
    REQUIRE_MESSAGE(doc, error);
    REQUIRE_MESSAGE(actions.load(*doc, &error), error);
    Input in;
    in.begin_frame();
    in.set_gamepad_connected(true);
    in.set_gamepad_button(GamepadButton::A, true);
    in.set_gamepad_axis(GamepadAxis::RightTrigger, 0.6f);
    in.set_gamepad_axis(GamepadAxis::LeftX, 0.1f); // inside the deadzone
    in.set_gamepad_axis(GamepadAxis::LeftY, 0.1f);
    actions.update(in);
    CHECK(actions.pressed("jump"));
    CHECK(actions.value("zoom") == doctest::Approx(0.6f));
    CHECK(actions.axis("move").x == 0.0f);
    CHECK(actions.axis("move").y == 0.0f);
    in.begin_frame();
    in.set_gamepad_axis(GamepadAxis::LeftX, 0.8f);
    in.set_gamepad_axis(GamepadAxis::LeftY, -0.5f);
    in.set_key(Key::D, true); // keyboard and stick add, then clamp
    actions.update(in);
    CHECK(actions.axis("move").x == 1.0f);
    CHECK(actions.axis("move").y == doctest::Approx(-0.5f));
    in.begin_frame();
    in.set_key(Key::D, false);
    in.set_gamepad_axis(GamepadAxis::LeftX, 0.0f);
    in.set_gamepad_axis(GamepadAxis::LeftY, 0.0f);
    in.set_gamepad_button(GamepadButton::DpadLeft, true);
    in.set_gamepad_axis(GamepadAxis::RightX, 0.5f);
    actions.update(in);
    CHECK(actions.axis("move").x == -1.0f);
    CHECK(actions.axis("look").x == doctest::Approx(1.0f)); // 0.5 * stick_scale 2
    in.begin_frame();
    in.set_gamepad_connected(false); // unplugged: everything idle
    actions.update(in);
    CHECK_FALSE(actions.down("jump"));
    CHECK(actions.axis("move").x == 0.0f);
    CHECK_FALSE(actions.load(Json::parse(R"({"actions": {"x": {"type": "button", "gamepad": ["Z"]}}})").value(), &error));
    CHECK(error.find("\"Z\"") != std::string::npos);
}
