#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <string>
#include <thread>

using namespace astral;

namespace devtools_test {
class SlowSystem final : public System {
public:
    void update(Context& ctx) override {
        ASTRAL_PROFILE_SCOPE(ctx.profiler, "custom scope");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
};
} // namespace devtools_test

TEST_CASE("the App profiles its phases and every system by name") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
    struct G final : Game {
        void on_start(Context& ctx) override {
            const auto& s = ctx.add_system<devtools_test::SlowSystem>(Phase::Update);
            name = s.name();
        }
        std::string name;
    } game;
    app.run_frames(game, 3);
    CHECK(game.name == "devtools_test::SlowSystem");
    const Profiler& p = *app.context().profiler;
    CHECK(p.frame_times().size() == 3);
    bool phase = false, system = false, custom = false, physics = false;
    std::uint16_t system_depth = 0, custom_depth = 0;
    for (const ProfileSample& s : p.last_frame()) {
        const std::string_view n = s.name;
        phase |= n == "Update";
        physics |= n == "physics.step";
        if (n == "devtools_test::SlowSystem") {
            system = true;
            system_depth = s.depth;
            CHECK(s.ms >= 0.9f);
        }
        if (n == "custom scope") {
            custom = true;
            custom_depth = s.depth;
        }
    }
    CHECK(phase);
    CHECK(physics);
    CHECK(system);
    CHECK(custom);
    CHECK(system_depth == 1);       // inside "Update"
    CHECK(custom_depth == 2);       // inside the system
    CHECK(p.scope_ms("Update") >= p.scope_ms("devtools_test::SlowSystem"));
}
