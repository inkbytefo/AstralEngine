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
#include <astral/world/streaming.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <fstream>
#include <unistd.h>
#endif

using namespace astral;

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Percentiles {
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

Percentiles compute_percentiles(std::vector<double> samples) {
    if (samples.empty()) return {};
    std::sort(samples.begin(), samples.end());
    auto at = [&](double q) {
        const std::size_t idx =
            std::min<std::size_t>(samples.size() - 1, static_cast<std::size_t>(std::floor(q * static_cast<double>(samples.size() - 1))));
        return samples[idx];
    };
    return {at(0.50), at(0.95), at(0.99)};
}

double process_rss_mb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
    return 0.0;
#elif defined(__linux__)
    std::ifstream in("/proc/self/statm");
    std::size_t total = 0, rss_pages = 0;
    if (in >> total >> rss_pages) {
        return static_cast<double>(rss_pages * static_cast<std::size_t>(sysconf(_SC_PAGESIZE))) / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    return 0.0;
#endif
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
            Clock::time_point frame_start{};
            std::vector<double> samples;
            void on_start(Context& ctx) override {
                for (std::size_t i = 0; i < entity_count; ++i) {
                    ctx.world.create(BenchC0{}, BenchC1{}, BenchC2{}, BenchC3{}, BenchC4{}, BenchC5{});
                }
                for (int wave = 0; wave < 5; ++wave) {
                    ctx.add_system<WaveBenchSystem<BenchC0>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC1>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC2>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC3>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC4>>(Phase::Update);
                    ctx.add_system<WaveBenchSystem<BenchC5>>(Phase::Update);
                }
                frame_start = Clock::now();
            }
            void on_update(Context&) override { frame_start = Clock::now(); }
            void on_shutdown(Context&) override {}
        } game;
        class FrameSampler final : public System {
        public:
            explicit FrameSampler(BenchGame& g) : game_(g) {}
            void update(Context&) override { game_.samples.push_back(elapsed_ms(game_.frame_start)); }
        private:
            BenchGame& game_;
        };
        game.entity_count = count;
        struct Wrapper final : Game {
            BenchGame* inner = nullptr;
            void on_start(Context& ctx) override {
                inner->on_start(ctx);
                ctx.add_system<FrameSampler>(Phase::PostUpdate, *inner);
            }
            void on_update(Context& ctx) override { inner->on_update(ctx); }
        } wrapper;
        wrapper.inner = &game;
        app.run_frames(wrapper, steps);
        return compute_percentiles(std::move(game.samples));
    };

    const Percentiles serial = run_mode(false);
    const Percentiles parallel = run_mode(true);
    std::printf("[bench_ecs]        entities=%zu systems=30 samples=%d serial(p50/p95/p99)=%.3f/%.3f/%.3f ms "
                "parallel=%.3f/%.3f/%.3f ms speedup=%.2fx rss=%.1f MB\n",
                count, steps, serial.p50, serial.p95, serial.p99, parallel.p50, parallel.p95, parallel.p99,
                parallel.p50 > 0.0 ? serial.p50 / parallel.p50 : 1.0, process_rss_mb());
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

    std::vector<double> serial_samples, parallel_samples;
    serial_samples.reserve(static_cast<std::size_t>(steps));
    parallel_samples.reserve(static_cast<std::size_t>(steps));
    for (int s = 0; s < steps; ++s) {
        const auto t0 = Clock::now();
        update_transforms(world, nullptr);
        serial_samples.push_back(elapsed_ms(t0));
    }
    for (int s = 0; s < steps; ++s) {
        const auto t0 = Clock::now();
        update_transforms(world, &jobs);
        parallel_samples.push_back(elapsed_ms(t0));
    }
    const Percentiles serial = compute_percentiles(std::move(serial_samples));
    const Percentiles parallel = compute_percentiles(std::move(parallel_samples));
    std::printf("[bench_transforms] entities=%zu depth=%d samples=%d serial(p50/p95/p99)=%.3f/%.3f/%.3f ms "
                "parallel=%.3f/%.3f/%.3f ms speedup=%.2fx rss=%.1f MB\n",
                roots * static_cast<std::size_t>(depth), depth, steps, serial.p50, serial.p95, serial.p99,
                parallel.p50, parallel.p95, parallel.p99, parallel.p50 > 0.0 ? serial.p50 / parallel.p50 : 1.0,
                process_rss_mb());
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
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(steps));
    for (int s = 0; s < steps; ++s) {
        const auto t0 = Clock::now();
        host.run(ScriptPhase::Update, {.delta = 1.0f / 60.0f});
        samples.push_back(elapsed_ms(t0));
    }
    const Percentiles p = compute_percentiles(std::move(samples));
    std::printf("[bench_script]     entities=%zu samples=%d astral.each(p50/p95/p99)=%.3f/%.3f/%.3f ms rss=%.1f MB\n",
                count, steps, p.p50, p.p95, p.p99, process_rss_mb());
}

