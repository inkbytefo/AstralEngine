#include <astral/render/surface_shader.hpp>
#include <astral/shader/compile.hpp>

#include <cstddef>
#include <string>

namespace astral::embedded {
extern const std::uint8_t astral_bindless_glsl[];
extern const std::size_t astral_bindless_glsl_size;
extern const std::uint8_t astral_scene_glsl[];
extern const std::size_t astral_scene_glsl_size;
extern const std::uint8_t astral_color_glsl[];
extern const std::size_t astral_color_glsl_size;
extern const std::uint8_t astral_brdf_glsl[];
extern const std::size_t astral_brdf_glsl_size;
extern const std::uint8_t astral_rt_glsl[];
extern const std::size_t astral_rt_glsl_size;
extern const std::uint8_t astral_primary_glsl[];
extern const std::size_t astral_primary_glsl_size;
extern const std::uint8_t astral_sdf_glsl[];
extern const std::size_t astral_sdf_glsl_size;
extern const std::uint8_t astral_rt_lighting_comp[];
extern const std::size_t astral_rt_lighting_comp_size;
extern const std::uint8_t astral_ddgi_trace_comp[];
extern const std::size_t astral_ddgi_trace_comp_size;
} // namespace astral::embedded

namespace astral {

namespace {

std::vector<GlslInclude> engine_includes() {
    auto text = [](const std::uint8_t* data, std::size_t size) { return std::string(reinterpret_cast<const char*>(data), size); };
    return {{"astral/bindless.glsl", text(embedded::astral_bindless_glsl, embedded::astral_bindless_glsl_size)},
            {"astral/scene.glsl", text(embedded::astral_scene_glsl, embedded::astral_scene_glsl_size)},
            {"astral/color.glsl", text(embedded::astral_color_glsl, embedded::astral_color_glsl_size)},
            {"astral/brdf.glsl", text(embedded::astral_brdf_glsl, embedded::astral_brdf_glsl_size)}};
}

/// `#define name value` for every uniform, reading the parameter block (astral_pf / astral_pu).
std::string uniform_macros(const ShaderSource& source, const UniformLayout& layout) {
    std::string s;
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

std::string uniform_undefs(const ShaderSource& source) {
    std::string s;
    for (const ShaderUniform& u : source.uniforms) s += "#undef " + u.name + "\n";
    return s;
}

/// Shared by every stage: push constants, parameter access. The uniforms are macros that exist only
/// around the user's code (see wrap_body), so a uniform called `scale` cannot clash with the engine's.
std::string prelude(const ShaderSource&, const UniformLayout&) {
    std::string s = R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/scene.glsl>
#include <astral/color.glsl>
#include <astral/brdf.glsl>
layout(push_constant, scalar) uniform Push {
    Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance;
    uint64_t params; float time; PointLights lights;
} pc;
ASTRAL_BUFFER(AstralParams, uint);
AstralParams astral_params;
uint astral_base;
float astral_pf(uint o) { return uintBitsToFloat(astral_params.data[astral_base + o]); }
uint astral_pu(uint o) { return astral_params.data[astral_base + o]; }
#define TIME pc.time
)";
    return s;
}

/// The user's code with the uniforms defined around it.
std::string wrap_body(const ShaderSource& source, const UniformLayout& layout, const std::string& body) {
    return uniform_macros(source, layout) + "#line 1\n" + body + "\n#line 100000\n" + uniform_undefs(source);
}

enum class Mode { GBuffer, Shadow, Forward };

std::string vertex_stage(const ShaderSource& source, const UniformLayout& layout, Mode mode) {
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
    s += wrap_body(source, layout, source.body_without("surface"));
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
    // Forward (blended) geometry uses the unjittered matrix: it is drawn over the finished image.
    s += mode == Mode::Shadow    ? "    gl_Position = f.light_view_projection * world;\n"
         : mode == Mode::Forward ? "    gl_Position = f.unjittered_view_projection * world;\n"
                                 : "    gl_Position = f.view_projection * world;\n";
    s += R"(    astral_out_normal = normal_matrix_of(inst.model) * NORMAL;
    astral_out_instance = id;
    astral_out_clip_now = f.unjittered_view_projection * world;
    astral_out_clip_prev = f.prev_view_projection * (inst.prev_model * vec4(VERTEX, 1.0));
    astral_out_uv = UV;
    astral_out_world = world.xyz;
}
)";
    return s;
}

