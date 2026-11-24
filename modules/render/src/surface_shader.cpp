#include "surface_shader_internal.hpp"

#include <algorithm>
#include <string>

namespace astral::detail {

std::string embedded_text(const std::uint8_t* data, std::size_t size) {
    std::string text(reinterpret_cast<const char*>(data), size);
    std::erase(text, '\r');
    return text;
}

std::vector<GlslInclude> engine_includes() {
    return {{"astral/bindless.glsl", embedded_text(embedded::astral_bindless_glsl, embedded::astral_bindless_glsl_size)},
            {"astral/scene.glsl", embedded_text(embedded::astral_scene_glsl, embedded::astral_scene_glsl_size)},
            {"astral/color.glsl", embedded_text(embedded::astral_color_glsl, embedded::astral_color_glsl_size)},
            {"astral/brdf.glsl", embedded_text(embedded::astral_brdf_glsl, embedded::astral_brdf_glsl_size)},
            {"astral/bsdf.glsl", embedded_text(embedded::astral_bsdf_glsl, embedded::astral_bsdf_glsl_size)}};
}

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
            const std::string index = "astral_pu(" + o + ")";
            const std::string image = "astral_textures[nonuniformEXT(" + index + ")]";
            s += "#define astral_image_" + u.name + " " + index + "\n";
            value = "sampler2D(" + image + ", astral_samplers[" + sampler + "])";
            break;
        }
        }
        s += "#define " + u.name + " " + value + "\n";
    }
    return s;
}

std::string wrap_body(const ShaderSource& source, const UniformLayout& layout, const std::string& body) {
    std::string undefs;
    for (const ShaderUniform& u : source.uniforms) {
        undefs += "#undef " + u.name + "\n";
        if (u.type == UniformType::Sampler2D) undefs += "#undef astral_image_" + u.name + "\n";
    }
    return uniform_macros(source, layout) + "#line 1\n" + body + "\n#line 100000\n" + undefs;
}

void collect_errors(const GlslCompile& c, std::vector<ShaderError>& errors, const char* stage) {
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

} // namespace astral::detail

namespace astral {

namespace {

std::string prelude() {
    return R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/scene.glsl>
#include <astral/color.glsl>
#include <astral/bsdf.glsl>
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
}

const char* glsl_shading_model(ShadingModel model) {
    switch (model) {
    case ShadingModel::Standard: return "SHADING_STANDARD";
    case ShadingModel::Clearcoat: return "SHADING_CLEARCOAT";
    case ShadingModel::Sheen: return "SHADING_SHEEN";
    case ShadingModel::Anisotropic: return "SHADING_ANISOTROPIC";
    case ShadingModel::Transmission: return "SHADING_TRANSMISSION";
    case ShadingModel::Subsurface: return "SHADING_SUBSURFACE";
    }
    return "SHADING_STANDARD";
}

enum class Mode { GBuffer, Shadow, Forward };

std::string vertex_stage(const ShaderSource& source, const UniformLayout& layout, Mode mode) {
    std::string s = prelude();
    s += R"(layout(location = 0) out vec3 astral_out_normal;
layout(location = 1) flat out uint astral_out_instance;
layout(location = 2) out vec4 astral_out_clip_now;
layout(location = 3) out vec4 astral_out_clip_prev;
layout(location = 4) out vec2 astral_out_uv;
layout(location = 5) out vec3 astral_out_world;
layout(location = 6) out vec2 astral_out_uv1;
vec3 VERTEX; vec3 NORMAL; vec2 UV; vec2 UV1; vec3 WORLD_POSITION; mat4 MODEL_MATRIX;
)";
    for (std::size_t i = 0; i < source.varyings.size(); ++i) {
        s += "layout(location = " + std::to_string(7 + i) + ") out " + source.varyings[i].type + " " + source.varyings[i].name + ";\n";
    }
    s += detail::wrap_body(source, layout, source.body_without("surface"));
    s += R"(void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    uint id = pc.first_instance + gl_InstanceIndex;
    Instance inst = pc.instances.data[id];
    astral_params = AstralParams(pc.params);
    astral_base = pc.materials.data[inst.material].custom.x;
    VERTEX = v.position;
    NORMAL = v.normal;
    UV = v.uv; UV1 = v.uv1;
    MODEL_MATRIX = inst.model;
    WORLD_POSITION = (inst.model * vec4(v.position, 1.0)).xyz;
)";
    if (source.has_function("vertex")) s += "    vertex();\n";
    s += R"(    vec4 world = inst.model * vec4(VERTEX, 1.0);
    Frame f = pc.frame.f;
)";
    s += mode == Mode::Shadow    ? "    gl_Position = f.light_view_projection * world;\n"
         : mode == Mode::Forward ? "    gl_Position = f.unjittered_view_projection * world;\n"
                                 : "    gl_Position = f.view_projection * world;\n";
    s += R"(    astral_out_normal = normal_matrix_of(inst.model) * NORMAL;
    astral_out_instance = id;
    astral_out_clip_now = f.unjittered_view_projection * world;
    astral_out_clip_prev = f.prev_view_projection * (inst.prev_model * vec4(VERTEX, 1.0));
    astral_out_uv = UV;
    astral_out_uv1 = UV1;
    astral_out_world = world.xyz;
}
)";
    return s;
}

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

