#include "surface_shader_internal.hpp"

#include <string>

namespace astral {

namespace {

void replace_all(std::string& text, std::string_view from, std::string_view to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
}

constexpr const char* screen_params = R"(ASTRAL_BUFFER(AstralParams, uint);
AstralParams astral_params;
uint astral_base;
float astral_pf(uint o) { return uintBitsToFloat(astral_params.data[astral_base + o]); }
uint astral_pu(uint o) { return astral_params.data[astral_base + o]; }
)";

std::string volume_compute(const ShaderSource& source, const UniformLayout& layout) {
    std::string s = R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/smoke_atlas.glsl>
layout(local_size_x = 8, local_size_y = 8) in;
layout(push_constant, scalar) uniform Push {
    FrameRef frame;
    SmokeWords words;
    vec4 corner_cell;
    uint side, target, material, first_word, first_brick;
    float age;
} pc;
vec3 UVW, WORLD_POSITION, TINT, EMISSION;
float DENSITY, TIME, AGE;
)";
    s += screen_params;
    s += detail::wrap_body(source, layout, source.body);
    s += R"(void main() {
    uvec2 p = gl_GlobalInvocationID.xy;
    uint side = pc.side;
    if (p.x >= side || p.y >= side * side) return;
    uint index = p.y * side + p.x;
    if ((pc.words.data[pc.first_word + index / 32u] & (1u << (index % 32u))) == 0u) return;
    uvec3 voxel = uvec3(p.x, p.y % side, p.y / side);
    uint brick = smoke_brick(pc.words, pc.first_brick, side, voxel);
    if (brick == NO_TEXTURE) return;
    ivec2 texel = smoke_atlas_texel(brick, voxel, 2u);
    Frame f = pc.frame.f;
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_base = Materials(f.scene_materials).data[pc.material].custom.x;
    vec3 sample_cell = vec3(voxel) + 0.5;
    UVW = sample_cell / float(side);
    WORLD_POSITION = pc.corner_cell.xyz + pc.corner_cell.w * sample_cell;
    TIME = uintBitsToFloat(f.custom.z);
    AGE = pc.age;
    TINT = vec3(1.0);
    DENSITY = 1.0;
    EMISSION = vec3(0.0);
    volume();
    imageStore(astral_storage_images[pc.target], texel, vec4(clamp(TINT, 0.0, 8.0), clamp(DENSITY, 0.0, 1.0)));
    imageStore(astral_storage_images[pc.target], texel + ivec2(0, 64),
               vec4(clamp(EMISSION, 0.0, 64.0), 0.0));
}
)";
    return s;
}

ScreenShaderBuild compile_effect_shader(const ShaderSource& source, std::string_view name, const ShaderCompileServices& services) {
    ScreenShaderBuild out;
    out.type = source.type;
    const bool particle = source.type == ShaderType::Particle;
    const char* entry = particle ? "particle" : "volume";
    if (!source.has_function(entry) || !source.varyings.empty()) {
        out.errors.push_back({1, std::string("expected ") + entry + "() with no custom varyings"});
        return out;
    }
    out.layout = layout_uniforms(source);
    out.additive = !source.has_render_mode("blend_mix");
    for (std::size_t i = 0; i < source.uniforms.size(); ++i) {
        if (source.uniforms[i].type == UniformType::Sampler2D)
            out.samplers.push_back({out.layout.offsets[i], source.uniforms[i].hints.hint_normal});
    }
    std::string code;
    std::vector<GlslRequest> requests;
    if (particle) {
        code = detail::embedded_text(embedded::astral_particle_frag, embedded::astral_particle_frag_size);
        code.insert(code.find('\n') + 1, "#extension GL_GOOGLE_include_directive : require\n");
        std::string body = "#define ASTRAL_CUSTOM_PARTICLE\n";
        body += screen_params;
        body += "vec2 UV; vec3 WORLD_POSITION, COLOR; float AGE, LIFETIME, LIFE_T, ALPHA;\n";
        body += detail::wrap_body(source, out.layout, source.body_without("motion"));
        replace_all(code, "// ASTRAL_PARTICLE_BODY", body);
        if (source.has_function("motion")) {
            auto vertex = detail::embedded_text(embedded::astral_particle_vert, embedded::astral_particle_vert_size);
            vertex.insert(vertex.find('\n') + 1, "#extension GL_GOOGLE_include_directive : require\n");
            std::string motion = "#define ASTRAL_PARTICLE_MOTION\n";
            motion += screen_params;
            motion += "vec3 WORLD_POSITION, ORIGIN, WIND; float AGE, LIFETIME, LIFE_T, RANDOM, SIZE;\n";
            motion += detail::wrap_body(source, out.layout, source.body_without("particle"));
            replace_all(vertex, "// ASTRAL_PARTICLE_BODY", motion);
            requests.push_back({std::move(vertex), ShaderStage::Vertex, std::string(name)});
        }
    } else {
        code = volume_compute(source, out.layout);
    }
    requests.push_back({std::move(code), particle ? ShaderStage::Fragment : ShaderStage::Compute, std::string(name)});
    std::vector<GlslCompile> built = compile_glsl_batch(requests, detail::engine_includes(), services);
    if (requests.size() == 2) {
        detail::collect_errors(built[0], out.errors, "motion");
        out.vertex = std::move(built[0].spirv);
    }
    detail::collect_errors(built.back(), out.errors, entry);
    (particle ? out.fragment : out.compute) = std::move(built.back().spirv);
    return out;
}

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

