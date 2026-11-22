#include <astral/core/version.hpp>
#include <doctest/doctest.h>

TEST_CASE("version reports the engine version") {
    // The project version from CMakeLists.txt (passed in by the build), so a version bump needs no edit here.
    CHECK(astral::version() == ASTRAL_EXPECTED_VERSION);
}
