// vfx_showcase, headless. Proves everything vfx_lab proved (atmosphere, aerial perspective, froxel fog,
// gameplay smoke + colliders + capsule dispersion, VFX timelines) plus Luau-driven VFX and headless audio,
// station switching without leftovers, CLI flags, and loose vs package parity.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/fly_camera/fly_camera.hpp"
#include "features/vfx_director/vfx_director.hpp"
#include "game.hpp"
#include "stations/stations.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

using namespace astral;
using vfx_showcase::Options;
using vfx_showcase::ShowcaseGame;
using vfx_showcase::StationId;

namespace {

AppDesc showcase_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    return d;
}

struct Census {
    std::size_t primitives, cameras, fly_cameras, atmospheres, wind_zones, beacons, dispersers, audio_sources, one_shots;
    float aerial, fog;
    bool operator==(const Census&) const = default;
};

Census census(App& app) {
    World& w = app.world();
    const Context& ctx = app.context();
    return {w.count<PrimitiveMesh>(),
            w.count<Camera>(),
            w.count<fly::FlyCamera>(),
            w.count<Atmosphere>(),
            w.count<WindZone>(),
            w.count<vfx_director::Beacon>(),
            w.count<vfx_director::SmokeDisperser>(),
            w.count<AudioSource>(),
            w.count<vfx_director::SoundOneShot>(),
            ctx.environment.aerial_extinction,
            ctx.environment.fog_extinction};
}

void tap(std::vector<InputScriptEvent>& script, std::uint64_t frame, Key key) {
    script.push_back({.frame = frame, .key = key, .down = true});
    script.push_back({.frame = frame + 2, .key = key, .down = false});
}

template <class Probe> struct Probed final : Game {
    ShowcaseGame inner;
    Probe probe;
    Probed(Options options, Probe p) : inner(options), probe(p) {}
    void on_start(Context& ctx) override { inner.on_start(ctx); }
    void on_update(Context& ctx) override {
        inner.on_update(ctx);
        probe(ctx.time.frame, ctx, inner);
    }
    void on_shutdown(Context& ctx) override { inner.on_shutdown(ctx); }
};

float smoke_transmittance(World& world, Vec3 from, Vec3 to) {
    float result = 1.0f;
    world.each<const GameplaySmoke, const Transform>([&](const GameplaySmoke& smoke, const Transform& t) {
        result = std::min(result, smoke.transmittance(t.position, from, to));
    });
    return result;
}

} // namespace

TEST_CASE("courtyard: loads scene, spawns colliders, Luau triggers VFX timeline and audio silently") {
    App app(showcase_desc());
    std::uint32_t peak_voices = 0;
    std::size_t peak_lights = 0;
    std::size_t peak_decals = 0;
    std::size_t peak_smoke = 0;
    Probed game({.station = StationId::Courtyard}, [&](std::uint64_t, Context& ctx, ShowcaseGame&) {
        if (ctx.audio) peak_voices = std::max(peak_voices, ctx.audio->voice_count());
        peak_lights = std::max(peak_lights, ctx.world.count<PointLight>());
        peak_decals = std::max(peak_decals, ctx.world.count<Decal>());
        peak_smoke = std::max(peak_smoke, ctx.world.count<GameplaySmoke>());
    });
    app.run_frames(game, 45);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().count<Camera>() == 1);
    CHECK(app.world().count<fly::FlyCamera>() == 1);
    CHECK(app.context().physics->body_count() > 0);
    CHECK(app.world().count<WindZone>() >= 1);
    CHECK(peak_smoke >= 1);
    CHECK(peak_lights >= 1);
    CHECK(peak_decals >= 1);
    // Headless audio mixes silently with game time: both ambient wind and VFX sound track play.
    REQUIRE(app.context().audio != nullptr);
    CHECK(peak_voices >= 2);
    CHECK(app.context().audio->mixed_frames() > 0);
    std::uint32_t bursts = 0;
    app.world().each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) { bursts += b.bursts_fired; });
    CHECK(bursts >= 1);
}

TEST_CASE("particles: wind zone and two Luau beacons fire particle, volume, light and sound tracks") {
    App app(showcase_desc());
    std::uint32_t peak_voices = 0;
    std::size_t peak_lights = 0;
    Probed game({.station = StationId::Particles}, [&](std::uint64_t, Context& ctx, ShowcaseGame&) {
        if (ctx.audio) peak_voices = std::max(peak_voices, ctx.audio->voice_count());
        peak_lights = std::max(peak_lights, ctx.world.count<PointLight>());
    });
    app.run_frames(game, 25);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().count<vfx_director::Beacon>() == 2);
    CHECK(app.context().physics->body_count() > 0);
    CHECK(length(app.context().fields.wind_at({0, 1, -2}, 0.25)) > 0.5f);
    CHECK(peak_lights >= 2);
    CHECK(peak_voices >= 2);
    std::uint32_t bursts = 0;
    app.world().each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) { bursts += b.bursts_fired; });
    CHECK(bursts >= 2);
}

