// Headless scale benchmarks for M13.6 (ADR-0032):
//   astral-bench [--smoke]
// Measures ECS system scaling, transform propagation, Luau script iteration, cell streaming decode,
// and .ashader compilation.

#include <astral/app/app.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/script/script.hpp>
#include <astral/shader/ashader.hpp>
#include <astral/world/map.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace astral;

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct BenchPos { Vec3 v{}; };
struct BenchVel { Vec3 v{1.0f, 2.0f, 3.0f}; };
struct BenchAcc { Vec3 v{0.1f, 0.2f, 0.3f}; };
struct BenchTemp { float t = 20.0f; };
struct BenchEnergy { float e = 100.0f; };
struct BenchShield { float s = 50.0f; };

struct BenchC0 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };
struct BenchC1 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };
struct BenchC2 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };
struct BenchC3 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };
struct BenchC4 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };
struct BenchC5 { Vec3 a{1, 2, 3}, b{0.1f, 0.2f, 0.3f}; };

template <class Comp>
class WaveBenchSystem final : public System {
public:
    void configure(SystemAccess& access) override { access.query<Comp>(); }
    void update(Context& ctx) override {
        ctx.world.each<Comp>([](Comp& c) {
            c.a += c.b * 0.016f;
            c.b = c.b * 0.999f + c.a * 0.0001f;
        });
    }
};

} // namespace

ASTRAL_REFLECT(BenchPos, v)
ASTRAL_REFLECT(BenchVel, v)

namespace {

void bench_ecs(bool smoke) {
    const std::size_t count = smoke ? 4000 : 100000;
    const int steps = smoke ? 2 : 12;
    auto run_mode = [&](bool parallel) {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f, .parallel_systems = parallel});
        struct BenchGame final : Game {
            std::size_t entity_count = 0;
            Clock::time_point frames_start{};
            double total_ms = 0.0;
            void on_start(Context& ctx) override {
                for (std::size_t i = 0; i < entity_count; ++i) {
                    ctx.world.create(BenchC0{}, BenchC1{}, BenchC2{}, BenchC3{}, BenchC4{}, BenchC5{});
                }
                // 30 systems across 5 waves of 6 independent component streams each.
                for (int wave = 0; wave < 5; ++wave) {
                    ctx.add_system<WaveBenchSystem<BenchC0>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC1>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC2>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC3>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC4>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC5>>(Phase::Update);
                }
                frames_start = Clock::now();
            }
            void on_shutdown(Context&) override { total_ms = elapsed_ms(frames_start); }
        } game;
        game.entity_count = count;
        app.run_frames(game, steps);
        return game.total_ms / steps;
    };

    const double serial_ms = run_mode(false);
    const double parallel_ms = run_mode(true);
    std::printf("[bench_ecs]        entities=%zu systems=30 steps=%d serial=%.3f ms/step parallel=%.3f ms/step speedup=%.2fx\n",
                count, steps, serial_ms, parallel_ms, parallel_ms > 0.0 ? serial_ms / parallel_ms : 1.0);
}

void bench_transforms(bool smoke) {
    const std::size_t roots = smoke ? 1000 : 25000;
    const int depth = 4;
    const int steps = smoke ? 3 : 20;
    World world;
    for (std::size_t r = 0; r < roots; ++r) {
        Entity parent = world.create(Transform{.position = {static_cast<float>(r), 0.0f, 0.0f}}, WorldTransform{});
        for (int d = 1; d < depth; ++d) {
            parent = world.create(Transform{.position = {0.0f, 1.0f, 0.0f}}, Parent{parent}, WorldTransform{});
        }
    }
    JobSystem jobs(0);
    update_transforms(world); // warm up

    const auto start_serial = Clock::now();
    for (int s = 0; s < steps; ++s) update_transforms(world, nullptr);
    const double serial_ms = elapsed_ms(start_serial) / steps;

    const auto start_parallel = Clock::now();
    for (int s = 0; s < steps; ++s) update_transforms(world, &jobs);
    const double parallel_ms = elapsed_ms(start_parallel) / steps;
    std::printf("[bench_transforms] entities=%zu depth=%d serial=%.3f ms parallel=%.3f ms speedup=%.2fx\n",
                roots * static_cast<std::size_t>(depth), depth, serial_ms, parallel_ms,
                parallel_ms > 0.0 ? serial_ms / parallel_ms : 1.0);
}

