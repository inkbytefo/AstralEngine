// The standard game shell (templates): command line.
#include <astral/app/shell.hpp>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the shell reads capture options and leaves the rest to the game") {
    const char* argv[] = {"game", "--capture", "out.ppm", "--frames", "24", "--width", "320", "--height", "180", "--night", "--view", "2", "level.scene", "--script-api", "docs/api"};
    const ShellOptions o = parse_shell_args(15, argv);
    CHECK(o.capturing());
    CHECK(o.capture == "out.ppm");
    CHECK(o.frames == 24);
    CHECK(o.width == 320);
    CHECK(o.height == 180);
    CHECK(o.script_api == "docs/api");
    CHECK(o.has("--night"));
    CHECK_FALSE(o.has("--day"));
    CHECK(o.value("--view") == "2");
    CHECK(o.value("--missing", "7") == "7");
    CHECK(o.positional({"--view"}) == std::vector<std::string>{"level.scene"});
}

TEST_CASE("no arguments means a window run with the game's defaults") {
    const char* argv[] = {"game"};
    const ShellOptions o = parse_shell_args(1, argv);
    CHECK_FALSE(o.capturing());
    CHECK(o.frames == 0);
    CHECK(o.args.empty());
}
