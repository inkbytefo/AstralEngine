// Regression tests for the Master Stabilization Roadmap (docs/archive/msr/master-stabilization-roadmap.md).
#include <astral/core/jobs.hpp>
#include <astral/world/serialize.hpp>
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
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

namespace msr {
struct BigIds {
    std::int64_t signed_id = 0;
    std::uint64_t unsigned_id = 0;
};
} // namespace msr

ASTRAL_REFLECT(msr::BigIds, signed_id, unsigned_id)

TEST_CASE("C05: typed save/load keeps 64-bit integers above 2^53") {
    const msr::BigIds in{(std::int64_t{1} << 53) + 1, std::numeric_limits<std::uint64_t>::max()};
    const Json saved = to_json(in, SaveContext{});
    const auto text = Json::parse(saved.dump());
    const auto binary = Json::from_binary(saved.to_binary());
    REQUIRE(text.has_value());
    REQUIRE(binary.has_value());
    for (const Json* j : {&*text, &*binary}) {
        msr::BigIds out;
        std::vector<std::string> errors;
        LoadContext ctx;
        ctx.errors = &errors;
        CHECK(from_json(*j, out, ctx));
        CHECK(errors.empty());
        CHECK(out.signed_id == in.signed_id);
        CHECK(out.unsigned_id == in.unsigned_id);
    }
    msr::BigIds out;
    std::vector<std::string> errors;
    LoadContext ctx;
    ctx.errors = &errors;
    CHECK_FALSE(from_json(*Json::parse(R"({"unsigned_id": -1})"), out, ctx));
    CHECK_FALSE(from_json(*Json::parse(R"({"signed_id": 9223372036854775808})"), out, ctx));
}

TEST_CASE("C05: reflection fields of 8 bytes roundtrip exactly and saturate without UB") {
    msr::BigIds v{(std::int64_t{1} << 53) + 1, (std::uint64_t{1} << 63) + 1};
    const FieldInfo s{.name = "signed_id", .kind = FieldKind::Int, .offset = offsetof(msr::BigIds, signed_id), .size = 8};
    const FieldInfo u{.name = "unsigned_id", .kind = FieldKind::UInt, .offset = offsetof(msr::BigIds, unsigned_id), .size = 8, .is_signed = false};
    const Json js = field_to_json(s, &v, SaveContext{});
    const Json ju = field_to_json(u, &v, SaveContext{});
    CHECK(js.as_int64() == v.signed_id);
    CHECK(ju.as_uint64() == v.unsigned_id);
    msr::BigIds back;
    CHECK(field_from_json(s, &back, js, LoadContext{}));
    CHECK(field_from_json(u, &back, ju, LoadContext{}));
    CHECK(back.signed_id == v.signed_id);
    CHECK(back.unsigned_id == v.unsigned_id);
    // Doubles at or beyond the type's range saturate to the integer limits (no out-of-range cast).
    write_number(s, &back, 1e30);
    write_number(u, &back, 1e30);
    CHECK(back.signed_id == std::numeric_limits<std::int64_t>::max());
    CHECK(back.unsigned_id == std::numeric_limits<std::uint64_t>::max());
    write_number(s, &back, -1e30);
    write_number(u, &back, -5.0);
    CHECK(back.signed_id == std::numeric_limits<std::int64_t>::min());
    CHECK(back.unsigned_id == 0);
}
