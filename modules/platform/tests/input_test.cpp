#include <astral/platform/input.hpp>
#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("key press is down and pressed for one frame only") {
    Input input;
    input.begin_frame();
    input.set_key(Key::W, true);
    CHECK(input.down(Key::W));
    CHECK(input.pressed(Key::W));

    input.begin_frame();
    CHECK(input.down(Key::W));
    CHECK_FALSE(input.pressed(Key::W));
}

TEST_CASE("key release is reported for one frame") {
    Input input;
    input.set_key(Key::Space, true);
    input.begin_frame();
    input.set_key(Key::Space, false);
    CHECK_FALSE(input.down(Key::Space));
    CHECK(input.released(Key::Space));
    input.begin_frame();
    CHECK_FALSE(input.released(Key::Space));
}

TEST_CASE("repeated down events do not re-trigger pressed") {
    Input input;
    input.set_key(Key::A, true);
    input.begin_frame();
    input.set_key(Key::A, true); // OS key repeat
    CHECK_FALSE(input.pressed(Key::A));
}

TEST_CASE("mouse delta accumulates within a frame and resets") {
    Input input;
    input.begin_frame();
    input.add_mouse_motion({10, 10}, {2, 3});
    input.add_mouse_motion({12, 14}, {2, 4});
    CHECK(input.mouse_position() == Vec2{12, 14});
    CHECK(input.mouse_delta() == Vec2{4, 7});
    input.begin_frame();
    CHECK(input.mouse_delta() == Vec2{0, 0});
}

TEST_CASE("wasd axis is normalised per direction") {
    Input input;
    input.set_key(Key::D, true);
    input.set_key(Key::W, true);
    const Vec2 axis = input.wasd();
    CHECK(axis.x == 1.0f);
    CHECK(axis.y == 1.0f);
    input.set_key(Key::A, true); // left and right cancel
    CHECK(input.wasd().x == 0.0f);
}

TEST_CASE("mouse buttons have edges like keys") {
    Input input;
    input.begin_frame();
    input.set_mouse_button(MouseButton::Left, true);
    CHECK(input.pressed(MouseButton::Left));
    input.begin_frame();
    CHECK(input.down(MouseButton::Left));
    CHECK_FALSE(input.pressed(MouseButton::Left));
}

TEST_CASE("typed text collects for one frame") {
    astral::Input in;
    in.add_text("ş");
    in.add_text("a");
    CHECK(in.text() == "şa");
    in.begin_frame();
    CHECK(in.text().empty());
}