std::string fragment_output(Mode mode, bool unshaded, ShadingModel shading_model) {
    if (mode == Mode::Forward) {
        std::string s = unshaded ? "    vec3 lit = ALBEDO + EMISSION;\n"
                                 : R"(    vec3 n = NORMAL;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) n = astral_perturb(n, NORMAL_MAP * 2.0 - 1.0, NORMAL_MAP_DEPTH);
    vec3 lit = astral_lit(n, VIEW_DIR, WORLD_POSITION, ALBEDO * AO, ROUGHNESS, METALLIC) + EMISSION;
)";
        s += R"(    astral_out_color = vec4(lit, clamp(ALPHA, 0.0, 1.0));
)";
        return s;
    }
    if (mode != Mode::GBuffer) return {};
    if (unshaded) {
        return R"(    astral_out_albedo = vec4(ALBEDO, 1.0);
    astral_out_normal = vec4(NORMAL, 0.0);
    astral_out_emissive = vec4(ALBEDO + EMISSION, float(SHADING_UNSHADED + 1u));
    astral_out_velocity = compute_velocity(astral_in_clip_now, astral_in_clip_prev);
    astral_out_material_ext = vec4(0.0);
)";
    }
    std::string s = R"(    vec3 n = NORMAL;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) n = astral_perturb(n, NORMAL_MAP * 2.0 - 1.0, NORMAL_MAP_DEPTH);
    vec3 a_col = ALBEDO * AO;
    float r_val = clamp(ROUGHNESS, ASTRAL_MIN_ROUGHNESS, 1.0);
    float m_val = clamp(METALLIC, 0.0, 1.0);
    float coat_v = CLEARCOAT, coat_r = CLEARCOAT_ROUGHNESS;
    astral_apply_wetness(a_col, r_val, coat_v, coat_r, m_val, WETNESS);
    uint model = )";
    s += glsl_shading_model(shading_model);
    s += R"(;
    if (model == SHADING_STANDARD && coat_v > 1e-4) model = SHADING_CLEARCOAT;
    vec4 ext = vec4(0.0);
    if (model == SHADING_CLEARCOAT) ext = vec4(clamp(coat_v, 0.0, 1.0), clamp(coat_r, ASTRAL_MIN_ROUGHNESS, 1.0), 0.0, 0.0);
    else if (model == SHADING_SHEEN) ext = vec4(max(SHEEN_COLOR, vec3(0.0)), clamp(SHEEN_ROUGHNESS, 0.05, 1.0));
    else if (model == SHADING_ANISOTROPIC) ext = vec4(bsdf_default_tangent(n, ANISOTROPY_ROTATION), clamp(ANISOTROPY, -0.95, 0.95));
    else if (model == SHADING_TRANSMISSION) { a_col = mix(a_col, max(ATTENUATION_COLOR, vec3(1e-4)), clamp(TRANSMISSION, 0.0, 1.0)); ext = vec4(clamp(TRANSMISSION, 0.0, 1.0), max(IOR, 1.01), max(THICKNESS, 0.001), max(ATTENUATION_DISTANCE, 0.001)); }
    else if (model == SHADING_SUBSURFACE) ext = vec4(max(SUBSURFACE_COLOR, vec3(0.0)), clamp(SUBSURFACE, 0.0, 1.0));
    astral_out_albedo = vec4(a_col, r_val);
    astral_out_normal = vec4(n, m_val);
    astral_out_emissive = vec4(EMISSION, float(model + 1u));
    astral_out_velocity = compute_velocity(astral_in_clip_now, astral_in_clip_prev);
    astral_out_material_ext = ext;
)";
    return s;
}