TEST_CASE("smoke: local gameplay smoke spreads without global fog, C disperses it and plays Luau sound") {
    std::vector<InputScriptEvent> script;
    tap(script, 24, Key::C);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    const Vec3 a{-3.8f, 1.05f, -2.4f};
    const Vec3 b{-1.8f, 1.05f, -2.4f};
    float trans_before = 1.0f;
    float trans_dispersed = 0.0f;
    float trans_restored = 1.0f;
    std::uint32_t voices_after_c = 0;
    std::size_t one_shots_after_c = 0;
    std::size_t one_shots_at_end = 99;
    Probed game({.station = StationId::Smoke}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame&) {
        if (frame == 22) trans_before = smoke_transmittance(ctx.world, a, b);
        if (frame == 26) {
            trans_dispersed = smoke_transmittance(ctx.world, a, b);
            one_shots_after_c = ctx.world.count<vfx_director::SoundOneShot>();
            if (ctx.audio) voices_after_c = ctx.audio->voice_count();
        }
        if (frame == 60) {
            trans_restored = smoke_transmittance(ctx.world, a, b);
            one_shots_at_end = ctx.world.count<vfx_director::SoundOneShot>();
        }
    });
    app.run_frames(game, 62);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.context().environment.fog_extinction == doctest::Approx(0.0f));
    CHECK(app.world().count<GameplaySmoke>() == 1);
    CHECK(trans_before < 0.5f);
    CHECK(trans_dispersed > 0.9f);
    CHECK(trans_restored < 0.5f);
    CHECK(one_shots_after_c >= 1);
    CHECK(voices_after_c >= 1);
    // One-shot whoosh finishes as silent headless mixer advances and Luau cleans up the entity.
    CHECK(one_shots_at_end == 0);
    std::uint32_t pulses = 0;
    app.world().each<const vfx_director::SmokeDisperser>([&](const vfx_director::SmokeDisperser& d) { pulses += d.pulses_fired; });
    CHECK(pulses == 1);
}

TEST_CASE("atmosphere: aerial perspective, froxel fog, sky IBL, CLI overrides and toggle keys") {
    {
        App app(showcase_desc());
        ShowcaseGame game({.station = StationId::Atmosphere,
                           .overrides = {.hour = 18.0f, .fog = 0.02f, .aerial = true},
                           .fog_self_shadows = false,
                           .fog_ddgi = false,
                           .sky_ibl = false});
        app.run_frames(game, 5);
        CHECK(app.context().scripts->take_errors().empty());
        const Atmosphere* atmo = app.context().singleton<Atmosphere>();
        REQUIRE(atmo != nullptr);
        CHECK(atmo->hour == doctest::Approx(18.0f));
        CHECK(atmo->enabled);
        CHECK(app.context().environment.fog_extinction == doctest::Approx(0.02f));
        CHECK(app.context().environment.aerial_extinction > 0.0f);
        CHECK_FALSE(game.settings().fog_self_shadows);
        CHECK_FALSE(game.settings().fog_ddgi);
        CHECK_FALSE(game.settings().sky_ibl);
    }
    {
        std::vector<InputScriptEvent> script;
        tap(script, 4, Key::F);
        tap(script, 8, Key::G);
        tap(script, 12, Key::I);
        tap(script, 16, Key::T);
        AppDesc desc = showcase_desc();
        desc.input_script = script;
        App app(desc);
        float hour_at_15 = 0.0f;
        float hour_at_40 = 0.0f;
        Probed game({.station = StationId::Atmosphere}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame&) {
            const Atmosphere* atmo = ctx.singleton<Atmosphere>();
            if (atmo && frame == 15) hour_at_15 = atmo->hour;
            if (atmo && frame == 40) hour_at_40 = atmo->hour;
        });
        app.run_frames(game, 42);
        CHECK_FALSE(game.inner.settings().fog_self_shadows);
        CHECK_FALSE(game.inner.settings().fog_ddgi);
        CHECK_FALSE(game.inner.settings().sky_ibl);
        CHECK(hour_at_40 > hour_at_15 + 0.1f);
    }
}

