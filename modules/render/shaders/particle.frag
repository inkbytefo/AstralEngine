#version 460
#include <astral/scene.glsl>
#include <astral/color.glsl>
#include <astral/brdf.glsl>
#include <astral/ddgi_layout.glsl>
layout(push_constant, scalar) uniform Push {
    FrameRef frame;
    vec4 position_age, drift_duration, color_size;
    uint seed, fog, sdf_depth, material;
    uint mesh_depth;
    float soft_distance_m;
    uint64_t states;
    uint irradiance, distance;
    float lighting;
} pc;
layout(location = 0) in vec2 corner_in;
layout(location = 1) in float fade_in;
layout(location = 2) in vec3 world_in;
layout(location = 3) in float life_in;
layout(location = 0) out vec4 color_out;
// ASTRAL_PARTICLE_BODY
float depth_at(uint index) {
    return texelFetch(sampler2D(astral_textures[index], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                      ivec2(gl_FragCoord.xy), 0).r;
}

void main() {
    float sdf_depth = pc.sdf_depth == NO_TEXTURE ? 1.0 : depth_at(pc.sdf_depth);
    if (gl_FragCoord.z >= sdf_depth) discard;
    Frame f = pc.frame.f;
    float alpha = (1.0 - smoothstep(0.05, 1.0, dot(corner_in, corner_in))) * fade_in;
    vec2 screen_uv = gl_FragCoord.xy * f.screen.zw;
    float tr = fog_sample(f, pc.fog, screen_uv, length(world_in - f.camera_position.xyz)).a;
    vec3 radiance = pc.color_size.rgb;
#ifdef ASTRAL_CUSTOM_PARTICLE
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_base = Materials(f.scene_materials).data[pc.material].custom.x;
    UV = corner_in * 0.5 + 0.5;
    WORLD_POSITION = world_in;
    AGE = pc.position_age.w;
    LIFETIME = pc.drift_duration.w;
    LIFE_T = life_in;
    COLOR = radiance;
    ALPHA = alpha;
    particle();
    radiance = max(COLOR, vec3(0));
    alpha = clamp(ALPHA, 0.0, 1.0);
#endif
    if (pc.soft_distance_m > 0.0) {
        float surface_depth = min(sdf_depth, depth_at(pc.mesh_depth));
        if (surface_depth < 1.0) {
            vec3 surface = reconstruct_world(f, screen_uv, surface_depth);
            vec3 particle_position = reconstruct_world(f, screen_uv, gl_FragCoord.z);
            alpha *= smoothstep(0.0, pc.soft_distance_m, length(surface - particle_position));
        }
    }
    if (pc.lighting > 0.0) {
        vec3 to_camera = normalize(f.camera_position.xyz - world_in);
        vec3 ambient = sky_diffuse_ibl(f, to_camera) * f.sky_color.a;
        if (pc.irradiance != NO_TEXTURE)
            ambient = ddgi_irradiance(f, pc.irradiance, pc.distance, world_in, to_camera, to_camera);
        radiance *= mix(vec3(1), ambient, pc.lighting);
    }
    vec3 light = radiance * aerial_transmittance(f, world_in) * tr;
    vec3 color = astral_aces_tonemap(light * f.ground_color.a);
    if (f.custom.w != 0u) color = astral_linear_to_srgb(color);
    color_out = vec4(color, alpha);
}
