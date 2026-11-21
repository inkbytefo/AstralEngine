#include <astral/platform/paths.hpp>

#include <doctest/doctest.h>

TEST_CASE("the executable's folder is known and holds the test binary") {
    const auto dir = astral::executable_directory();
    REQUIRE_FALSE(dir.empty());
    CHECK(std::filesystem::is_directory(dir));
    bool found = false;
    for (const auto& e : std::filesystem::directory_iterator(dir)) found = found || e.path().stem() == "astral_platform_tests";
    CHECK(found);
}