TEST_CASE("Luau actions: Space fires burst + spatial AudioSource, X sends StopVfx to clear active VFX") {
    std::vector<InputScriptEvent> script;
    tap(script, 12, Key::Space);
    tap(script, 22, Key::X);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    std::size_t one_shots_after_space = 0;
    std::size_t smoke_before_x = 0;
    std::size_t smoke_after_x = 99;
    std::size_t decals_after_x = 99;
    std::uint32_t bursts_after_space = 0;
    std::uint32_t sounds_after_space = 0;
    std::uint32_t stops_at_end = 0;
    Probed game({.station = StationId::Courtyard}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame&) {
        if (frame == 15) {
            one_shots_after_space = ctx.world.count<vfx_director::SoundOneShot>();
            ctx.world.each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) {
                bursts_after_space += b.bursts_fired;
                sounds_after_space += b.sounds_fired;
            });
        }
        if (frame == 21) smoke_before_x = ctx.world.count<GameplaySmoke>();
        if (frame == 26) {
            smoke_after_x = ctx.world.count<GameplaySmoke>();
            decals_after_x = ctx.world.count<Decal>();
            ctx.world.each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) { stops_at_end += b.stops_sent; });
        }
    });
    app.run_frames(game, 30);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(bursts_after_space >= 2);
    CHECK(sounds_after_space >= 1);
    CHECK(one_shots_after_space >= 1);
    CHECK(smoke_before_x >= 1);
    CHECK(stops_at_end >= 1);
    CHECK(smoke_after_x == 0);
    CHECK(decals_after_x == 0);
}

TEST_CASE("number keys 1/2/3/4 switch stations and leave no leftovers behind") {
    Census fresh{};
    {
        App app(showcase_desc());
        ShowcaseGame game({.station = StationId::Courtyard});
        app.run_frames(game, 10);
        fresh = census(app);
    }
    std::vector<InputScriptEvent> script;
    tap(script, 10, Key::Num2);
    tap(script, 20, Key::Num3);
    tap(script, 30, Key::Num4);
    tap(script, 40, Key::Num1);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    std::vector<StationId> seen;
    Probed game({.station = StationId::Courtyard}, [&](std::uint64_t frame, Context&, ShowcaseGame& g) {
        if (frame == 15 || frame == 25 || frame == 35 || frame == 45) seen.push_back(g.station());
    });
    app.run_frames(game, 50);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(seen == std::vector<StationId>{StationId::Particles, StationId::Smoke, StationId::Atmosphere, StationId::Courtyard});
    CHECK(census(app) == fresh);
}

TEST_CASE("station names from the command line; unknown names fall back to courtyard") {
    CHECK(vfx_showcase::station_from_name("courtyard") == StationId::Courtyard);
    CHECK(vfx_showcase::station_from_name("particles") == StationId::Particles);
    CHECK(vfx_showcase::station_from_name("smoke") == StationId::Smoke);
    CHECK(vfx_showcase::station_from_name("atmosphere") == StationId::Atmosphere);
    CHECK(vfx_showcase::station_from_name("volcano") == StationId::Courtyard);
}

TEST_CASE("courtyard behaves the same from loose assets and the package") {
    struct Snapshot {
        std::size_t smoke_count = 0;
        std::size_t light_count = 0;
        std::size_t decal_count = 0;
        std::uint32_t bursts = 0;
        std::uint32_t peak_voices = 0;
        std::uint64_t mixed_frames = 0;
        float transmittance = 1.0f;
        float hour = 0.0f;
    };
    const auto run = [](bool packed) {
        AppDesc desc = showcase_desc();
        if (packed) {
            desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
            REQUIRE(std::filesystem::is_regular_file(desc.pack));
            desc.asset_root = "no-loose-assets-here";
        }
        App app(desc);
        Snapshot s;
        Probed game({.station = StationId::Courtyard}, [&](std::uint64_t, Context& ctx, ShowcaseGame&) {
            if (ctx.audio) s.peak_voices = std::max(s.peak_voices, ctx.audio->voice_count());
        });
        app.run_frames(game, 35);
        CHECK(app.context().scripts->take_errors().empty());
        s.smoke_count = app.world().count<GameplaySmoke>();
        s.light_count = app.world().count<PointLight>();
        s.decal_count = app.world().count<Decal>();
        app.world().each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) { s.bursts += b.bursts_fired; });
        if (app.context().audio) s.mixed_frames = app.context().audio->mixed_frames();
        s.transmittance = smoke_transmittance(app.world(), {-3.8f, 1.05f, -2.4f}, {-1.8f, 1.05f, -2.4f});
        if (const Atmosphere* atmo = app.context().singleton<Atmosphere>()) s.hour = atmo->hour;
        return s;
    };
    const Snapshot loose = run(false);
    const Snapshot packed = run(true);
    CHECK(loose.smoke_count >= 1);
    CHECK(loose.smoke_count == packed.smoke_count);
    CHECK(loose.light_count == packed.light_count);
    CHECK(loose.decal_count == packed.decal_count);
    CHECK(loose.bursts >= 1);
    CHECK(loose.bursts == packed.bursts);
    CHECK(loose.peak_voices >= 2);
    CHECK(loose.peak_voices == packed.peak_voices);
    CHECK(loose.mixed_frames > 0);
    CHECK(loose.mixed_frames == packed.mixed_frames);
    CHECK(loose.transmittance == doctest::Approx(packed.transmittance).epsilon(1e-4));
    CHECK(loose.hour == doctest::Approx(packed.hour).epsilon(1e-4));
}