std::string fragment_stage(const ShaderSource& source, const UniformLayout& layout, Mode mode) {
    std::string s = prelude();
    s += R"(layout(location = 0) in vec3 astral_in_normal;
layout(location = 1) flat in uint astral_in_instance;
layout(location = 2) in vec4 astral_in_clip_now;
layout(location = 3) in vec4 astral_in_clip_prev;
layout(location = 4) in vec2 astral_in_uv;
layout(location = 5) in vec3 astral_in_world;
layout(location = 6) in vec2 astral_in_uv1;
)";
    if (mode == Mode::GBuffer) {
        s += R"(layout(location = 0) out vec4 astral_out_albedo;
layout(location = 1) out vec4 astral_out_normal;
layout(location = 2) out vec4 astral_out_emissive;
layout(location = 3) out vec2 astral_out_velocity;
layout(location = 4) out vec4 astral_out_material_ext;
)";
    } else if (mode == Mode::Forward) {
        s += "layout(location = 0) out vec4 astral_out_color;\n";
    }
    for (std::size_t i = 0; i < source.varyings.size(); ++i) {
        s += "layout(location = " + std::to_string(7 + i) + ") in " + source.varyings[i].type + " " + source.varyings[i].name + ";\n";
    }
    s += detail::kSurfaceBuiltinsDecl;
    const bool unshaded = source.has_render_mode("unshaded");
    if (mode == Mode::Forward && !unshaded) s += forward_lighting;
    s += detail::wrap_body(source, layout, source.body_without("vertex"));
    s += R"(vec3 astral_perturb(vec3 n, vec3 map, float scale) {
    vec3 dp1 = dFdx(astral_in_world), dp2 = dFdy(astral_in_world);
    vec2 duv1 = dFdx(NORMAL_UV), duv2 = dFdy(NORMAL_UV);
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
    UV = astral_in_uv; UV1 = astral_in_uv1; NORMAL_UV = UV;
    NORMAL = normalize(astral_in_normal);
    FRONT_FACING = gl_FrontFacing;
    if (!gl_FrontFacing) NORMAL = -NORMAL;
    WORLD_POSITION = astral_in_world;
    VIEW_DIR = normalize(f.camera_position.xyz - astral_in_world);
    SCREEN_UV = gl_FragCoord.xy * f.screen.zw;
)";
    s += detail::kSurfaceBuiltinsInit;
    if (source.has_function("surface")) s += "    surface();\n";
    if (source.has_render_mode("alpha_test")) s += "    if (ALPHA < ALPHA_SCISSOR) discard;\n";
    s += fragment_output(mode, unshaded, source.shading_model);
    s += "}\n";
    return s;
}

