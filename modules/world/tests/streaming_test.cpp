#include <astral/world/streaming.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <set>

using namespace astral;

namespace {

struct Key {
    std::int32_t x, z;
    friend auto operator<=>(const Key&, const Key&) = default;
};

/// Records calls; loads finish when the test says so.
struct FakeLoader final : CellLoader {
    std::set<Key> loading, ready_set, attached;
    std::vector<Key> attach_order;
    int cancelled = 0;
    void begin_load(CellKey k) override { loading.insert({k.x, k.z}); }
    bool ready(CellKey k) override { return ready_set.count({k.x, k.z}) > 0; }
    void attach(CellKey k) override {
        attached.insert({k.x, k.z});
        attach_order.push_back({k.x, k.z});
        loading.erase({k.x, k.z});
    }
    void detach(CellKey k) override { attached.erase({k.x, k.z}); ready_set.erase({k.x, k.z}); }
    void cancel(CellKey k) override {
        loading.erase({k.x, k.z});
        ready_set.erase({k.x, k.z});
        ++cancelled;
    }
    void finish_all() { ready_set.insert(loading.begin(), loading.end()); }
};

MapDesc one_grid() {
    MapDesc map;
    map.grids.push_back({"main", 100.0, 150.0});
    return map;
}

std::vector<CellKey> row(std::int32_t from, std::int32_t to) {
    std::vector<CellKey> keys;
    for (std::int32_t x = from; x <= to; ++x) keys.push_back({0, x, 0});
    return keys;
}

} // namespace

TEST_CASE("cells within load range of a source start loading and attach when ready") {
    FakeLoader loader;
    CellStreamer streamer(one_grid(), row(-5, 5), loader);
    const StreamingPoint source{50, 50}; // inside cell 0,0
    streamer.update(std::span(&source, 1), 100);
    // Range 150 from (50,50): cells x = -1..1 (distance 50) qualify, x = 2 (150) qualifies, x = -2 (150) too.
    CHECK(loader.loading == std::set<Key>{{-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0}});
    CHECK(loader.attached.empty());
    loader.finish_all();
    streamer.update(std::span(&source, 1), 100);
    CHECK(loader.attached.size() == 5);
    CHECK(loader.attach_order.front() == Key{0, 0}); // nearest first
    CHECK(streamer.stats().attached == 5);
    CHECK(streamer.is_attached({0, 2, 0}));
}

TEST_CASE("attaching is limited per update") {
    FakeLoader loader;
    CellStreamer streamer(one_grid(), row(-5, 5), loader);
    const StreamingPoint source{50, 50};
    streamer.update(std::span(&source, 1), 100);
    loader.finish_all();
    streamer.update(std::span(&source, 1), 2);
    CHECK(loader.attached.size() == 2);
    streamer.update(std::span(&source, 1), 2);
    CHECK(loader.attached.size() == 4);
    CHECK(streamer.stats().waiting == 1);
}

TEST_CASE("hysteresis: a cell just outside the range stays; far away it is detached") {
    FakeLoader loader;
    CellStreamer streamer(one_grid(), row(-5, 5), loader);
    StreamingPoint source{50, 50};
    streamer.update(std::span(&source, 1), 100);
    loader.finish_all();
    streamer.update(std::span(&source, 1), 100);
    REQUIRE(loader.attached.count({-2, 0}));
    // Move right by 10 m: cell -2 is now 160 m away (> 150, < 165 = 150 * 1.1) → kept.
    source.x = 60;
    streamer.update(std::span(&source, 1), 100);
    CHECK(loader.attached.count({-2, 0}));
    // 30 m further: 190 m → detached.
    source.x = 90;
    streamer.update(std::span(&source, 1), 100);
    CHECK_FALSE(loader.attached.count({-2, 0}));
    CHECK(streamer.stats().detached_last_update == 1);
}

TEST_CASE("loads that are no longer wanted are cancelled; several sources union") {
    FakeLoader loader;
    CellStreamer streamer(one_grid(), row(-20, 20), loader);
    StreamingPoint source{50, 50};
    streamer.update(std::span(&source, 1), 100);
    source.x = 1550; // jump far away before anything finished
    streamer.update(std::span(&source, 1), 100);
    CHECK(loader.cancelled == 5);
    CHECK(loader.loading.count({15, 0}));

    FakeLoader loader2;
    CellStreamer two(one_grid(), row(-20, 20), loader2);
    const StreamingPoint sources[] = {{50, 50}, {-1450, 50}};
    two.update(sources, 100);
    CHECK(loader2.loading.count({0, 0}));
    CHECK(loader2.loading.count({-15, 0}));
}

TEST_CASE("unload_all detaches everything") {
    FakeLoader loader;
    CellStreamer streamer(one_grid(), row(-5, 5), loader);
    const StreamingPoint source{50, 50};
    streamer.update(std::span(&source, 1), 100);
    loader.finish_all();
    streamer.update(std::span(&source, 1), 100);
    streamer.unload_all();
    CHECK(loader.attached.empty());
    CHECK(streamer.stats().attached == 0);
}