/// Forward lighting for blended surfaces: unified GGX PBR (sun, point lights, sky/ground ambient).
constexpr const char* forward_lighting = R"(vec3 astral_lit(vec3 N, vec3 V, vec3 P, vec3 albedo, float roughness, float metallic) {
    Frame f = pc.frame.f;
    float r = clamp(roughness, ASTRAL_MIN_ROUGHNESS, 1.0);
    vec3 L = -normalize(f.sun_direction.xyz);
    vec3 c = eval_direct_pbr(N, V, L, albedo, r, metallic) * f.sun_color.rgb;
    for (uint i = 0u; i < f.counts.x; ++i) {
        PointLight light = pc.lights.data[i];
        vec3 to = light.position_radius.xyz - P;
        float dist = max(length(to), 1e-4);
        float radius = max(light.position_radius.w, 1e-4);
        if (dist >= radius) continue;
        float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
        falloff = falloff * falloff / (dist * dist + 1.0);
        c += eval_direct_pbr(N, V, to / dist, albedo, r, metallic) * light.color_intensity.rgb * light.color_intensity.a * falloff;
    }
    float nv = max(dot(N, V), 1e-4);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 dfg = env_brdf(f0, r, nv);
    vec3 diffuse_ambient = mix(f.ground_color.rgb, f.sky_color.rgb, N.y * 0.5 + 0.5) * albedo * (1.0 - metallic);
    vec3 reflected = sky_radiance(f, reflect(-V, N));
    vec3 specular_ambient = mix(reflected, mix(f.ground_color.rgb, f.sky_color.rgb, 0.5), r) * dfg;
    return c + (diffuse_ambient * (1.0 - dfg) + specular_ambient) * f.sky_color.a;
}
)";

std::string fragment_stage(const ShaderSource& source, const UniformLayout& layout, Mode mode) {
    std::string s = prelude(source, layout);
    s += R"(layout(location = 0) in vec3 astral_in_normal;
layout(location = 1) flat in uint astral_in_instance;
layout(location = 2) in vec4 astral_in_clip_now;
layout(location = 3) in vec4 astral_in_clip_prev;
layout(location = 4) in vec2 astral_in_uv;
layout(location = 5) in vec3 astral_in_world;
)";
    if (mode == Mode::GBuffer) {
        s += R"(layout(location = 0) out vec4 astral_out_albedo;
layout(location = 1) out vec4 astral_out_normal;
layout(location = 2) out vec4 astral_out_emissive;
layout(location = 3) out vec2 astral_out_velocity;
)";
    } else if (mode == Mode::Forward) {
        s += "layout(location = 0) out vec4 astral_out_color;\n";
    }
    for (std::size_t i = 0; i < source.varyings.size(); ++i) {
        s += "layout(location = " + std::to_string(6 + i) + ") in " + source.varyings[i].type + " " + source.varyings[i].name + ";\n";
    }
    s += R"(vec2 UV; vec3 NORMAL; vec3 WORLD_POSITION; vec3 VIEW_DIR; bool FRONT_FACING; vec2 SCREEN_UV;
