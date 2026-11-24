#version 460
#include <astral/bindless.glsl>

layout(push_constant, scalar) uniform Push { uint sdf_depth; } pc;

void main() {
    gl_FragDepth = texelFetch(sampler2D(astral_textures[pc.sdf_depth],
                                        astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                              ivec2(gl_FragCoord.xy), 0).r;
}