std::string rt_chunk(const ShaderSource& source, const UniformLayout& layout) {
    if (!source.has_function("surface")) return {};
    const std::string prefix = "astral_s@ID@_";
    std::string s = "// ---- shader slot @ID@\n";
    for (const std::string& f : source.functions) s += "#define " + f + " " + prefix + f + "\n";
    for (const ShaderVarying& v : source.varyings) s += "#define " + v.name + " " + prefix + v.name + "\n" + v.type + " " + v.name + ";\n";
    s += "#define ASTRAL_RT_CHUNK 1\n#define texture(s, uv) textureLod(s, uv, 2.0)\n#define dFdx(x) ((x) * 0.0)\n#define dFdy(x) ((x) * 0.0)\n";
    s += detail::wrap_body(source, layout, source.body_without("vertex"));
    s += R"(void astral_run_@ID@(uint base, vec2 uv, vec2 uv1, vec3 view_dir, inout Surface s) {
    astral_base = base;
    UV = uv; UV1 = uv1; NORMAL_UV = uv; NORMAL = s.n; WORLD_POSITION = s.p; VIEW_DIR = view_dir; FRONT_FACING = !s.backface; SCREEN_UV = vec2(0.0);
)";
    s += detail::kSurfaceBuiltinsInit;
    s += "    surface();\n";
    if (source.has_render_mode("unshaded")) {
        s += R"(    s.albedo = ALBEDO; s.roughness = 1.0; s.metallic = 0.0; s.emissive = EMISSION;
    s.unshaded = true; s.shading_model = SHADING_UNSHADED; s.mat_ext = vec4(0.0);
}
)";
    } else {
        s += R"(    vec3 a_col = ALBEDO * AO;
    float r_val = clamp(ROUGHNESS, ASTRAL_MIN_ROUGHNESS, 1.0);
    float m_val = clamp(METALLIC, 0.0, 1.0);
    float coat_v = CLEARCOAT, coat_r = CLEARCOAT_ROUGHNESS;
    astral_apply_wetness(a_col, r_val, coat_v, coat_r, m_val, WETNESS);
    uint model = )";
        s += glsl_shading_model(source.shading_model);
        s += R"(;
    if (model == SHADING_STANDARD && coat_v > 1e-4) model = SHADING_CLEARCOAT;
    s.albedo = a_col; s.roughness = r_val; s.metallic = m_val; s.emissive = EMISSION;
    s.unshaded = false; s.shading_model = model; s.mat_ext = vec4(0.0);
    s.attenuation_color = max(ATTENUATION_COLOR, vec3(1e-4)); s.sss_radius = max(SUBSURFACE_RADIUS, 0.01);
    if (model == SHADING_CLEARCOAT) s.mat_ext = vec4(clamp(coat_v, 0.0, 1.0), clamp(coat_r, ASTRAL_MIN_ROUGHNESS, 1.0), 0.0, 0.0);
    else if (model == SHADING_SHEEN) s.mat_ext = vec4(max(SHEEN_COLOR, vec3(0.0)), clamp(SHEEN_ROUGHNESS, 0.05, 1.0));
    else if (model == SHADING_ANISOTROPIC) s.mat_ext = vec4(bsdf_default_tangent(s.n, ANISOTROPY_ROTATION), clamp(ANISOTROPY, -0.95, 0.95));
    else if (model == SHADING_TRANSMISSION) s.mat_ext = vec4(clamp(TRANSMISSION, 0.0, 1.0), max(IOR, 1.01), max(THICKNESS, 0.001), max(ATTENUATION_DISTANCE, 0.001));
    else if (model == SHADING_SUBSURFACE) s.mat_ext = vec4(max(SUBSURFACE_COLOR, vec3(0.0)), clamp(SUBSURFACE, 0.0, 1.0));
}
)";
    }
    if (source.has_render_mode("alpha_test")) {
        s += R"(bool astral_alpha_@ID@(uint base, vec2 uv, vec2 uv1, vec3 p, vec3 n, vec3 view_dir, bool backface) {
    astral_base = base;
    UV = uv; UV1 = uv1; NORMAL_UV = uv; NORMAL = n; WORLD_POSITION = p; VIEW_DIR = view_dir; FRONT_FACING = !backface; SCREEN_UV = vec2(0.0);
)";
        s += detail::kSurfaceBuiltinsInit;
        s += "    surface();\n    return ALPHA >= ALPHA_SCISSOR;\n}\n";
    }
    s += "#undef ASTRAL_RT_CHUNK\n#undef texture\n#undef dFdx\n#undef dFdy\n";
    for (const ShaderVarying& v : source.varyings) s += "#undef " + v.name + "\n";
    for (const std::string& f : source.functions) s += "#undef " + f + "\n";
    return s;
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
    const std::vector<GlslInclude> includes = detail::engine_includes();

    if (source.has_render_mode("blend_mix") || source.has_render_mode("blend_add")) {
        out.blend = true;
        out.additive = source.has_render_mode("blend_add");
        out.rt_chunk.clear();
        GlslCompile forward_frag = compile_glsl(fragment_stage(source, out.layout, Mode::Forward), ShaderStage::Fragment, includes, name);
        detail::collect_errors(forward_frag, out.errors, "fragment");
        GlslCompile forward_vert = compile_glsl(vertex_stage(source, out.layout, Mode::Forward), ShaderStage::Vertex, includes, name);
        detail::collect_errors(forward_vert, out.errors, "vertex");
        if (out.errors.empty()) {
            out.forward_fragment = std::move(forward_frag.spirv);
            out.forward_vertex = std::move(forward_vert.spirv);
        }
        return out;
    }

    GlslCompile frag = compile_glsl(fragment_stage(source, out.layout, Mode::GBuffer), ShaderStage::Fragment, includes, name);
    detail::collect_errors(frag, out.errors, "fragment");
    GlslCompile vert = compile_glsl(vertex_stage(source, out.layout, Mode::GBuffer), ShaderStage::Vertex, includes, name);
    detail::collect_errors(vert, out.errors, "vertex");
    if (!out.errors.empty()) return out;
    out.gbuffer_fragment = std::move(frag.spirv);
    out.gbuffer_vertex = std::move(vert.spirv);

    GlslCompile shadow_vert = compile_glsl(vertex_stage(source, out.layout, Mode::Shadow), ShaderStage::Vertex, includes, name);
    detail::collect_errors(shadow_vert, out.errors, "shadow vertex");
    out.shadow_vertex = std::move(shadow_vert.spirv);
    if (source.has_render_mode("alpha_test")) {
        GlslCompile shadow_frag = compile_glsl(fragment_stage(source, out.layout, Mode::Shadow), ShaderStage::Fragment, includes, name);
        detail::collect_errors(shadow_frag, out.errors, "shadow fragment");
        out.shadow_fragment = std::move(shadow_frag.spirv);
    }
    if (!out.errors.empty()) {
        out.gbuffer_fragment.clear();
        out.gbuffer_vertex.clear();
    }
    return out;
}

} // namespace astral