vec3 ALBEDO; float ROUGHNESS; float METALLIC; vec3 EMISSION; float ALPHA; float ALPHA_SCISSOR;
vec3 NORMAL_MAP; float NORMAL_MAP_DEPTH; float AO;
)";
    if (mode == Mode::Forward) s += forward_lighting;
    s += wrap_body(source, layout, source.body_without("vertex"));
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
    if (mode == Mode::Forward) {
        s += R"(    vec3 n = NORMAL;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) n = astral_perturb(n, NORMAL_MAP * 2.0 - 1.0, NORMAL_MAP_DEPTH);
    vec3 lit = astral_lit(n, VIEW_DIR, WORLD_POSITION, ALBEDO * AO, ROUGHNESS, METALLIC) + EMISSION;
    vec3 c = astral_aces_tonemap(lit * f.ground_color.a);
    if (f.custom.w != 0u) c = astral_linear_to_srgb(c); // UNORM targets: exact sRGB encode here
    astral_out_color = vec4(c, clamp(ALPHA, 0.0, 1.0));
)";
    } else if (mode == Mode::GBuffer) {
        s += R"(    vec3 n = NORMAL;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) n = astral_perturb(n, NORMAL_MAP * 2.0 - 1.0, NORMAL_MAP_DEPTH);
    astral_out_albedo = vec4(ALBEDO * AO, clamp(ROUGHNESS, ASTRAL_MIN_ROUGHNESS, 1.0));
    astral_out_normal = vec4(n, clamp(METALLIC, 0.0, 1.0));
    astral_out_emissive = vec4(EMISSION, 1.0);
    astral_out_velocity = compute_velocity(astral_in_clip_now, astral_in_clip_prev);
)";
    }
    s += "}\n";
    return s;
}

/// The ray-traced version of one shader's surface() (see compose_rt_surfaces).
std::string rt_chunk(const ShaderSource& source, const UniformLayout& layout) {
    if (!source.has_function("surface")) return {};
    const std::string prefix = "astral_s@ID@_";
    std::string s = "// ---- " + std::string("shader slot @ID@") + "\n";
    for (const std::string& f : source.functions) s += "#define " + f + " " + prefix + f + "\n";
    for (const ShaderVarying& v : source.varyings) s += "#define " + v.name + " " + prefix + v.name + "\n" + v.type + " " + v.name + ";\n";
    s += "#define texture(s, uv) textureLod(s, uv, 2.0)\n#define dFdx(x) ((x) * 0.0)\n#define dFdy(x) ((x) * 0.0)\n";
    s += wrap_body(source, layout, source.body_without("vertex"));
    s += R"(void astral_run_@ID@(uint base, vec2 uv, vec3 view_dir, inout Surface s) {
    astral_base = base;
    UV = uv; NORMAL = s.n; WORLD_POSITION = s.p; VIEW_DIR = view_dir; FRONT_FACING = !s.backface; SCREEN_UV = vec2(0.0);
    ALBEDO = vec3(1.0); ROUGHNESS = 0.6; METALLIC = 0.0; EMISSION = vec3(0.0); ALPHA = 1.0; ALPHA_SCISSOR = 0.5;
    NORMAL_MAP = vec3(0.5, 0.5, 1.0); NORMAL_MAP_DEPTH = 1.0; AO = 1.0;
    surface();
    s.albedo = ALBEDO * AO;
    s.roughness = clamp(ROUGHNESS, ASTRAL_MIN_ROUGHNESS, 1.0);
    s.metallic = clamp(METALLIC, 0.0, 1.0);
    s.emissive = EMISSION;
}
)";
    if (source.has_render_mode("alpha_test")) {
        s += R"(bool astral_alpha_@ID@(uint base, vec2 uv, vec3 p, vec3 n, vec3 view_dir, bool backface) {
    astral_base = base;
    UV = uv; NORMAL = n; WORLD_POSITION = p; VIEW_DIR = view_dir; FRONT_FACING = !backface; SCREEN_UV = vec2(0.0);
    ALBEDO = vec3(1.0); ROUGHNESS = 0.6; METALLIC = 0.0; EMISSION = vec3(0.0); ALPHA = 1.0; ALPHA_SCISSOR = 0.5;
    NORMAL_MAP = vec3(0.5, 0.5, 1.0); NORMAL_MAP_DEPTH = 1.0; AO = 1.0;
    surface();
    return ALPHA >= ALPHA_SCISSOR;
}
)";
    }
    s += R"(#undef texture
