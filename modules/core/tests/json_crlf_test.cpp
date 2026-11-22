#include <astral/core/json.hpp>
#include <doctest/doctest.h>

#include <string>
#include <string_view>

using astral::Json;

namespace {

std::string to_crlf(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 32);
    for (char c : text) {
        if (c == '\n') out += "\r\n";
        else if (c != '\r') out += c;
    }
    return out;
}

} // namespace

TEST_CASE("Json::parse accepts CRLF line endings identically to LF") {
    const std::string lf =
        "{\n"
        "  \"name\": \"arena\",\n"
        "  \"version\": 1,\n"
        "  \"enabled\": true,\n"
        "  \"nothing\": null,\n"
        "  \"values\": [\n"
        "    1,\n"
        "    -2.5,\n"
        "    \"line\\n\\tquoted\"\n"
        "  ],\n"
        "  \"nested\": {\n"
        "    \"x\": 10,\n"
        "    \"y\": 20\n"
        "  }\n"
        "}\n";
    const std::string crlf = to_crlf(lf);

    std::string err_lf;
    std::string err_crlf;
    const auto parsed_lf = Json::parse(lf, &err_lf);
    const auto parsed_crlf = Json::parse(crlf, &err_crlf);

    REQUIRE_MESSAGE(parsed_lf.has_value(), err_lf);
    REQUIRE_MESSAGE(parsed_crlf.has_value(), err_crlf);
    CHECK(*parsed_crlf == *parsed_lf);
    CHECK(parsed_crlf->dump(2) == parsed_lf->dump(2));
}
