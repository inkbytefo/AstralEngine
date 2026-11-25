// G1.1 item 3: particle shader built-ins mean the same in motion() and particle().
#include <astral/render/surface_shader.hpp>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("G1.1 LIFETIME and LIFE_T are available in both particle stages") {
    const ShaderParse p = parse_ashader("shader_type particle;\n"
                                        "void motion() { SIZE *= 1.0 - LIFE_T; WORLD_POSITION.y += AGE / max(LIFETIME, 0.001); }\n"
                                        "void particle() { ALPHA *= 1.0 - LIFE_T; COLOR *= clamp(LIFETIME - AGE, 0.0, 1.0); }\n");
    REQUIRE(p.errors.empty());
    const ScreenShaderBuild build = compile_screen_shader(p.source, "g1_life_both");
    for (const auto& e : build.errors) MESSAGE(e.message);
    CHECK(build.ok());
    CHECK_FALSE(build.vertex.empty());
    CHECK_FALSE(build.fragment.empty());
}

TEST_CASE("G1.1 a particle() without motion() reads LIFE_T from the engine vertex stage") {
    const ShaderParse p = parse_ashader("shader_type particle;\n"
                                        "void particle() { ALPHA *= smoothstep(1.0, 0.0, LIFE_T); }\n");
    REQUIRE(p.errors.empty());
    const ScreenShaderBuild build = compile_screen_shader(p.source, "g1_life_fragment");
    for (const auto& e : build.errors) MESSAGE(e.message);
    CHECK(build.ok());
    CHECK(build.vertex.empty());
    CHECK_FALSE(build.fragment.empty());
}