#undef dFdx
#undef dFdy
)";
    for (const ShaderVarying& v : source.varyings) s += "#undef " + v.name + "\n";
    for (const std::string& f : source.functions) s += "#undef " + f + "\n";
    return s;
}

void replace_all(std::string& text, std::string_view from, std::string_view to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) text.replace(at, from.size(), to);
}

/// Embedded GLSL as text with '\n' line ends (a Windows checkout may give the files CRLF).
std::string embedded_text(const std::uint8_t* data, std::size_t size) {
    std::string text(reinterpret_cast<const char*>(data), size);
    std::erase(text, '\r');
    return text;
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
    for (std::size_t i = 0; i < source.uniforms.size(); ++i) {
        if (source.uniforms[i].type == UniformType::Sampler2D) out.samplers.push_back({out.layout.offsets[i], source.uniforms[i].hints.hint_normal});
    }
    out.rt_chunk = rt_chunk(source, out.layout);
    out.cull_disabled = source.has_render_mode("cull_disabled");
    out.cull_front = source.has_render_mode("cull_front");
    out.casts_shadows = !source.has_render_mode("shadows_disabled");
    out.alpha_test = source.has_render_mode("alpha_test");
    const std::vector<GlslInclude> includes = engine_includes();

    if (source.has_render_mode("blend_mix") || source.has_render_mode("blend_add")) {
        out.blend = true;
        out.additive = source.has_render_mode("blend_add");
        out.rt_chunk.clear(); // blended surfaces are not in the acceleration structure
        GlslCompile forward_frag = compile_glsl(fragment_stage(source, out.layout, Mode::Forward), ShaderStage::Fragment, includes, name);
        collect(forward_frag, out.errors, "fragment");
        GlslCompile forward_vert = compile_glsl(vertex_stage(source, out.layout, Mode::Forward), ShaderStage::Vertex, includes, name);
        collect(forward_vert, out.errors, "vertex");
        if (out.errors.empty()) {
            out.forward_fragment = std::move(forward_frag.spirv);
            out.forward_vertex = std::move(forward_vert.spirv);
        }
        return out;
    }

    GlslCompile frag = compile_glsl(fragment_stage(source, out.layout, Mode::GBuffer), ShaderStage::Fragment, includes, name);
    collect(frag, out.errors, "fragment");
    GlslCompile vert = compile_glsl(vertex_stage(source, out.layout, Mode::GBuffer), ShaderStage::Vertex, includes, name);
    collect(vert, out.errors, "vertex");
    if (!out.errors.empty()) return out;
    out.gbuffer_fragment = std::move(frag.spirv);
    out.gbuffer_vertex = std::move(vert.spirv);

    GlslCompile shadow_vert = compile_glsl(vertex_stage(source, out.layout, Mode::Shadow), ShaderStage::Vertex, includes, name);
    collect(shadow_vert, out.errors, "shadow vertex");
    out.shadow_vertex = std::move(shadow_vert.spirv);
    if (source.has_render_mode("alpha_test")) {
        GlslCompile shadow_frag = compile_glsl(fragment_stage(source, out.layout, Mode::Shadow), ShaderStage::Fragment, includes, name);
        collect(shadow_frag, out.errors, "shadow fragment");
        out.shadow_fragment = std::move(shadow_frag.spirv);
    }
    if (!out.errors.empty()) {
        out.gbuffer_fragment.clear();
        out.gbuffer_vertex.clear();
    }
    return out;
}

