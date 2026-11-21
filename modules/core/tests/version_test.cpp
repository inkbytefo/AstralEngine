#include <astral/core/version.hpp>
#include <doctest/doctest.h>

TEST_CASE("version reports the engine version") {
    CHECK(astral::version() == "0.7.0");
}
