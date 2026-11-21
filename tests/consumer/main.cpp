#include <astral/core/jobs.hpp>
#include <astral/core/version.hpp>
#include <astral/gpu/device.hpp>
#include <astral/platform/input.hpp>

#include <shaders/fill.comp.hpp> // compiled by the installed astral_add_shaders

#include <atomic>
#include <cstdio>

int main() {
    astral::JobSystem jobs;
    astral::JobCounter counter;
    std::atomic<int> sum{0};
    for (int i = 0; i < 100; ++i) {
        jobs.submit(counter, [&sum] { sum.fetch_add(1); });
    }
    jobs.wait(counter);
    std::printf("astral %.*s, jobs=%d\n", static_cast<int>(astral::version().size()),
                astral::version().data(), sum.load());
    astral::Input input; // links Astral::Platform
    auto device = astral::gpu::Device::create({}); // links Astral::Gpu (may be null without a GPU)
    std::printf("gpu: %s, shader words: %zu\n", device ? "yes" : "no", astral::shaders::fill_comp.size());
    return sum.load() == 100 ? 0 : 1;
}
