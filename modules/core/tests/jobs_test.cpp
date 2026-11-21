#include <astral/core/jobs.hpp>
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

using astral::JobCounter;
using astral::JobSystem;

TEST_CASE("worker count is at least one") {
    JobSystem jobs(0);
    CHECK(jobs.worker_count() >= 1);
}

TEST_CASE("all submitted jobs run before wait returns") {
    JobSystem jobs(4);
    JobCounter counter;
    std::atomic<int> sum{0};
    for (int i = 0; i < 10'000; ++i) {
        jobs.submit(counter, [&sum] { sum.fetch_add(1, std::memory_order_relaxed); });
    }
    jobs.wait(counter);
    CHECK(sum.load() == 10'000);
}

TEST_CASE("parallel_for visits every index exactly once") {
    JobSystem jobs(4);
    JobCounter counter;
    constexpr std::size_t count = 1'000'000;
    std::vector<std::atomic<std::uint8_t>> hits(count);
    jobs.parallel_for(counter, count, 4096, [&hits](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            hits[i].fetch_add(1, std::memory_order_relaxed);
        }
    });
    jobs.wait(counter);
    std::size_t wrong = 0;
    for (auto& h : hits) {
        wrong += h.load() != 1 ? 1u : 0u;
    }
    CHECK(wrong == 0);
}

TEST_CASE("parallel_for with zero count completes immediately") {
    JobSystem jobs(2);
    JobCounter counter;
    jobs.parallel_for(counter, 0, 64, [](std::size_t, std::size_t) {});
    jobs.wait(counter);
    CHECK(true);
}

TEST_CASE("waiting inside a job does not deadlock") {
    JobSystem jobs(1); // single worker: nested wait must help or it deadlocks
    auto done = std::async(std::launch::async, [&jobs] {
        JobCounter outer;
        std::atomic<int> inner_runs{0};
        jobs.submit(outer, [&jobs, &inner_runs] {
            JobCounter inner;
            for (int i = 0; i < 16; ++i) {
                jobs.submit(inner, [&inner_runs] { inner_runs.fetch_add(1); });
            }
            jobs.wait(inner);
        });
        jobs.wait(outer);
        return inner_runs.load();
    });
    REQUIRE(done.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(done.get() == 16);
}

TEST_CASE("an exception in a job is rethrown by wait and other jobs still run") {
    JobSystem jobs(2);
    JobCounter counter;
    std::atomic<int> ran{0};
    jobs.submit(counter, [] { throw std::runtime_error("job failed"); });
    for (int i = 0; i < 100; ++i) {
        jobs.submit(counter, [&ran] { ran.fetch_add(1); });
    }
    CHECK_THROWS_WITH_AS(jobs.wait(counter), "job failed", std::runtime_error);
    CHECK(ran.load() == 100);
}

TEST_CASE("destroying the system runs all pending jobs") {
    auto ran = std::make_shared<std::atomic<int>>(0);
    JobCounter counter;
    {
        JobSystem jobs(2);
        for (int i = 0; i < 1000; ++i) {
            jobs.submit(counter, [ran] { ran->fetch_add(1); });
        }
    }
    CHECK(ran->load() == 1000);
}

TEST_CASE("a job waiting on another group's counter does not deadlock") {
    // Regression: R waits on its children while a job (PP) that waits on R's group sits
    // earlier in the queue. If R helps by running PP, PP waits on R's group, which can
    // never finish because R is suspended underneath PP on the same stack.
    JobSystem jobs(1);
    auto done = std::async(std::launch::async, [&jobs] {
        JobCounter f;
        JobCounter p;
        std::atomic<int> children{0};
        jobs.submit(f, [&jobs, &children] {
            JobCounter c;
            for (int i = 0; i < 4; ++i) {
                jobs.submit(c, [&children] { children.fetch_add(1); });
            }
            jobs.wait(c);
        });
        jobs.submit(p, [&jobs, &f] { jobs.wait(f); });
        jobs.wait(p);
        jobs.wait(f);
        return children.load();
    });
    REQUIRE(done.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(done.get() == 4);
}
