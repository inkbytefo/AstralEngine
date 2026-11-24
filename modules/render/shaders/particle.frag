#version 460
#include <astral/scene.glsl>
#include <astral/color.glsl>
layout(push_constant, scalar) uniform Push {
    FrameRef frame;
    vec4 position_age, drift_duration, color_size;
    uint seed, fog, sdf_depth;
} pc;
layout(location = 0) in vec2 corner_in;
layout(location = 1) in float fade_in;
layout(location = 2) in vec3 world_in;
layout(location = 0) out vec4 color_out;
void main() {
    if (pc.sdf_depth != NO_TEXTURE) {
        float sdf_depth = texelFetch(sampler2D(astral_textures[pc.sdf_depth], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                                         ivec2(gl_FragCoord.xy), 0).r;
        if (gl_FragCoord.z >= sdf_depth) discard;
    }
    Frame f = pc.frame.f;
    float alpha = (1.0 - smoothstep(0.05, 1.0, dot(corner_in, corner_in))) * fade_in;
    float tr = fog_sample(f, pc.fog, gl_FragCoord.xy * f.screen.zw, length(world_in - f.camera_position.xyz)).a;
    vec3 light = pc.color_size.rgb * aerial_transmittance(f, world_in) * tr;
    vec3 color = astral_aces_tonemap(light * f.ground_color.a);
    if (f.custom.w != 0u) color = astral_linear_to_srgb(color);
    color_out = vec4(color, alpha);
}
