// hello_core — the game owns the loop; the engine is just a set of tools it calls.
#include <astral/core/clock.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/core/version.hpp>

#include <cmath>
#include <memory>
#include <vector>

int main() {
    astral::Logger log;
    log.add_sink(std::make_shared<astral::ConsoleSink>());
    log.info("AstralEngine {}", astral::version());

    astral::JobSystem jobs;
    astral::Clock clock;
    log.info("{} worker threads", jobs.worker_count());

    std::vector<float> positions(100'000, 0.0f);
    clock.tick();
    for (int frame = 0; frame < 3; ++frame) {
        const float dt = clock.tick();

        astral::JobCounter frame_jobs;
        jobs.parallel_for(frame_jobs, positions.size(), 4096,
                          [&](std::size_t begin, std::size_t end) {
                              for (std::size_t i = begin; i < end; ++i) {
                                  positions[i] += std::sin(static_cast<float>(i)) * dt;
                              }
                          });
        jobs.wait(frame_jobs);

        log.info("frame {} dt={:.4f}s", frame, dt);
    }
    log.info("done, elapsed {:.4f}s", clock.elapsed());
}
