// Generated script API documents (M11.7).
#include <astral/script/api.hpp>
#include <astral/world/scene.hpp>

#include <doctest/doctest.h>

namespace {
enum class Side : std::uint8_t { Player, Opponent };
struct Paddle {
    Side side = Side::Player;
    float speed = 9.0f;
    astral::Vec3 aim{};
    astral::Vec2 half{1, 1};
    astral::Entity owner{};
};
struct Scored {
    Side by = Side::Player;
};
} // namespace
ASTRAL_REFLECT_ENUM(Side, Player, Opponent)
ASTRAL_REFLECT(Paddle, side, speed, aim, half, owner)
ASTRAL_REFLECT(Scored, by)

using namespace astral;

TEST_CASE("the type definitions and the reference list every component, field and event") {
    TypeRegistry types;
    register_world_types(types);
    types.add<Paddle>();
    types.add_dynamic("game::Health", {{"value", FieldKind::Float, 100.0}, {"dead", FieldKind::Bool, false}});
    EventRegistry events;
    events.add<Scored>();
    events.add_dynamic("game::Ping");
    const ScriptApi api = generate_script_api(types, events, {{"Paddle", "pong_rules"}, {"Scored", "pong_rules"}});

    const std::string& d = api.luau_types;
    CHECK(d.find("export type C_Paddle = {") != std::string::npos);
    CHECK(d.find("side: \"Player\" | \"Opponent\"") != std::string::npos);
    CHECK(d.find("aim: vector") != std::string::npos);
    CHECK(d.find("half: Vec2") != std::string::npos);
    CHECK(d.find("owner: Entity?") != std::string::npos);
    CHECK(d.find("export type C_game__Health") != std::string::npos);
    CHECK(d.find("((self: Entity, name: \"Paddle\") -> C_Paddle?)") != std::string::npos);
    CHECK(d.find("(event: \"Scored\", fn: (ctx: Ctx, event: E_Scored) -> ()") != std::string::npos);
    CHECK(d.find("declare astral: {") != std::string::npos);

    const std::string& m = api.markdown;
    CHECK(m.find("### `Paddle` (feature `pong_rules`)") != std::string::npos);
    CHECK(m.find("| `speed` | number |") != std::string::npos);
    CHECK(m.find("one of: Player, Opponent") != std::string::npos);
    CHECK(m.find("### `game::Health` (defined by a script)") != std::string::npos);
    CHECK(m.find("### `Scored` (feature `pong_rules`)") != std::string::npos);
    CHECK(m.find("### `game::Ping` (script-only") != std::string::npos);
    CHECK(m.find("astral.system") != std::string::npos);
}
