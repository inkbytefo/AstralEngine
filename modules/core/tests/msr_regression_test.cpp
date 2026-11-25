// Regression tests for the Master Stabilization Roadmap (docs/archive/msr/master-stabilization-roadmap.md).
#include <astral/core/json.hpp>
#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <string>

using namespace astral;

namespace {
constexpr std::int64_t two53 = std::int64_t{1} << 53;
constexpr std::int64_t i64_min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t i64_max = std::numeric_limits<std::int64_t>::max();
constexpr std::uint64_t u64_max = std::numeric_limits<std::uint64_t>::max();

Json text_roundtrip(const Json& j) {
    const auto parsed = Json::parse(j.dump());
    REQUIRE(parsed.has_value());
    return *parsed;
}
Json binary_roundtrip(const Json& j) {
    const auto parsed = Json::from_binary(j.to_binary());
    REQUIRE(parsed.has_value());
    return *parsed;
}
} // namespace

TEST_CASE("C05: 64-bit integers above 2^53 stay distinct and exact") {
    const Json a(two53);
    const Json b(two53 + 1);
    CHECK(a != b);
    CHECK(a.is_number());
    CHECK(b.is_integer());
    CHECK(b.as_int64() == two53 + 1);
    CHECK(Json(i64_min).as_int64() == i64_min);
    CHECK(Json(i64_max).as_int64() == i64_max);
    CHECK(Json(u64_max).as_uint64() == u64_max);
    CHECK_FALSE(Json(u64_max).as_int64().has_value());
    CHECK_FALSE(Json(std::int64_t{-1}).as_uint64().has_value());
}

TEST_CASE("C05: text and AJB roundtrips keep 64-bit integers") {
    for (const Json& v : {Json(two53 - 1), Json(two53), Json(two53 + 1), Json(i64_min), Json(i64_max), Json(u64_max),
                          Json(std::uint64_t{1} << 63)}) {
        CAPTURE(v.dump());
        CHECK(text_roundtrip(v) == v);
        CHECK(binary_roundtrip(v) == v);
        CHECK(text_roundtrip(v).as_uint64() == v.as_uint64());
        CHECK(binary_roundtrip(v).as_int64() == v.as_int64());
    }
    CHECK(Json(two53 + 1).dump() == "9007199254740993");
    CHECK(Json(u64_max).dump() == "18446744073709551615");
    const auto parsed = Json::parse("[9007199254740993, -9223372036854775808, 18446744073709551615]");
    REQUIRE(parsed.has_value());
    CHECK((*parsed)[0].as_int64() == two53 + 1);
    CHECK((*parsed)[1].as_int64() == i64_min);
    CHECK((*parsed)[2].as_uint64() == u64_max);
}

TEST_CASE("C05: integers and doubles of the same value compare equal; conversions are checked") {
    CHECK(Json(1) == Json(1.0));
    CHECK(Json(std::uint64_t{5}) == Json(5));
    CHECK(Json(two53 + 1) != Json(static_cast<double>(two53 + 1))); // the double is 2^53
    CHECK(Json(3).as_number() == 3.0);
    CHECK(Json(2.0).as_int64() == 2);
    CHECK_FALSE(Json(2.5).as_int64().has_value());
    CHECK_FALSE(Json(1e300).as_int64().has_value());
    CHECK_FALSE(Json(9223372036854775808.0).as_int64().has_value());
    CHECK(Json(9223372036854775808.0).as_uint64() == std::uint64_t{1} << 63);
    CHECK_FALSE(Json("1").as_int64().has_value());
    // Numbers with a fraction or exponent stay doubles.
    CHECK_FALSE(Json::parse("1.0")->is_integer());
    CHECK_FALSE(Json::parse("1e3")->is_integer());
    CHECK(Json::parse("1e3")->as_int64() == 1000);
    // Beyond uint64: a double, not an error.
    CHECK(Json::parse("123456789012345678901234567890")->as_number() > 1e29);
}
