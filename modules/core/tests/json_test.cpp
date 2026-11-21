#include <astral/core/json.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <string>

using astral::Json;

TEST_CASE("json values are built, queried and printed in insertion order") {
    Json j = Json::object();
    j["name"] = "arena";
    j["size"] = 64;
    j["lit"] = true;
    j["tags"] = Json::array({"a", "b"});
    j["nothing"] = nullptr;
    CHECK(j.is_object());
    CHECK(j["name"].as_string() == "arena");
    CHECK(j["size"].as_number() == 64.0);
    CHECK(j["lit"].as_bool());
    CHECK(j["tags"].size() == 2);
    CHECK(j["tags"][1].as_string() == "b");
    CHECK(j["nothing"].is_null());
    CHECK(j.find("missing") == nullptr);
    CHECK(j.dump() == R"({"name":"arena","size":64,"lit":true,"tags":["a","b"],"nothing":null})");
}

TEST_CASE("json parses what it prints, including escapes and nesting") {
    const std::string text = R"({ "s": "line\n\"quoted\"\tç", "n": -1.5e3, "a": [1, [2, {}], []], "o": {"x": false} })";
    std::string error;
    const auto j = Json::parse(text, &error);
    REQUIRE_MESSAGE(j.has_value(), error);
    CHECK((*j)["s"].as_string() == "line\n\"quoted\"\t\xc3\xa7");
    CHECK((*j)["n"].as_number() == -1500.0);
    CHECK((*j)["a"][1][0].as_number() == 2.0);
    CHECK((*j)["a"][1][1].is_object());
    CHECK((*j)["o"]["x"].as_bool() == false);
    const auto again = Json::parse(j->dump(2));
    REQUIRE(again.has_value());
    CHECK(*again == *j);
}

TEST_CASE("json numbers round-trip exactly") {
    for (double v : {0.1, 1.0 / 3.0, 123456789.0, -2.5e-7, 9007199254740991.0}) {
        const auto j = Json::parse(Json(v).dump());
        REQUIRE(j.has_value());
        CHECK(j->as_number() == v);
    }
}

TEST_CASE("invalid json is rejected with a position") {
    std::string error;
    CHECK_FALSE(Json::parse(R"({"a": 1,})", &error).has_value());
    CHECK(error.find("line 1") != std::string::npos);
    CHECK_FALSE(Json::parse("[1, 2", &error).has_value());
    CHECK_FALSE(Json::parse(R"("unterminated)", &error).has_value());
    CHECK_FALSE(Json::parse("{} trailing", &error).has_value());
    CHECK_FALSE(Json::parse("nan", &error).has_value());
}

TEST_CASE("wrong-type access returns defaults instead of throwing") {
    const Json j = 5;
    CHECK(j.as_string().empty());
    CHECK(j.as_bool() == false);
    CHECK(j["x"].is_null());
    CHECK(j[3].is_null());
    CHECK(j.size() == 0);
}

TEST_CASE("indexing a non-const json by number is not mistaken for a key") {
    Json j = Json::array({10, 20});
    CHECK(j[0].as_number() == 10.0);
    CHECK(j[1].as_number() == 20.0);
}