std::string screen_fragment(const ShaderSource& source, const UniformLayout& layout, bool sky, bool capture = false) {
    std::string s = R"(#version 460
#extension GL_GOOGLE_include_directive : require
#include <astral/scene.glsl>
#include <astral/color.glsl>
#include <astral/sky_ibl.glsl>
layout(push_constant, scalar) uniform Push { FrameRef frame; Materials materials; uint material; uint scene; } pc;
layout(location = 0) in vec2 astral_uv;
layout(location = 0) out vec4 astral_out_color;
#define TIME uintBitsToFloat(pc.frame.f.custom.z)
)";
    s += screen_params;
    s += sky ? "vec3 COLOR; vec3 EYEDIR; vec3 SUN_DIR; vec3 SUN_COLOR;\n"
             : "vec3 COLOR; vec2 SCREEN_UV;\n#define SCREEN_TEXTURE sampler2D(astral_textures[pc.scene], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP])\n";
    if (sky) s += capture ? "const bool IBL_CAPTURE = true;\n" : "const bool IBL_CAPTURE = false;\n";
    s += detail::wrap_body(source, layout, source.body);
    s += R"(void main() {
    Frame f = pc.frame.f;
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_base = pc.materials.data[pc.material].custom.x;
)";
    if (sky) {
        s += capture ? "    EYEDIR = sky_direction(astral_uv);\n" : R"(    if (f.counts.y > 0u && pc.scene != NO_TEXTURE) {
        float sd = texelFetch(sampler2D(astral_textures[pc.scene], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                              ivec2(gl_FragCoord.xy), 0).r;
        if (sd < 1.0) discard;
    }
    vec2 uv = gl_FragCoord.xy * f.screen.zw;
    EYEDIR = normalize(reconstruct_world(f, uv, 1.0) - f.camera_position.xyz);
)";
        s += R"(
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
    if (f.custom.w == 0u) COLOR = astral_linear_to_srgb(COLOR);
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

std::string compose_rt_surfaces(std::span<const RtSurface> surfaces) {
    std::string s = "// Generated: the custom surface() functions for ray-traced hits.\n";
    s += detail::kSurfaceBuiltinsDecl;
    for (const RtSurface& surface : surfaces) {
        std::string chunk = surface.chunk;
        replace_all(chunk, "@ID@", std::to_string(surface.slot));
        s += chunk;
    }
    s += "void astral_custom_surface(uint shader, uint base, vec2 uv, vec2 uv1, vec3 view_dir, inout Surface s) {\n    switch (shader) {\n";
    for (const RtSurface& surface : surfaces) {
        if (surface.chunk.empty()) continue;
        s += "    case " + std::to_string(surface.slot) + "u: astral_run_" + std::to_string(surface.slot) + "(base, uv, uv1, view_dir, s); break;\n";
    }
    s += "    default: break;\n    }\n}\n";
    s += "bool astral_custom_alpha_test(uint shader, uint base, vec2 uv, vec2 uv1, vec3 p, vec3 n, vec3 view_dir, bool backface) {\n    switch (shader) {\n";
    for (const RtSurface& surface : surfaces) {
        if (surface.chunk.empty() || !surface.alpha_test) continue;
        s += "    case " + std::to_string(surface.slot) + "u: return astral_alpha_" + std::to_string(surface.slot) + "(base, uv, uv1, p, n, view_dir, backface);\n";
    }
    s += "    default: return true;\n    }\n}\n";
    return s;
}

