#include <astral/core/clock.hpp>
#include <doctest/doctest.h>

#include <chrono>

using namespace std::chrono_literals;

namespace {
struct FakeTimeSource final : astral::ITimeSource {
    std::chrono::nanoseconds t{0};
    std::chrono::nanoseconds now() const override { return t; }
};
} // namespace

TEST_CASE("first tick returns zero") {
    FakeTimeSource src;
    astral::Clock clock(src);
    CHECK(clock.tick() == 0.0f);
}

TEST_CASE("tick returns elapsed seconds since previous tick") {
    FakeTimeSource src;
    astral::Clock clock(src);
    clock.tick();
    src.t += 16ms;
    CHECK(clock.tick() == doctest::Approx(0.016f));
}

TEST_CASE("large gaps are clamped to max delta") {
    FakeTimeSource src;
    astral::Clock clock(src);
    clock.tick();
    src.t += 2s;
    CHECK(clock.tick() == doctest::Approx(0.25f));
}

TEST_CASE("max delta is configurable") {
    FakeTimeSource src;
    astral::Clock clock(src);
    clock.set_max_delta(0.1f);
    clock.tick();
    src.t += 1s;
    CHECK(clock.tick() == doctest::Approx(0.1f));
}

TEST_CASE("elapsed reports unclamped total time") {
    FakeTimeSource src;
    astral::Clock clock(src);
    clock.tick();
    src.t += 2s;
    clock.tick();
    src.t += 500ms;
    clock.tick();
    CHECK(clock.elapsed() == doctest::Approx(2.5));
}

TEST_CASE("default clock uses the steady time source") {
    astral::Clock clock;
    clock.tick();
    CHECK(clock.tick() >= 0.0f);
}
