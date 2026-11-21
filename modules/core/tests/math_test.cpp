#include <astral/core/math.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {
bool near(float a, float b, float eps = 1e-5f) { return std::abs(a - b) <= eps; }
bool near(Vec3 a, Vec3 b, float eps = 1e-5f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}
bool near(Vec4 a, Vec4 b, float eps = 1e-5f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps) &&
           near(a.w, b.w, eps);
}
} // namespace

TEST_CASE("vector arithmetic") {
    Vec3 a{1, 2, 3};
    Vec3 b{4, 5, 6};
    CHECK(a + b == Vec3{5, 7, 9});
    CHECK(b - a == Vec3{3, 3, 3});
    CHECK(a * 2.0f == Vec3{2, 4, 6});
    CHECK(dot(a, b) == 32.0f);
    CHECK(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}) == Vec3{0, 0, 1});
    CHECK(near(length(Vec3{3, 4, 0}), 5.0f));
    CHECK(near(normalize(Vec3{0, 0, 9}), Vec3{0, 0, 1}));
}

TEST_CASE("normalizing a zero vector returns zero, not NaN") {
    CHECK(normalize(Vec3{0, 0, 0}) == Vec3{0, 0, 0});
}

TEST_CASE("identity matrix leaves points unchanged") {
    CHECK(near(Mat4::identity() * Vec4{1, 2, 3, 1}, Vec4{1, 2, 3, 1}));
}

TEST_CASE("translate, scale and rotate transform points") {
    CHECK(near(translate(Vec3{1, 2, 3}) * Vec4{0, 0, 0, 1}, Vec4{1, 2, 3, 1}));
    CHECK(near(scale(Vec3{2, 3, 4}) * Vec4{1, 1, 1, 1}, Vec4{2, 3, 4, 1}));
    // +90 degrees about +Y maps +X to -Z (right-handed).
    CHECK(near(rotate(Quat::from_axis_angle({0, 1, 0}, radians(90.0f))) * Vec4{1, 0, 0, 1},
               Vec4{0, 0, -1, 1}));
}

TEST_CASE("matrix product composes right to left") {
    Mat4 m = translate(Vec3{10, 0, 0}) * scale(Vec3{2, 2, 2});
    CHECK(near(m * Vec4{1, 0, 0, 1}, Vec4{12, 0, 0, 1}));
}

TEST_CASE("inverse undoes a transform") {
    Mat4 m = translate(Vec3{1, -2, 3}) * rotate(Quat::from_axis_angle({1, 1, 0}, 0.7f)) *
             scale(Vec3{2, 3, 4});
    Mat4 r = inverse(m) * m;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            CHECK(near(r[c][row], c == row ? 1.0f : 0.0f, 1e-4f));
        }
    }
}

TEST_CASE("quaternion rotates vectors and composes") {
    Quat q = Quat::from_axis_angle({0, 0, 1}, radians(90.0f));
    CHECK(near(q * Vec3{1, 0, 0}, Vec3{0, 1, 0}));
    CHECK(near((q * q) * Vec3{1, 0, 0}, Vec3{-1, 0, 0}));
}

TEST_CASE("look_at puts the target on the -Z axis in view space") {
    Mat4 v = look_at(Vec3{0, 0, 5}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    CHECK(near(v * Vec4{0, 0, 0, 1}, Vec4{0, 0, -5, 1}));
}

TEST_CASE("perspective maps near to depth 0 and far to depth 1 (Vulkan clip space)") {
    Mat4 p = perspective(radians(90.0f), 1.0f, 0.1f, 100.0f);
    Vec4 n = p * Vec4{0, 0, -0.1f, 1};
    Vec4 f = p * Vec4{0, 0, -100.0f, 1};
    CHECK(near(n.z / n.w, 0.0f, 1e-4f));
    CHECK(near(f.z / f.w, 1.0f, 1e-4f));
    // Vulkan's clip-space Y points down: a point above the camera lands at negative y.
    Vec4 up = p * Vec4{0, 1, -1, 1};
    CHECK(up.y / up.w < 0.0f);
}
