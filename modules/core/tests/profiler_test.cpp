#include <astral/core/profiler.hpp>

#include <doctest/doctest.h>

#include <thread>

namespace demo_ns {
struct Thing {};
} // namespace demo_ns

TEST_CASE("profiler: nested scopes, frame totals and history") {
    astral::Profiler p(4);
    CHECK(p.last_frame().empty());
    for (int frame = 0; frame < 6; ++frame) {
        p.begin_frame();
        {
            ASTRAL_PROFILE_SCOPE(&p, "update");
            {
                ASTRAL_PROFILE_SCOPE(&p, "physics");
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            ASTRAL_PROFILE_SCOPE(&p, "physics"); // the same name twice adds up
        }
        p.begin("left open");                   // closed by end_frame
        p.end_frame();
    }
    const auto samples = p.last_frame();
    REQUIRE(samples.size() == 4);
    CHECK(std::string_view(samples[0].name) == "update");
    CHECK(samples[0].depth == 0);
    CHECK(samples[1].depth == 1);
    CHECK(samples[1].ms >= 1.5f);
    CHECK(samples[0].ms >= samples[1].ms);
    CHECK(samples[3].depth == 0);
    CHECK(p.scope_ms("physics") >= samples[1].ms);
    CHECK(p.scope_ms("nothing") == 0.0f);
    CHECK(p.last_frame_ms() >= samples[0].ms);
    CHECK(p.frame_times().size() == 4); // history capped
    CHECK(p.frame_times().back() == p.last_frame_ms());

    p.set_enabled(false);
    p.begin_frame();
    p.begin("ignored");
    p.end();
    p.end_frame();
    CHECK(p.last_frame().size() == 4); // unchanged while disabled
    ASTRAL_PROFILE_SCOPE(nullptr, "no profiler: no-op");
}

TEST_CASE("demangle gives readable type names") {
    CHECK(astral::demangle(typeid(demo_ns::Thing).name()) == "demo_ns::Thing");
    CHECK(astral::demangle(typeid(int).name()) == "int");
}
