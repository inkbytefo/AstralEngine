#include <astral/core/reflect.hpp>
#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace game {
struct Stats {
    int level = 1;
    float speed = 2.5f;
    bool boss = false;
};
struct Tag {};
} // namespace game

ASTRAL_REFLECT(game::Stats, level, speed, boss)
ASTRAL_REFLECT_EMPTY(game::Tag)

TEST_CASE("reflected types expose name, stable id and fields in order") {
    using R = astral::Reflect<game::Stats>;
    static_assert(astral::is_reflected_v<game::Stats>);
    static_assert(!astral::is_reflected_v<int>);
    CHECK(R::name == "game::Stats");
    CHECK(R::id == astral::stable_hash("game::Stats"));
    std::vector<std::string> names;
    astral::for_each_field<game::Stats>([&](const auto& f) { names.emplace_back(f.name); });
    CHECK(names == std::vector<std::string>{"level", "speed", "boss"});
}

TEST_CASE("member pointers read and write the fields") {
    game::Stats s;
    astral::for_each_field<game::Stats>([&](const auto& f) {
        using T = typename std::remove_cvref_t<decltype(f)>::type;
        if constexpr (std::is_same_v<T, float>) s.*(f.member) = 9.0f;
    });
    CHECK(s.speed == 9.0f);
    int count = 0;
    astral::for_each_field<game::Tag>([&](const auto&) { ++count; });
    CHECK(count == 0);
}

namespace game {
struct Wide32 {
    int f0 = 0, f1 = 1, f2 = 2, f3 = 3, f4 = 4, f5 = 5, f6 = 6, f7 = 7, f8 = 8, f9 = 9, f10 = 10, f11 = 11, f12 = 12,
        f13 = 13, f14 = 14, f15 = 15, f16 = 16, f17 = 17, f18 = 18, f19 = 19, f20 = 20, f21 = 21, f22 = 22, f23 = 23,
        f24 = 24, f25 = 25, f26 = 26, f27 = 27, f28 = 28, f29 = 29, f30 = 30, f31 = 31;
};
} // namespace game

ASTRAL_REFLECT(game::Wide32, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
               f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31)

TEST_CASE("ASTRAL_REFLECT accepts its documented maximum of 32 fields") {
    int count = 0;
    int sum = 0;
    const game::Wide32 w;
    astral::for_each_field<game::Wide32>([&](const auto& f) {
        ++count;
        sum += w.*(f.member);
    });
    CHECK(count == 32);
    CHECK(sum == 31 * 32 / 2);
}
