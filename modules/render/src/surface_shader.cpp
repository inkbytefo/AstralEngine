#include <astral/render/surface_shader.hpp>
#include <astral/shader/compile.hpp>

#include <cstddef>
#include <string>

namespace astral::embedded {
extern const std::uint8_t astral_bindless_glsl[];
extern const std::size_t astral_bindless_glsl_size;
extern const std::uint8_t astral_scene_glsl[];
extern const std::size_t astral_scene_glsl_size;
} // namespace astral::embedded

namespace astral {

namespace {

std::vector<GlslInclude> engine_includes() {
    auto text = [](const std::uint8_t* data, std::size_t size) { return std::string(reinterpret_cast<const char*>(data), size); };
    return {{"astral/bindless.glsl", text(embedded::astral_bindless_glsl, embedded::astral_bindless_glsl_size)},
            {"astral/scene.glsl", text(embedded::astral_scene_glsl, embedded::astral_scene_glsl_size)}};
}

/// Shared by every stage: push constants, parameter access, uniforms as macros.
std::string prelude(const ShaderSource& source, const UniformLayout& layout) {
    std::string s = R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push {
    Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance;
    uint64_t params; float time;
} pc;
ASTRAL_BUFFER(AstralParams, uint);
AstralParams astral_params;
uint astral_base;
float astral_pf(uint o) { return uintBitsToFloat(astral_params.data[astral_base + o]); }
uint astral_pu(uint o) { return astral_params.data[astral_base + o]; }
#define TIME pc.time
)";
    for (std::size_t i = 0; i < source.uniforms.size(); ++i) {
        const ShaderUniform& u = source.uniforms[i];
        const std::string o = std::to_string(layout.offsets[i]) + "u";
        auto at = [&](int k) { return "astral_pf(" + std::to_string(layout.offsets[i] + static_cast<std::uint32_t>(k)) + "u)"; };
        std::string value;
        switch (u.type) {
        case UniformType::Float: value = at(0); break;
        case UniformType::Vec2: value = "vec2(" + at(0) + ", " + at(1) + ")"; break;
        case UniformType::Vec3: value = "vec3(" + at(0) + ", " + at(1) + ", " + at(2) + ")"; break;
        case UniformType::Vec4: value = "vec4(" + at(0) + ", " + at(1) + ", " + at(2) + ", " + at(3) + ")"; break;
        case UniformType::Int: value = "int(" + at(0) + ")"; break;
        case UniformType::Bool: value = "(" + at(0) + " != 0.0)"; break;
        case UniformType::Sampler2D: {
            const char* sampler = u.hints.filter == TextureFilter::Nearest || u.hints.filter == TextureFilter::NearestMipmap
                                      ? "ASTRAL_SAMPLER_NEAREST_CLAMP"
                                      : (u.hints.repeat ? "ASTRAL_SAMPLER_LINEAR_REPEAT" : "ASTRAL_SAMPLER_LINEAR_CLAMP");
            value = "sampler2D(astral_textures[nonuniformEXT(astral_pu(" + o + "))], astral_samplers[" + sampler + "])";
            break;
        }
        }
        s += "#define " + u.name + " " + value + "\n";
    }
    return s;
}

std::string vertex_stage(const ShaderSource& source, const UniformLayout& layout, bool shadow) {
    std::string s = prelude(source, layout);
    s += R"(layout(location = 0) out vec3 astral_out_normal;
layout(location = 1) flat out uint astral_out_instance;
layout(location = 2) out vec4 astral_out_clip_now;
layout(location = 3) out vec4 astral_out_clip_prev;
layout(location = 4) out vec2 astral_out_uv;
layout(location = 5) out vec3 astral_out_world;
vec3 VERTEX; vec3 NORMAL; vec2 UV; vec3 WORLD_POSITION; mat4 MODEL_MATRIX;
)";
    for (std::size_t i = 0; i < source.varyings.size(); ++i) {
        s += "layout(location = " + std::to_string(6 + i) + ") out " + source.varyings[i].type + " " + source.varyings[i].name + ";\n";
    }
    s += "#line 1\n" + source.body_without("surface") + "\n#line 100000\n";
    s += R"(void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    uint id = pc.first_instance + gl_InstanceIndex;
    Instance inst = pc.instances.data[id];
    astral_params = AstralParams(pc.params);
    astral_base = pc.materials.data[inst.material].custom.x;
    VERTEX = v.position;
    NORMAL = v.normal;
    UV = v.uv;
    MODEL_MATRIX = inst.model;
    WORLD_POSITION = (inst.model * vec4(v.position, 1.0)).xyz;
)";
    if (source.has_function("vertex")) s += "    vertex();\n";
    s += R"(    vec4 world = inst.model * vec4(VERTEX, 1.0);
    Frame f = pc.frame.f;
)";
    s += shadow ? "    gl_Position = f.light_view_projection * world;\n" : "    gl_Position = f.view_projection * world;\n";
    s += R"(    astral_out_normal = transpose(inverse(mat3(inst.model))) * NORMAL;
    astral_out_instance = id;
    astral_out_clip_now = f.unjittered_view_projection * world;
    astral_out_clip_prev = f.prev_view_projection * (inst.prev_model * vec4(VERTEX, 1.0));
    astral_out_uv = UV;
    astral_out_world = world.xyz;
}
)";
    return s;
}

