// Luau bytecode for shipped games (M11 follow-up): compile, load, and report syntax errors with file:line.
#include <astral/script/compile.hpp>
#include <astral/script/script.hpp>
#include <astral/world/scene.hpp>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("a compiled script loads and runs like its source") {
    std::vector<std::uint8_t> code;
    std::string error;
    REQUIRE(compile_script("rule.luau", "astral.system { run = function(ctx) astral.send('x::Ping') end }\nastral.define_event('x::Ping')", code, &error));
    const std::string_view bytes(reinterpret_cast<const char*>(code.data()), code.size());
    CHECK(is_compiled_script(bytes));
    CHECK_FALSE(is_compiled_script("astral.log('hi')"));

    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    ScriptHost host({world, types, events, log});
    // The send happens at run time, after define_event ran at load time.
    CHECK(host.load("rule.luau", bytes).empty());
    host.run(ScriptPhase::Update, {.delta = 0.1f});
    CHECK(host.take_errors().empty());
    CHECK(host.loaded("rule.luau"));
}

TEST_CASE("a syntax error is found at compile time, named with file and line") {
    std::vector<std::uint8_t> code;
    std::string error;
    CHECK_FALSE(compile_script("bad.luau", "local x = 1\nlocal = = 2\n", code, &error));
    CHECK(error.find("bad.luau:2") != std::string::npos);
    CHECK(code.empty());
}