std::string compose_rt_surfaces(std::span<const RtSurface> surfaces) {
    std::string s = R"(// Generated: the custom surface() functions for ray-traced hits.
vec2 UV; vec3 NORMAL; vec3 WORLD_POSITION; vec3 VIEW_DIR; bool FRONT_FACING; vec2 SCREEN_UV;
vec3 ALBEDO; float ROUGHNESS; float METALLIC; vec3 EMISSION; float ALPHA; float ALPHA_SCISSOR;
vec3 NORMAL_MAP; float NORMAL_MAP_DEPTH; float AO;
)";
    for (const RtSurface& surface : surfaces) {
        std::string chunk = surface.chunk;
        replace_all(chunk, "@ID@", std::to_string(surface.slot));
        s += chunk;
    }
    s += "void astral_custom_surface(uint shader, uint base, vec2 uv, vec3 view_dir, inout Surface s) {\n    switch (shader) {\n";
    for (const RtSurface& surface : surfaces) {
        if (surface.chunk.empty()) continue;
        s += "    case " + std::to_string(surface.slot) + "u: astral_run_" + std::to_string(surface.slot) + "(base, uv, view_dir, s); break;\n";
    }
    s += "    default: break;\n    }\n}\n";
    s += "bool astral_custom_alpha_test(uint shader, uint base, vec2 uv, vec3 p, vec3 n, vec3 view_dir, bool backface) {\n    switch (shader) {\n";
    for (const RtSurface& surface : surfaces) {
        if (surface.chunk.empty() || !surface.alpha_test) continue;
        s += "    case " + std::to_string(surface.slot) + "u: return astral_alpha_" + std::to_string(surface.slot) + "(base, uv, p, n, view_dir, backface);\n";
    }
    s += "    default: return true;\n    }\n}\n";
    return s;
}

RtVariants compile_rt_variants(std::span<const RtSurface> surfaces) {
    RtVariants out;
    std::vector<GlslInclude> includes = engine_includes();
    includes.push_back({"astral/rt.glsl", embedded_text(embedded::astral_rt_glsl, embedded::astral_rt_glsl_size)});
    includes.push_back({"astral/primary.glsl", embedded_text(embedded::astral_primary_glsl, embedded::astral_primary_glsl_size)});
    includes.push_back({"astral/sdf.glsl", embedded_text(embedded::astral_sdf_glsl, embedded::astral_sdf_glsl_size)});
    includes.push_back({"astral/rt_custom.glsl", compose_rt_surfaces(surfaces)});
    bool any_alpha_test = false;
    for (const RtSurface& surface : surfaces) any_alpha_test = any_alpha_test || (surface.alpha_test && !surface.chunk.empty());

    auto build = [&](const std::uint8_t* data, std::size_t size, std::vector<std::uint32_t>& spirv, const char* name) {
        std::string text = embedded_text(data, size);
        const std::string header = "#version 460\n";
        std::string defs = "#extension GL_GOOGLE_include_directive : require\n#define ASTRAL_RT_CUSTOM\n";
        if (any_alpha_test) defs += "#define ASTRAL_RT_ALPHA_TEST\n";
        if (text.starts_with(header)) text.insert(header.size(), defs);
        GlslCompile c = compile_glsl(text, ShaderStage::Compute, includes, name);
        std::vector<ShaderError> errors;
        collect(c, errors, name);
        for (ShaderError& e : errors) {
            e.message = std::string("ray-traced variant: ") + e.message;
            out.errors.push_back(std::move(e));
        }
        out.log += c.log;
        spirv = std::move(c.spirv);
    };
    build(embedded::astral_rt_lighting_comp, embedded::astral_rt_lighting_comp_size, out.lighting, "rt_lighting");
    build(embedded::astral_ddgi_trace_comp, embedded::astral_ddgi_trace_comp_size, out.ddgi_trace, "ddgi_trace");
    return out;
}

namespace {

constexpr const char* screen_params = R"(ASTRAL_BUFFER(AstralParams, uint);
AstralParams astral_params;
uint astral_base;
float astral_pf(uint o) { return uintBitsToFloat(astral_params.data[astral_base + o]); }
uint astral_pu(uint o) { return astral_params.data[astral_base + o]; }
)";

