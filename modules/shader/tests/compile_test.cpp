#include <astral/shader/compile.hpp>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("compile_glsl turns GLSL into SPIR-V") {
    const GlslCompile r = compile_glsl(R"(#version 460
layout(local_size_x = 8) in;
layout(std430, binding = 0) buffer B { float v[]; };
void main() { v[gl_GlobalInvocationID.x] *= 2.0; }
)",
                                       ShaderStage::Compute);
    REQUIRE_MESSAGE(r.ok(), r.log);
    REQUIRE(r.spirv.size() > 5);
    CHECK(r.spirv[0] == 0x07230203u); // SPIR-V magic
}

TEST_CASE("compile errors carry the line given by #line (the .ashader line)") {
    const GlslCompile r = compile_glsl("#version 460\n"
                                       "layout(location = 0) out vec4 color;\n"
                                       "void main() {\n"
                                       "#line 10\n"
                                       "    color = vec4(1.0);\n"
                                       "    color.x = undefined_thing;\n" // line 11 of the .ashader
                                       "}\n",
                                       ShaderStage::Fragment);
    CHECK_FALSE(r.ok());
    REQUIRE_FALSE(r.errors.empty());
    CHECK(r.errors[0].line == 11);
    CHECK(r.errors[0].message.find("undefined_thing") != std::string::npos);
}

TEST_CASE("includes come from the given list") {
    const GlslInclude inc[] = {{"astral/common.glsl", "float twice(float x) { return x * 2.0; }\n"}};
    const GlslCompile r = compile_glsl("#version 460\n#extension GL_GOOGLE_include_directive : require\n"
                                       "#include <astral/common.glsl>\n"
                                       "layout(location = 0) out vec4 color;\n"
                                       "void main() { color = vec4(twice(0.5)); }\n",
                                       ShaderStage::Fragment, inc);
    CHECK_MESSAGE(r.ok(), r.log);
    const GlslCompile missing = compile_glsl("#version 460\n#extension GL_GOOGLE_include_directive : require\n"
                                             "#include <nope.glsl>\nvoid main() {}\n",
                                             ShaderStage::Fragment);
    CHECK_FALSE(missing.ok());
}