std::string fragment_stage(const ShaderSource& source, const UniformLayout& layout, bool shadow) {
    std::string s = prelude(source, layout);
    s += R"(layout(location = 0) in vec3 astral_in_normal;
layout(location = 1) flat in uint astral_in_instance;
layout(location = 2) in vec4 astral_in_clip_now;
layout(location = 3) in vec4 astral_in_clip_prev;
layout(location = 4) in vec2 astral_in_uv;
layout(location = 5) in vec3 astral_in_world;
)";
    if (!shadow) {
        s += R"(layout(location = 0) out vec4 astral_out_albedo;
layout(location = 1) out vec4 astral_out_normal;
layout(location = 2) out vec4 astral_out_emissive;
layout(location = 3) out vec2 astral_out_velocity;
)";
    }
    for (std::size_t i = 0; i < source.varyings.size(); ++i) {
        s += "layout(location = " + std::to_string(6 + i) + ") in " + source.varyings[i].type + " " + source.varyings[i].name + ";\n";
    }
    s += R"(vec2 UV; vec3 NORMAL; vec3 WORLD_POSITION; vec3 VIEW_DIR; bool FRONT_FACING; vec2 SCREEN_UV;
vec3 ALBEDO; float ROUGHNESS; float METALLIC; vec3 EMISSION; float ALPHA; float ALPHA_SCISSOR;
vec3 NORMAL_MAP; float NORMAL_MAP_DEPTH; float AO;
)";
    s += "#line 1\n" + source.body_without("vertex") + "\n#line 100000\n";
    s += R"(vec3 astral_perturb(vec3 n, vec3 map, float scale) {
    vec3 dp1 = dFdx(astral_in_world), dp2 = dFdy(astral_in_world);
    vec2 duv1 = dFdx(astral_in_uv), duv2 = dFdy(astral_in_uv);
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float inv = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    map.xy *= scale;
    return normalize(mat3(t * inv, b * inv, n) * map);
}
void main() {
    Instance inst = pc.instances.data[astral_in_instance];
    astral_params = AstralParams(pc.params);
    astral_base = pc.materials.data[inst.material].custom.x;
    Frame f = pc.frame.f;
    UV = astral_in_uv;
    NORMAL = normalize(astral_in_normal);
    FRONT_FACING = gl_FrontFacing;
    if (!gl_FrontFacing) NORMAL = -NORMAL;
    WORLD_POSITION = astral_in_world;
    VIEW_DIR = normalize(f.camera_position.xyz - astral_in_world);
    SCREEN_UV = gl_FragCoord.xy * f.screen.zw;
    ALBEDO = vec3(1.0); ROUGHNESS = 0.6; METALLIC = 0.0; EMISSION = vec3(0.0); ALPHA = 1.0;
    ALPHA_SCISSOR = 0.5; NORMAL_MAP = vec3(0.5, 0.5, 1.0); NORMAL_MAP_DEPTH = 1.0; AO = 1.0;
)";
    if (source.has_function("surface")) s += "    surface();\n";
    if (source.has_render_mode("alpha_test")) s += "    if (ALPHA < ALPHA_SCISSOR) discard;\n";
    if (!shadow) {
        s += R"(    vec3 n = NORMAL;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) n = astral_perturb(n, NORMAL_MAP * 2.0 - 1.0, NORMAL_MAP_DEPTH);
    astral_out_albedo = vec4(ALBEDO * AO, clamp(ROUGHNESS, 0.02, 1.0));
    astral_out_normal = vec4(n, clamp(METALLIC, 0.0, 1.0));
    astral_out_emissive = vec4(EMISSION, 1.0);
    vec2 now = astral_in_clip_now.xy / astral_in_clip_now.w * 0.5 + 0.5;
    vec2 prev = astral_in_clip_prev.xy / max(astral_in_clip_prev.w, 1e-6) * 0.5 + 0.5;
    astral_out_velocity = now - prev;
)";
    }
    s += "}\n";
    return s;
}

