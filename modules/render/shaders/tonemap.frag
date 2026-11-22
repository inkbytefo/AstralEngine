#version 460
#include <astral/bindless.glsl>
#include <astral/color.glsl>
// HDR -> display: energy-conserving bloom mix, exposure, ACES filmic curve, exact sRGB for UNORM targets.
layout(push_constant) uniform Push { uint hdr; float exposure; uint encode_srgb; uint bloom; float bloom_strength; } pc;
layout(location = 0) out vec4 out_color;

void main() {
    vec3 hdr = texelFetch(sampler2D(astral_textures[pc.hdr], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                          ivec2(gl_FragCoord.xy), 0).rgb;
    if (pc.bloom != 0xFFFFFFFFu) {
        ivec2 size = textureSize(sampler2D(astral_textures[pc.hdr], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), 0);
        vec3 glow = astral_sample(pc.bloom, ASTRAL_SAMPLER_LINEAR_CLAMP, gl_FragCoord.xy / vec2(size)).rgb;
        hdr = mix(hdr, glow, pc.bloom_strength);
    }
    vec3 c = astral_aces_tonemap(hdr * pc.exposure);
    // sRGB targets encode in hardware; UNORM targets use the matching IEC 61966-2-1 transfer function.
    if (pc.encode_srgb != 0u) c = astral_linear_to_srgb(c);
    out_color = vec4(c, 1.0);
}
