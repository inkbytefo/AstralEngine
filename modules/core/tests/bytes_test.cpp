#include <astral/core/bytes.hpp>
#include <astral/core/json.hpp>

#include <doctest/doctest.h>

#include <limits>

using namespace astral;

TEST_CASE("ByteWriter/ByteReader round-trip little-endian values") {
    ByteWriter w;
    w.u8(7);
    w.u32(0xA1B2C3D4u);
    w.u64(0x0102030405060708ull);
    w.f32(1.5f);
    w.f64(-2.25);
    w.varint(0);
    w.varint(300);
    w.varint(std::numeric_limits<std::uint64_t>::max());
    w.string("héllo");
    const std::uint8_t raw[3] = {9, 8, 7};
    w.bytes(raw);
    const std::vector<std::uint8_t> data = w.take();
    CHECK(data[1] == 0xD4); // little-endian on every platform

    ByteReader r(data);
    CHECK(r.u8() == 7);
    CHECK(r.u32() == 0xA1B2C3D4u);
    CHECK(r.u64() == 0x0102030405060708ull);
    CHECK(r.f32() == 1.5f);
    CHECK(r.f64() == -2.25);
    CHECK(r.varint() == 0);
    CHECK(r.varint() == 300);
    CHECK(r.varint() == std::numeric_limits<std::uint64_t>::max());
    CHECK(r.string() == "héllo");
    const auto b = r.bytes(3);
    REQUIRE(b.size() == 3);
    CHECK(b[2] == 7);
    CHECK(r.ok());
    CHECK(r.remaining() == 0);

    // Reading past the end fails softly: zeros, and ok() turns false for good.
    CHECK(r.u32() == 0);
    CHECK_FALSE(r.ok());
    CHECK(r.bytes(1).empty());
}

TEST_CASE("binary JSON round-trips every kind of value and is smaller than text") {
    auto doc = Json::parse(R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 3, "components": {"astral::Transform": {"position": [1.5, -2, 1e20], "rotation": [0, 0.7071, 0, 0.7071]},
                                 "astral::Name": {"value": "ünïcode ✓"}, "flag": true, "nothing": null, "neg": -12345678901}},
        {"id": 4, "components": {}, "empty": []}]})");
    REQUIRE(doc);
    const std::vector<std::uint8_t> bin = doc->to_binary();
    CHECK(Json::is_binary(bin));
    CHECK(bin.size() < doc->dump().size());
    std::string error;
    const auto back = Json::from_binary(bin, &error);
    REQUIRE_MESSAGE(back, error);
    CHECK(*back == *doc);
    CHECK(back->dump() == doc->dump()); // member order kept
}

TEST_CASE("binary JSON refuses damaged input instead of crashing") {
    const Json doc = *Json::parse(R"({"a": [1, 2, {"b": "c"}]})");
    std::vector<std::uint8_t> bin = doc.to_binary();
    std::string error;
    for (std::size_t cut = 0; cut < bin.size(); ++cut) {
        std::vector<std::uint8_t> part(bin.begin(), bin.begin() + static_cast<std::ptrdiff_t>(cut));
        CHECK_FALSE(Json::from_binary(part, &error));
    }
    CHECK_FALSE(Json::is_binary(std::vector<std::uint8_t>{'{', '}'}));
    std::vector<std::uint8_t> bad = bin;
    bad[4] = 0xEE; // unknown tag
    CHECK_FALSE(Json::from_binary(bad, &error));
    CHECK_FALSE(error.empty());
    // A huge count must not allocate wildly.
    std::vector<std::uint8_t> huge = {'A', 'J', 'B', '1', 6, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    CHECK_FALSE(Json::from_binary(huge, &error));
    // Deep nesting is refused, not a stack overflow.
    std::vector<std::uint8_t> deep = {'A', 'J', 'B', '1'};
    for (int i = 0; i < 100000; ++i) {
        deep.push_back(6);
        deep.push_back(1);
    }
    deep.push_back(0);
    CHECK_FALSE(Json::from_binary(deep, &error));
}