std::string screen_vertex(bool sky) {
    std::string s = R"(#version 460
layout(location = 0) out vec2 astral_uv;
void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    astral_uv = p;
)";
    s += sky ? "    gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);\n}\n" : "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n}\n";
    return s;
}

std::string screen_fragment(const ShaderSource& source, const UniformLayout& layout, bool sky) {
    std::string s = R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/scene.glsl>
#include <astral/color.glsl>
layout(push_constant, scalar) uniform Push { FrameRef frame; Materials materials; uint material; uint scene; } pc;
layout(location = 0) in vec2 astral_uv;
layout(location = 0) out vec4 astral_out_color;
#define TIME uintBitsToFloat(pc.frame.f.custom.z)
)";
    s += screen_params;
    s += sky ? "vec3 COLOR; vec3 EYEDIR; vec3 SUN_DIR; vec3 SUN_COLOR;\n"
             : "vec3 COLOR; vec2 SCREEN_UV;\n#define SCREEN_TEXTURE sampler2D(astral_textures[pc.scene], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP])\n";
    s += wrap_body(source, layout, source.body);
    s += R"(void main() {
    Frame f = pc.frame.f;
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_base = pc.materials.data[pc.material].custom.x;
)";
    if (sky) {
        s += R"(    vec2 uv = gl_FragCoord.xy * f.screen.zw;
    EYEDIR = normalize(reconstruct_world(f, uv, 1.0) - f.camera_position.xyz);
    SUN_DIR = -normalize(f.sun_direction.xyz);
    SUN_COLOR = f.sun_color.rgb;
    COLOR = vec3(0.0);
    sky();
    astral_out_color = vec4(max(COLOR, vec3(0.0)), 1.0);
}
)";
    } else {
        s += R"(    SCREEN_UV = gl_FragCoord.xy * f.screen.zw;
    COLOR = texelFetch(sampler2D(astral_textures[pc.scene], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), ivec2(gl_FragCoord.xy), 0).rgb;
    if (f.custom.w == 0u) COLOR = astral_linear_to_srgb(COLOR); // hardware-sRGB target: show the encoded colour
    post();
    COLOR = clamp(COLOR, vec3(0.0), vec3(1.0));
    if (f.custom.w == 0u) COLOR = astral_srgb_to_linear(COLOR);
    astral_out_color = vec4(COLOR, 1.0);
}
)";
    }
    return s;
}

} // namespace

ScreenShaderBuild compile_screen_shader(const ShaderSource& source, std::string_view name) {
    ScreenShaderBuild out;
    out.type = source.type;
    if (source.type != ShaderType::Post && source.type != ShaderType::Sky) {
        out.errors.push_back({1, "not a post or sky shader (shader_type " + std::string(to_string(source.type)) + ")"});
        return out;
    }
    const bool sky = source.type == ShaderType::Sky;
    if (!source.has_function(sky ? "sky" : "post")) {
        out.errors.push_back({1, std::string("a ") + std::string(to_string(source.type)) + " shader needs a " + (sky ? "sky()" : "post()") + " function"});
        return out;
    }
    out.layout = layout_uniforms(source);
    for (std::size_t i = 0; i < source.uniforms.size(); ++i) {
        if (source.uniforms[i].type == UniformType::Sampler2D) out.samplers.push_back({out.layout.offsets[i], source.uniforms[i].hints.hint_normal});
    }
    const std::vector<GlslInclude> includes = engine_includes();
    GlslCompile frag = compile_glsl(screen_fragment(source, out.layout, sky), ShaderStage::Fragment, includes, name);
    collect(frag, out.errors, "fragment");
    GlslCompile vert = compile_glsl(screen_vertex(sky), ShaderStage::Vertex, includes, name);
    collect(vert, out.errors, "vertex");
    if (!out.errors.empty()) return out;
    out.fragment = std::move(frag.spirv);
    out.vertex = std::move(vert.spirv);
    return out;
}

} // namespace astral