struct CellKeyHash {
    std::size_t operator()(const CellKey& k) const noexcept {
        return (static_cast<std::size_t>(k.grid) << 40) ^
               (static_cast<std::size_t>(static_cast<std::uint32_t>(k.x)) << 20) ^
               static_cast<std::size_t>(static_cast<std::uint32_t>(k.z));
    }
};

class FlightCellLoader final : public CellLoader {
public:
    FlightCellLoader(const MapDesc& map, const std::unordered_map<CellKey, std::string, CellKeyHash>& blobs, JobSystem& jobs)
        : map_(map), blobs_(blobs), jobs_(jobs) {}
    ~FlightCellLoader() override { wait_all(); }

    void begin_load(CellKey cell) override {
        auto it = blobs_.find(cell);
        if (it == blobs_.end()) return;
        auto state = std::make_shared<Pending>();
        {
            std::scoped_lock lock(mu_);
            pending_[cell] = state;
        }
        const std::string* blob = &it->second;
        jobs_.submit(counter_, [this, state, blob]() {
            const auto t0 = Clock::now();
            if (auto doc = Json::parse(*blob)) state->entities = read_cell(map_, *doc);
            state->decode_ms = elapsed_ms(t0);
            state->done.store(true, std::memory_order_release);
        });
    }
    bool ready(CellKey cell) override {
        std::scoped_lock lock(mu_);
        const auto it = pending_.find(cell);
        return it != pending_.end() && it->second->done.load(std::memory_order_acquire);
    }
    void attach(CellKey cell) override {
        std::shared_ptr<Pending> state;
        {
            std::scoped_lock lock(mu_);
            if (auto it = pending_.find(cell); it != pending_.end()) {
                state = std::move(it->second);
                pending_.erase(it);
            }
        }
        if (state) {
            decode_samples.push_back(state->decode_ms);
            attached_entities += state->entities.size();
            ++attached_cells;
        }
    }
    void detach(CellKey) override { ++detached_cells; }
    void cancel(CellKey cell) override {
        std::scoped_lock lock(mu_);
        if (pending_.erase(cell) > 0) ++cancelled_cells;
    }
    void wait_all() { jobs_.wait(counter_); }

    std::vector<double> decode_samples;
    std::size_t attached_cells = 0;
    std::size_t detached_cells = 0;
    std::size_t cancelled_cells = 0;
    std::size_t attached_entities = 0;

private:
    struct Pending {
        std::vector<MapEntity> entities;
        double decode_ms = 0.0;
        std::atomic<bool> done{false};
    };
    const MapDesc& map_;
    const std::unordered_map<CellKey, std::string, CellKeyHash>& blobs_;
    JobSystem& jobs_;
    JobCounter counter_;
    std::mutex mu_;
    std::unordered_map<CellKey, std::shared_ptr<Pending>, CellKeyHash> pending_;
};

