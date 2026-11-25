// Wind and water fields: pure functions of position and time (M12.4).
#include <astral/physics/fields.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using namespace astral;

TEST_CASE("the noise is deterministic, in range and smooth") {
    float lowest = 1.0f, highest = 0.0f, biggest_step = 0.0f;
    float previous = field_noise({0, 0, 0}, 5);
    for (int i = 1; i <= 2000; ++i) {
        const Vec3 p{static_cast<float>(i) * 0.01f, 3.3f, -1.7f};
        const float n = field_noise(p, 5);
        CHECK(n == field_noise(p, 5));
        lowest = std::min(lowest, n);
        highest = std::max(highest, n);
        biggest_step = std::max(biggest_step, std::abs(n - previous));
        previous = n;
    }
    CHECK(lowest >= 0.0f);
    CHECK(highest < 1.0f);
    CHECK(highest - lowest > 0.5f);   // it varies
    CHECK(biggest_step < 0.05f);      // and varies smoothly (0.01 m steps)
    CHECK(field_noise({1.5f, 2, 3}, 1) != field_noise({1.5f, 2, 3}, 2)); // seeds differ
}

TEST_CASE("an empty field world has no wind and no water") {
    FieldWorld fields;
    CHECK(length(fields.wind_at({1, 2, 3}, 0.0)) == 0.0f);
    CHECK_FALSE(fields.water_at({1, 2, 3}, 0.0).inside);
}

TEST_CASE("global wind: a constant, with gusts and turbulence that stay within their strength") {
    FieldWorld fields;
    fields.set_global_wind({.velocity = {10, 0, 0}});
    CHECK(fields.wind_at({50, 5, -20}, 12.0).x == doctest::Approx(10.0f));
    fields.set_global_wind({.velocity = {10, 0, 0}, .gust_strength = 4.0f, .gust_frequency = 0.5f});
    float lowest = 100.0f, highest = -100.0f;
    for (int i = 0; i < 400; ++i) {
        const Vec3 w = fields.wind_at({0, 0, 0}, static_cast<double>(i) * 0.05);
        lowest = std::min(lowest, w.x);
        highest = std::max(highest, w.x);
        CHECK(std::abs(w.y) < 1e-4f); // gusts blow along the wind
    }
    CHECK(lowest >= 6.0f - 1e-3f);
    CHECK(highest <= 14.0f + 1e-3f);
    CHECK(highest - lowest > 2.0f);  // it does gust
    fields.set_global_wind({.velocity = {0, 0, 0}, .turbulence = 2.0f, .turbulence_scale = 5.0f});
    float most = 0.0f;
    for (int i = 0; i < 200; ++i) most = std::max(most, length(fields.wind_at({static_cast<float>(i), 1, 2}, 3.0)));
    CHECK(most > 0.5f);
    CHECK(most < 2.0f * std::sqrt(3.0f) + 1e-3f);
    // The same place and time always give the same wind.
    CHECK(fields.wind_at({4, 5, 6}, 7.0) == fields.wind_at({4, 5, 6}, 7.0));
}

TEST_CASE("wind volumes add to (or replace) the global wind, fading at their edges") {
    FieldWorld fields;
    fields.set_global_wind({.velocity = {2, 0, 0}});
    fields.add_wind_volume({.center = {0, 0, 0}, .half_extents = {10, 10, 10}, .wind = {.velocity = {0, 0, 6}}, .falloff = 4.0f});
    CHECK(fields.wind_at({0, 0, 0}, 0.0) == Vec3{2, 0, 6});                         // inside: both
    CHECK(fields.wind_at({20, 0, 0}, 0.0) == Vec3{2, 0, 0});                        // outside: only the global wind
    const Vec3 edge = fields.wind_at({8, 0, 0}, 0.0);                               // 2 m inside a 4 m falloff: half way (smoothstep 0.5)
    CHECK(edge.z == doctest::Approx(3.0f).epsilon(0.01));
    FieldWorld replaced;
    replaced.set_global_wind({.velocity = {2, 0, 0}});
    replaced.add_wind_volume({.center = {0, 0, 0}, .half_extents = {10, 10, 10}, .wind = {.velocity = {0, 0, 6}}, .replace = true});
    CHECK(replaced.wind_at({0, 0, 0}, 0.0) == Vec3{0, 0, 6}); // inside: only the volume
    CHECK(replaced.wind_at({50, 0, 0}, 0.0) == Vec3{2, 0, 0});
    replaced.clear_volumes();
    CHECK(replaced.wind_at({0, 0, 0}, 0.0) == Vec3{2, 0, 0});
}

TEST_CASE("water volumes: surface, depth, flow, density; waves move the surface") {
    FieldWorld fields;
    fields.add_water_volume({.center = {0, -2, 0}, .half_extents = {20, 2, 20}, .density = 1020.0f, .flow = {1.5f, 0, 0}});
    const WaterSample under = fields.water_at({3, -1.5f, 2}, 0.0);
    CHECK(under.inside);
    CHECK(under.surface_height == doctest::Approx(0.0f));
    CHECK(under.depth == doctest::Approx(1.5f));
    CHECK(under.density == doctest::Approx(1020.0f));
    CHECK(under.flow.x == doctest::Approx(1.5f));
    CHECK_FALSE(fields.water_at({3, 0.5f, 2}, 0.0).inside);     // above the surface
    CHECK_FALSE(fields.water_at({30, -1, 0}, 0.0).inside);      // outside the box sideways
    CHECK_FALSE(fields.water_at({0, -5, 0}, 0.0).inside);       // below the floor of the volume
    FieldWorld waves;
    waves.add_water_volume({.center = {0, -2, 0}, .half_extents = {50, 2, 50}, .wave_height = 0.5f, .wave_length = 10.0f, .wave_speed = 2.0f});
    float lowest = 10.0f, highest = -10.0f;
    for (int i = 0; i < 400; ++i) {
        const WaterSample s = waves.water_at({static_cast<float>(i) * 0.1f, -1, 3}, static_cast<double>(i) * 0.1);
        lowest = std::min(lowest, s.surface_height);
        highest = std::max(highest, s.surface_height);
    }
    CHECK(lowest >= -0.5f - 1e-4f);
    CHECK(highest <= 0.5f + 1e-4f);
    CHECK(highest - lowest > 0.5f); // it moves
    // Overlapping volumes: the deeper one wins.
    fields.add_water_volume({.center = {0, -3, 0}, .half_extents = {20, 4, 20}, .density = 1100.0f}); // its surface is at +1
    CHECK(fields.water_at({0, -3.5f, 0}, 0.0).density == doctest::Approx(1100.0f)); // 4.5 deep in the second, 3.5 in the first
}

TEST_CASE("PH07: field noise is defined for huge and non-finite coordinates") {
    for (const float big : {1e30f, -1e30f, 3e9f, -3e9f, 2147483520.0f}) {
        const float n = astral::field_noise({big, big * 0.5f, -big}, 3);
        CHECK(n >= 0.0f);
        CHECK(n <= 1.0f);
    }
    CHECK(astral::field_noise({std::nanf(""), 0, 0}, 3) == 0.5f);
    CHECK(astral::field_noise({std::numeric_limits<float>::infinity(), 0, 0}, 3) == 0.5f);
}
