#ifndef ASTRAL_FOG_DDGI_GLSL
#define ASTRAL_FOG_DDGI_GLSL
#include <astral/ddgi_layout.glsl>

vec3 fog_probe_ambient(Frame f, uint radiance, uint distance_tex, vec3 p, vec3 sky) {
    if (radiance == NO_TEXTURE) return sky;
    ivec3 last = ivec3(f.ddgi_counts.xyz) - 1;
    vec3 grid = (p - f.ddgi_origin.xyz) / f.ddgi_spacing.xyz;
    vec3 bounded = clamp(grid, vec3(0), vec3(last));
    vec3 outside = abs(grid - bounded);
    float blend = 1.0 - smoothstep(0.0, 1.0, max(outside.x, max(outside.y, outside.z)));
    if (blend <= 0.0) return sky;

    ivec3 base = min(ivec3(floor(bounded)), last - 1);
    vec3 alpha = bounded - vec3(base);
    float max_distance = ddgi_max_dist(f);
    uint columns = f.ddgi_counts.x * f.ddgi_counts.y;
    vec3 sum = vec3(0);
    float weights = 0.0;
    for (int i = 0; i < 8; ++i) {
        ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        vec3 tri = mix(1.0 - alpha, alpha, vec3(offset));
        float w = tri.x * tri.y * tri.z;
        if (w <= 0.0) continue;
        ivec3 coord = base + offset;
        uint index = ddgi_probe_index(f, coord);
        vec3 delta = p - ddgi_probe_position(f, coord);
        float r = length(delta);
        if (r > 1e-4) {
            vec2 uv = ddgi_uv(f, index, delta / r, DDGI_DIST);
            vec2 moments = astral_sample(distance_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, uv).rg;
            w *= ddgi_visibility(moments, r / max_distance);
        }
        ivec2 texel = ivec2(index % columns, index / columns);
        vec3 value = texelFetch(sampler2D(astral_textures[radiance], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                                texel, 0).rgb;
        sum += w * value;
        weights += w;
    }
    return mix(sky, sum / max(weights, 1e-4), blend);
}
#endif