/// Lines past the user's body (#line 100000) are the engine's: report them without a line.
void collect(const GlslCompile& c, std::vector<ShaderError>& errors, const char* stage) {
    for (const ShaderError& e : c.errors) {
        ShaderError copy = e;
        if (copy.line >= 100000) {
            copy.message = std::string(stage) + " (engine template): " + copy.message;
            copy.line = 0;
        }
        bool duplicate = false;
        for (const ShaderError& seen : errors) duplicate = duplicate || (seen.line == copy.line && seen.message == copy.message);
        if (!duplicate) errors.push_back(std::move(copy));
    }
}

} // namespace

SurfaceShaderBuild compile_surface_shader(const ShaderSource& source, std::string_view name) {
    SurfaceShaderBuild out;
    if (source.type != ShaderType::Surface) {
        out.errors.push_back({1, "not a surface shader (shader_type " + std::string(to_string(source.type)) + ")"});
        return out;
    }
    out.layout = layout_uniforms(source);
    out.cull_disabled = source.has_render_mode("cull_disabled");
    out.cull_front = source.has_render_mode("cull_front");
    out.casts_shadows = !source.has_render_mode("shadows_disabled");
    const std::vector<GlslInclude> includes = engine_includes();

    GlslCompile frag = compile_glsl(fragment_stage(source, out.layout, false), ShaderStage::Fragment, includes, name);
    collect(frag, out.errors, "fragment");
    GlslCompile vert = compile_glsl(vertex_stage(source, out.layout, false), ShaderStage::Vertex, includes, name);
    collect(vert, out.errors, "vertex");
    if (!out.errors.empty()) return out;
    out.gbuffer_fragment = std::move(frag.spirv);
    out.gbuffer_vertex = std::move(vert.spirv);

    GlslCompile shadow_vert = compile_glsl(vertex_stage(source, out.layout, true), ShaderStage::Vertex, includes, name);
    collect(shadow_vert, out.errors, "shadow vertex");
    out.shadow_vertex = std::move(shadow_vert.spirv);
    if (source.has_render_mode("alpha_test")) {
        GlslCompile shadow_frag = compile_glsl(fragment_stage(source, out.layout, true), ShaderStage::Fragment, includes, name);
        collect(shadow_frag, out.errors, "shadow fragment");
        out.shadow_fragment = std::move(shadow_frag.spirv);
    }
    if (!out.errors.empty()) {
        out.gbuffer_fragment.clear();
        out.gbuffer_vertex.clear();
    }
    return out;
}

} // namespace astral
