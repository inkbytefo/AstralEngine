#include <astral/core/jobs.hpp>
#include <astral/core/version.hpp>

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
    return sum.load() == 100 ? 0 : 1;
}
