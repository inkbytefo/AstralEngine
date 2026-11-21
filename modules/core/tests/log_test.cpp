#include <astral/core/log.hpp>
#include <doctest/doctest.h>

#include <memory>
#include <thread>
#include <vector>

using namespace astral;

TEST_CASE("messages below the logger level are dropped") {
    Logger log;
    auto sink = std::make_shared<MemorySink>();
    log.add_sink(sink);
    log.set_level(LogLevel::Warn);

    log.info("hidden");
    log.warn("shown");

    REQUIRE(sink->lines().size() == 1);
    CHECK(sink->lines()[0] == "shown");
}

TEST_CASE("format arguments are applied") {
    Logger log;
    auto sink = std::make_shared<MemorySink>();
    log.add_sink(sink);

    log.info("x={}", 5);

    REQUIRE(sink->lines().size() == 1);
    CHECK(sink->lines()[0] == "x=5");
}

TEST_CASE("every sink receives each record") {
    Logger log;
    auto a = std::make_shared<MemorySink>();
    auto b = std::make_shared<MemorySink>();
    log.add_sink(a);
    log.add_sink(b);

    log.error("boom");

    CHECK(a->lines().size() == 1);
    CHECK(b->lines().size() == 1);
}

TEST_CASE("concurrent logging keeps every line intact") {
    Logger log;
    auto sink = std::make_shared<MemorySink>();
    log.add_sink(sink);

    constexpr int threads = 8;
    constexpr int per_thread = 1000;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&log, t] {
            for (int i = 0; i < per_thread; ++i) {
                log.info("thread-{}-line-{}", t, i);
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }

    auto lines = sink->lines();
    REQUIRE(lines.size() == threads * per_thread);
    for (const auto& line : lines) {
        CHECK(line.rfind("thread-", 0) == 0);
        CHECK(line.find("-line-") != std::string::npos);
    }
}