void bench_script(bool smoke) {
    const std::size_t count = smoke ? 500 : 10000;
    const int steps = smoke ? 3 : 10;
    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    types.add<BenchPos>();
    types.add<BenchVel>();
    for (std::size_t i = 0; i < count; ++i) {
        world.create(BenchPos{{0.0f, 0.0f, 0.0f}}, BenchVel{{1.0f, 2.0f, 3.0f}});
    }
    ScriptHost host({world, types, events, log}, {.budget_ms = 2000.0});
    host.load("bench_each.luau", R"(
        astral.system {
            name = "move",
            phase = "Update",
            run = function(ctx)
                astral.each({"BenchPos", "BenchVel"}, function(e, p, v)
                    p.v = p.v + v.v * ctx.delta
                end)
            end
        }
    )");
    const auto start = Clock::now();
    for (int s = 0; s < steps; ++s) host.run(ScriptPhase::Update, {.delta = 1.0f / 60.0f});
    const double each_ms = elapsed_ms(start) / steps;
    std::printf("[bench_script]     entities=%zu steps=%d astral.each=%.3f ms/step\n", count, steps, each_ms);
}

void bench_streaming(bool smoke) {
    const int cells = smoke ? 16 : 64;
    const int entities_per_cell = smoke ? 20 : 60;
    MapSource src;
    src.map.grids.push_back(GridDesc{.name = "main", .cell_size = 100.0, .load_range = 300.0});
    std::uint64_t next_id = 1;
    for (int c = 0; c < cells; ++c) {
        const double cx = static_cast<double>((c % 8) * 100 + 25);
        const double cz = static_cast<double>((c / 8) * 100 + 25);
        for (int i = 0; i < entities_per_cell; ++i) {
            MapEntity e;
            e.id = next_id++;
            e.grid = "main";
            e.position = {cx + static_cast<double>(i), 0.0, cz};
            Json pos = Json::array();
            pos.push_back(Json(e.position[0]));
            pos.push_back(Json(0.0));
            pos.push_back(Json(e.position[2]));
            Json tf = Json::object();
            tf["position"] = std::move(pos);
            Json comps = Json::object();
            comps["astral::Transform"] = std::move(tf);
            e.record = Json::object();
            e.record["id"] = Json(static_cast<double>(e.id));
            e.record["components"] = std::move(comps);
            src.entities.push_back(std::move(e));
        }
    }
    const CookedMap cooked = cook_map(src);
    std::vector<std::string> blobs;
    blobs.reserve(cooked.cells.size());
    for (const CookedCell& cell : cooked.cells) blobs.push_back(cell.document.dump());

    JobSystem jobs(0);
    const auto start = Clock::now();
    JobCounter counter;
    std::vector<std::vector<MapEntity>> decoded(blobs.size());
    jobs.parallel_for(counter, blobs.size(), 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            if (auto doc = Json::parse(blobs[i])) decoded[i] = read_cell(src.map, *doc);
        }
    });
    jobs.wait(counter);
    const double ms = elapsed_ms(start);
    std::printf("[bench_streaming]  cells=%zu entities=%zu worker_decode=%.3f ms\n",
                blobs.size(), static_cast<std::size_t>(cells * entities_per_cell), ms);
}

void bench_shader_compile(bool smoke) {
    const int count = smoke ? 2 : 10;
    constexpr std::string_view src = R"(
        shader_type surface;
        uniform vec3 tint : source_color = vec3(0.2, 0.7, 1.0);
        uniform float speed : range(0.0, 5.0, 0.1) = 1.5;
        void surface() {
            float w = 0.5 + 0.5 * sin(TIME * speed + WORLD_POSITION.x);
            ALBEDO = tint * w;
            ROUGHNESS = 0.3;
        }
    )";
    const auto start = Clock::now();
    for (int i = 0; i < count; ++i) {
        const ShaderParse parsed = parse_ashader(src);
        const SurfaceShaderBuild build = compile_surface_shader(parsed.source, "bench.ashader");
        if (!build.ok()) std::fprintf(stderr, "bench_shader_compile failed\n");
    }
    const double ms = elapsed_ms(start) / count;
    std::printf("[bench_shader]     compiles=%d avg=%.3f ms/shader\n", count, ms);
}

} // namespace

int main(int argc, char** argv) {
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke") == 0) smoke = true;
    }
    bench_ecs(smoke);
    bench_transforms(smoke);
    bench_script(smoke);
    bench_streaming(smoke);
    bench_shader_compile(smoke);
    return 0;
}