RtVariants compile_rt_variants(std::span<const RtSurface> surfaces, const ShaderCompileServices& services) {
    RtVariants out;
    std::vector<GlslInclude> includes = detail::engine_includes();
    includes.push_back({"astral/rt.glsl", detail::embedded_text(embedded::astral_rt_glsl, embedded::astral_rt_glsl_size)});
    includes.push_back({"astral/projection.glsl", detail::embedded_text(embedded::astral_projection_glsl, embedded::astral_projection_glsl_size)});
    includes.push_back({"astral/restir.glsl", detail::embedded_text(embedded::astral_restir_glsl, embedded::astral_restir_glsl_size)});
    includes.push_back({"astral/primary.glsl", detail::embedded_text(embedded::astral_primary_glsl, embedded::astral_primary_glsl_size)});
    includes.push_back({"astral/sdf.glsl", detail::embedded_text(embedded::astral_sdf_glsl, embedded::astral_sdf_glsl_size)});
    includes.push_back({"astral/rt_custom.glsl", compose_rt_surfaces(surfaces)});
    bool any_alpha_test = false;
    for (const RtSurface& surface : surfaces) any_alpha_test = any_alpha_test || (surface.alpha_test && !surface.chunk.empty());

    struct Variant {
        const std::uint8_t* data;
        std::size_t size;
        const char* name;
        std::vector<std::uint32_t>* spirv;
    };
    const Variant variants[] = {
        {embedded::astral_rt_lighting_comp, embedded::astral_rt_lighting_comp_size, "rt_lighting", &out.lighting},
        {embedded::astral_ddgi_trace_comp, embedded::astral_ddgi_trace_comp_size, "ddgi_trace", &out.ddgi_trace},
        {embedded::astral_restir_di_temporal_comp, embedded::astral_restir_di_temporal_comp_size, "restir_di_temporal",
         &out.restir_temporal},
        {embedded::astral_restir_di_spatial_comp, embedded::astral_restir_di_spatial_comp_size, "restir_di_spatial",
         &out.restir_spatial},
        {embedded::astral_path_tracer_comp, embedded::astral_path_tracer_comp_size, "path_tracer", &out.path_tracer},
    };
    std::vector<GlslRequest> requests;
    for (const Variant& v : variants) {
        std::string text = detail::embedded_text(v.data, v.size);
        const std::string header = "#version 460\n";
        std::string defs = "#extension GL_GOOGLE_include_directive : require\n#define ASTRAL_RT_CUSTOM\n";
        if (any_alpha_test) defs += "#define ASTRAL_RT_ALPHA_TEST\n";
        if (text.starts_with(header)) text.insert(header.size(), defs);
        requests.push_back({std::move(text), ShaderStage::Compute, v.name});
    }
    // Five independent compute shaders: the slowest compiles in the engine, so run them side by side.
    std::vector<GlslCompile> built = compile_glsl_batch(requests, includes, services);
    for (std::size_t i = 0; i < built.size(); ++i) {
        std::vector<ShaderError> errors;
        detail::collect_errors(built[i], errors, variants[i].name);
        for (ShaderError& e : errors) {
            e.message = std::string("ray-traced variant: ") + e.message;
            out.errors.push_back(std::move(e));
        }
        out.log += built[i].log;
        *variants[i].spirv = std::move(built[i].spirv);
    }
    return out;
}

ScreenShaderBuild compile_screen_shader(const ShaderSource& source, std::string_view name, const ShaderCompileServices& services) {
    if (source.type == ShaderType::Particle || source.type == ShaderType::Volume) return compile_effect_shader(source, name, services);
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
    const std::vector<GlslInclude> includes = detail::engine_includes();
    std::vector<GlslRequest> requests = {{screen_fragment(source, out.layout, sky), ShaderStage::Fragment, std::string(name)},
                                         {screen_vertex(sky), ShaderStage::Vertex, std::string(name)}};
    if (sky) requests.push_back({screen_fragment(source, out.layout, true, true), ShaderStage::Fragment, std::string(name)});
    std::vector<GlslCompile> built = compile_glsl_batch(requests, includes, services);
    detail::collect_errors(built[0], out.errors, "fragment");
    detail::collect_errors(built[1], out.errors, "vertex");
    if (!out.errors.empty()) return out;
    out.fragment = std::move(built[0].spirv);
    out.vertex = std::move(built[1].spirv);
    if (sky) {
        detail::collect_errors(built[2], out.errors, "sky capture");
        out.sky_capture = std::move(built[2].spirv);
        out.sky_uses_time = source.body.find("TIME") != std::string::npos;
    }
    return out;
}

} // namespace astral
