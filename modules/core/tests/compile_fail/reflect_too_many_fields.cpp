// Must NOT compile: 33 fields exceed ASTRAL_REFLECT's limit. Building this file is a ctest that
// expects the static_assert message (see modules/core/CMakeLists.txt, reflect_too_many_fields).
#include <astral/core/reflect.hpp>

namespace game {
struct Wide33 {
    int f0 = 0, f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0, f6 = 0, f7 = 0, f8 = 0, f9 = 0, f10 = 0, f11 = 0, f12 = 0,
        f13 = 0, f14 = 0, f15 = 0, f16 = 0, f17 = 0, f18 = 0, f19 = 0, f20 = 0, f21 = 0, f22 = 0, f23 = 0, f24 = 0,
        f25 = 0, f26 = 0, f27 = 0, f28 = 0, f29 = 0, f30 = 0, f31 = 0, f32 = 0;
};
} // namespace game

ASTRAL_REFLECT(game::Wide33, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
               f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32)

int main() { return static_cast<int>(astral::Reflect<game::Wide33>::id & 1u); }
