// Regression tests for the Master Stabilization Roadmap (docs/status/master-stabilization-roadmap.md).
#include <astral/core/jobs.hpp>
#include <astral/world/serialize.hpp>
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace astral;

namespace msr {
enum class Mode : std::uint8_t { Idle, Run };
struct Counters {
    int health = 100;
    std::uint16_t ammo = 7;
    Mode mode = Mode::Idle;
    float speed = 1.0f;
};
} // namespace msr

ASTRAL_REFLECT(msr::Counters, health, ammo, mode, speed)

namespace {
LoadResult load_counters(World& w, const char* fields) {
    TypeRegistry types;
    register_world_types(types);
    types.add<msr::Counters>();
    const std::string text = std::string(R"({"format": "astral.world", "version": 1, "entities": [{"id": 0, "components": {"msr::Counters": )") +
                             fields + "}}]}";
    const auto doc = Json::parse(text);
    REQUIRE(doc.has_value());
    return load_world(w, types, *doc);
}
} // namespace

TEST_CASE("C04: numbers outside the field's type are reported and the default is kept") {
    World w;
    const LoadResult r = load_counters(w, R"({"health": 1e20, "ammo": -1, "mode": 300, "speed": 1e300})");
    REQUIRE(r.entities.size() == 1);
    const auto* c = w.get<msr::Counters>(r.entities[0]);
    REQUIRE(c != nullptr);
    CHECK(c->health == 100);
    CHECK(c->ammo == 7);
    CHECK(c->mode == msr::Mode::Idle);
    CHECK(c->speed == 1.0f);
    CHECK(r.errors.size() == 4);
}

TEST_CASE("C04: fractional numbers are refused for integer fields; exact ones load") {
    World w;
    const LoadResult r = load_counters(w, R"({"health": 2.5, "ammo": 65535, "mode": 1, "speed": -3.25})");
    REQUIRE(r.entities.size() == 1);
    const auto* c = w.get<msr::Counters>(r.entities[0]);
    CHECK(c->health == 100);
    CHECK(c->ammo == 65535);
    CHECK(c->mode == msr::Mode::Run);
    CHECK(c->speed == -3.25f);
    CHECK(r.errors.size() == 1);
}

TEST_CASE("C03: a $schema_version beyond uint32 is refused, not cast") {
    World w;
    const LoadResult r = load_counters(w, R"({"$schema_version": 1e20, "health": 5})");
    CHECK_FALSE(r.errors.empty());
}

namespace msr {
struct Ping {
    int value = 0;
};
} // namespace msr

TEST_CASE("C02: sending the same event type from inside a read callback is safe and delivered next read") {
    World w;
    for (int i = 0; i < 4; ++i) w.send(msr::Ping{i});
    EventReader<msr::Ping> reader;
    std::vector<int> first;
    reader.read(w, [&](const msr::Ping& p) {
        first.push_back(p.value);
        // Many sends force the queue to grow while it is being read.
        if (p.value < 4) {
            for (int k = 0; k < 64; ++k) w.send(msr::Ping{100 + p.value});
        }
    });
    CHECK(first == std::vector<int>{0, 1, 2, 3});
    int later = 0;
    reader.read(w, [&](const msr::Ping& p) {
        CHECK(p.value >= 100);
        ++later;
    });
    CHECK(later == 4 * 64);
}

namespace msr {
struct Score {
    int value = 0;
};
} // namespace msr

TEST_CASE("C01: parallel_each workers stamp and filter with the caller's ScopedTick") {
    World w;
    for (int i = 0; i < 4096; ++i) w.create(msr::Score{i});
    JobSystem jobs(4);
    {
        World::ScopedTick scope(w, 0, 1000); // a system running at tick 1000
        w.query<msr::Score>().parallel_each(jobs, [](msr::Score& s) { s.value += 1; }, 64);
    }
    // The next system (baseline 999) must see every row as changed at 1000, whichever thread wrote it.
    World::ScopedTick next(w, 999, 1001);
    std::size_t changed = 0;
    w.query<const msr::Score>().changed<msr::Score>().each([&](const msr::Score&) { ++changed; });
    CHECK(changed == 4096);
}

namespace msr {
/// Move-constructible but not move-assignable (a const member): still a valid component.
struct Tag {
    const int id;
};
} // namespace msr

TEST_CASE("C06: World::add replaces a component that is not move-assignable") {
    World w;
    const Entity e = w.create(msr::Tag{1});
    w.add(e, msr::Tag{2});
    CHECK(w.get<msr::Tag>(e)->id == 2);
}
