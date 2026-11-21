#version 460
#include <astral/bindless.glsl>
// HDR -> display: optional bloom mix, exposure, ACES filmic curve, gamma for UNORM targets.
layout(push_constant) uniform Push { uint hdr; float exposure; uint encode_srgb; uint bloom; float bloom_strength; } pc;
layout(location = 0) out vec4 out_color;

vec3 aces(vec3 x) { // Narkowicz fit
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec3 hdr = texelFetch(sampler2D(astral_textures[pc.hdr], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                          ivec2(gl_FragCoord.xy), 0).rgb;
    if (pc.bloom != 0xFFFFFFFFu) {
        ivec2 size = textureSize(sampler2D(astral_textures[pc.hdr], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), 0);
        vec3 glow = astral_sample(pc.bloom, ASTRAL_SAMPLER_LINEAR_CLAMP, gl_FragCoord.xy / vec2(size)).rgb;
        hdr = mix(hdr, glow, pc.bloom_strength);
    }
    vec3 c = aces(hdr * pc.exposure);
    // sRGB targets encode in hardware; UNORM targets need it here.
    if (pc.encode_srgb != 0u) c = pow(c, vec3(1.0 / 2.2));
    out_color = vec4(c, 1.0);
}