void bench_streaming(bool smoke) {
    const int side = smoke ? 6 : 16;
    const int entities_per_cell = smoke ? 20 : 60;
    const double cell_size = 128.0;
    MapSource src;
    src.map.grids.push_back(GridDesc{.name = "main", .cell_size = cell_size, .load_range = 384.0});
    std::uint64_t next_id = 1;
    std::vector<CellKey> cell_keys;
    cell_keys.reserve(static_cast<std::size_t>(side * side));
    for (int gz = 0; gz < side; ++gz) {
        for (int gx = 0; gx < side; ++gx) {
            cell_keys.push_back(CellKey{0, gx, gz});
            const double cx = static_cast<double>(gx) * cell_size + 16.0;
            const double cz = static_cast<double>(gz) * cell_size + 16.0;
            for (int i = 0; i < entities_per_cell; ++i) {
                MapEntity e;
                e.id = next_id++;
                e.grid = "main";
                e.position = {cx + static_cast<double>(i % 16), 0.0, cz + static_cast<double>(i / 16)};
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
    }
    const CookedMap cooked = cook_map(src);
    std::vector<std::string> blobs;
    std::unordered_map<CellKey, std::string, CellKeyHash> blob_by_key;
    blobs.reserve(cooked.cells.size());
    for (const CookedCell& cell : cooked.cells) {
        std::string dumped = cell.document.dump();
        for (const CellKey& k : cell_keys) {
            if (cell_name(src.map, k) == cell.name) {
                blob_by_key.emplace(k, dumped);
                break;
            }
        }
        blobs.push_back(std::move(dumped));
    }

    JobSystem jobs(0);
    const auto burst_start = Clock::now();
    JobCounter counter;
    std::vector<std::vector<MapEntity>> decoded(blobs.size());
    jobs.parallel_for(counter, blobs.size(), 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            if (auto doc = Json::parse(blobs[i])) decoded[i] = read_cell(src.map, *doc);
        }
    });
    jobs.wait(counter);
    const double burst_ms = elapsed_ms(burst_start);

    // Real flight profile: fly diagonally across the map, streaming cells asynchronously on workers.
    FlightCellLoader loader(src.map, blob_by_key, jobs);
    CellStreamer streamer(src.map, cell_keys, loader);
    const int flight_frames = smoke ? 16 : 120;
    const double max_coord = static_cast<double>(side) * cell_size;
    std::vector<double> tick_samples;
    tick_samples.reserve(static_cast<std::size_t>(flight_frames));
    for (int f = 0; f < flight_frames; ++f) {
        const double t = static_cast<double>(f) / static_cast<double>(std::max(flight_frames - 1, 1));
        const StreamingPoint pt{t * max_coord, t * max_coord};
        const auto t0 = Clock::now();
        streamer.update(std::span(&pt, 1), 4);
        tick_samples.push_back(elapsed_ms(t0));
        loader.wait_all();
        streamer.update(std::span(&pt, 1), 4);
    }
    streamer.unload_all();
    loader.wait_all();

    const Percentiles dec_p = compute_percentiles(loader.decode_samples);
    const Percentiles tick_p = compute_percentiles(std::move(tick_samples));
    std::printf("[bench_streaming]  cells=%zu entities=%zu burst_decode=%.3f ms | "
                "flight(frames=%d attached=%zu detached=%zu cancelled=%zu) "
                "cell_decode(p50/p95/p99)=%.3f/%.3f/%.3f ms tick(p50/p95/p99)=%.3f/%.3f/%.3f ms rss=%.1f MB\n",
                blobs.size(), static_cast<std::size_t>(side * side * entities_per_cell), burst_ms,
                flight_frames, loader.attached_cells, loader.detached_cells, loader.cancelled_cells,
                dec_p.p50, dec_p.p95, dec_p.p99, tick_p.p50, tick_p.p95, tick_p.p99, process_rss_mb());
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
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto t0 = Clock::now();
        const ShaderParse parsed = parse_ashader(src);
        const SurfaceShaderBuild build = compile_surface_shader(parsed.source, "bench.ashader");
        if (!build.ok()) std::fprintf(stderr, "bench_shader_compile failed\n");
        samples.push_back(elapsed_ms(t0));
    }
    const Percentiles p = compute_percentiles(std::move(samples));
    std::printf("[bench_shader]     samples=%d compile(p50/p95/p99)=%.3f/%.3f/%.3f ms rss=%.1f MB\n",
                count, p.p50, p.p95, p.p99, process_rss_mb());
}

