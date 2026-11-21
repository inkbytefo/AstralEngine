#include <astral/core/handle.hpp>
#include <doctest/doctest.h>

#include <string>

using astral::Handle;
using astral::HandlePool;

TEST_CASE("default handle is invalid") {
    Handle<int> h;
    CHECK_FALSE(h.valid());
}

TEST_CASE("created handle resolves to its value") {
    HandlePool<std::string> pool;
    auto h = pool.create("mesh");
    REQUIRE(h.valid());
    REQUIRE(pool.get(h) != nullptr);
    CHECK(*pool.get(h) == "mesh");
    CHECK(pool.size() == 1);
}

TEST_CASE("destroyed handle no longer resolves") {
    HandlePool<int> pool;
    auto h = pool.create(7);
    CHECK(pool.destroy(h));
    CHECK(pool.get(h) == nullptr);
    CHECK(pool.size() == 0);
}

TEST_CASE("destroying twice fails the second time") {
    HandlePool<int> pool;
    auto h = pool.create(1);
    CHECK(pool.destroy(h));
    CHECK_FALSE(pool.destroy(h));
}

TEST_CASE("stale handle never reaches the object that reused its slot") {
    HandlePool<int> pool;
    auto old_h = pool.create(1);
    pool.destroy(old_h);
    auto new_h = pool.create(2);

    CHECK(new_h.index == old_h.index);
    CHECK(new_h.generation != old_h.generation);
    CHECK(pool.get(old_h) == nullptr);
    REQUIRE(pool.get(new_h) != nullptr);
    CHECK(*pool.get(new_h) == 2);
}

TEST_CASE("handle from an out-of-range index is rejected") {
    HandlePool<int> pool;
    Handle<int> bogus{42, 1};
    CHECK(pool.get(bogus) == nullptr);
    CHECK_FALSE(pool.destroy(bogus));
}

TEST_CASE("pool can use a separate tag type for its handles") {
    struct Record {
        int value;
    };
    struct PublicTag;
    HandlePool<Record, PublicTag> pool;
    Handle<PublicTag> h = pool.create(Record{5});
    REQUIRE(pool.get(h) != nullptr);
    CHECK(pool.get(h)->value == 5);
}

TEST_CASE("for_each visits only live objects") {
    HandlePool<int> pool;
    auto a = pool.create(1);
    pool.create(2);
    pool.create(3);
    pool.destroy(a);
    int sum = 0;
    int visits = 0;
    pool.for_each([&](Handle<int>, int& v) {
        sum += v;
        ++visits;
    });
    CHECK(visits == 2);
    CHECK(sum == 5);
}
