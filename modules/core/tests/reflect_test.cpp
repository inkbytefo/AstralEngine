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