void bench_physics_p0(bool smoke) {
    PhysicsWorld phys;
    const std::size_t bodies = smoke ? 120 : 400;
    const std::size_t rays = smoke ? 256 : 4096;
    const int steps = smoke ? 6 : 30;
    phys.create_body({.shape = ShapeDesc::box({100, 0.5f, 100}), .motion = MotionType::Static, .position = {0, -0.5f, 0}});
    const ShapeDesc hull = ShapeDesc::convex_hull(
        {{-0.4f, -0.4f, -0.4f}, {0.4f, -0.4f, -0.4f}, {0.4f, -0.4f, 0.4f}, {-0.4f, -0.4f, 0.4f}, {0, 0.6f, 0}});
    const std::vector<std::byte> cooked = cook_physics_shape(hull);
    const auto t_uncook = Clock::now();
    for (std::size_t i = 0; i < bodies / 2; ++i) {
        phys.create_body({.shape = hull, .position = {static_cast<float>(i % 20) * 1.5f - 14.0f, 3.0f + static_cast<float>(i / 20) * 1.5f, 0}});
    }
    const double uncook_ms = elapsed_ms(t_uncook);
    const ShapeHandle shared = phys.create_shape_from_cooked(cooked);
    const auto t_cook = Clock::now();
    for (std::size_t i = bodies / 2; i < bodies; ++i) {
        phys.create_body({.position = {static_cast<float>(i % 20) * 1.5f - 14.0f, 3.0f + static_cast<float>(i / 20) * 1.5f, 4.0f},
                          .cooked_shape = shared});
    }
    const double cook_ms = elapsed_ms(t_cook);

    std::vector<double> solo_samples, contended_samples;
    for (int i = 0; i < steps; ++i) {
        const auto t0 = Clock::now();
        phys.step(1.0f / 60.0f);
        solo_samples.push_back(elapsed_ms(t0));
    }
    JobSystem jobs(4);
    for (int i = 0; i < steps; ++i) {
        JobCounter c;
        jobs.parallel_for(c, 64, 4, [](std::size_t b, std::size_t e) {
            volatile float acc = 0.0f;
            for (std::size_t k = b; k < e; ++k) for (int r = 0; r < 1000; ++r) acc = acc + std::sin(static_cast<float>(k + r));
        });
        const auto t0 = Clock::now();
        phys.step(1.0f / 60.0f);
        contended_samples.push_back(elapsed_ms(t0));
        jobs.wait(c);
    }
    std::vector<RayQuery> rq(rays);
    for (std::size_t i = 0; i < rays; ++i) {
        rq[i] = {.origin = {static_cast<float>(i % 40) - 20.0f, 20.0f, static_cast<float>((i / 40) % 10)}, .direction = {0, -1, 0}, .max_distance = 40.0f};
    }
    std::vector<std::optional<RayHit>> hits(rays);
    const auto t_rs = Clock::now();
    phys.raycast_batch(rq, hits, nullptr);
    const double ray_serial_ms = elapsed_ms(t_rs);
    const auto t_rp = Clock::now();
    phys.raycast_batch(rq, hits, &jobs);
    const double ray_parallel_ms = elapsed_ms(t_rp);

    const Percentiles solo_p = compute_percentiles(std::move(solo_samples));
    const Percentiles cont_p = compute_percentiles(std::move(contended_samples));
    std::printf("[bench_physics_p0] bodies=%zu spawn(uncook=%.2f ms cooked=%.2f ms) "
                "step_solo(p50/p95)=%.3f/%.3f ms step_contended(p50/p95)=%.3f/%.3f ms "
                "rays=%zu(serial=%.2f ms parallel=%.2f ms) rss=%.1f MB\n",
                bodies, uncook_ms, cook_ms, solo_p.p50, solo_p.p95, cont_p.p50, cont_p.p95,
                rays, ray_serial_ms, ray_parallel_ms, process_rss_mb());
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
    bench_physics_p0(smoke);
    return 0;
}
