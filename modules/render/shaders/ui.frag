#version 460
#include <astral/bindless.glsl>
layout(push_constant, scalar) uniform Push {
    vec2 scale; vec2 translate; uint64_t vertices; uint64_t indices;
    int vertex_offset; uint texture; uint flags; float sdf_softness;
} pc;
layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

const uint FLAG_TEXTURE = 1u;
const uint FLAG_LINEAR_TARGET = 2u; // sRGB target: hardware encodes, so hand it linear values

vec3 srgb_to_linear(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c)); }

void main() {
    vec4 c = in_color;
    if ((pc.flags & FLAG_TEXTURE) != 0u) {
        vec4 t = textureLod(sampler2D(astral_textures[nonuniformEXT(pc.texture)], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP]), in_uv, 0.0);
        if (pc.sdf_softness > 0.0) {
            // Screen-space aware edge: the softness widens with the distance gradient (crisp when small or big).
            float w = max(pc.sdf_softness, fwidth(t.a) * 0.7);
            c.a *= smoothstep(0.5 - w, 0.5 + w, t.a);
        } else {
            c *= t;
        }
    }
    if ((pc.flags & FLAG_LINEAR_TARGET) != 0u) c.rgb = srgb_to_linear(c.rgb);
    out_color = c;
}
