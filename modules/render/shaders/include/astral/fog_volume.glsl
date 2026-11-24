#ifndef ASTRAL_FOG_VOLUME_GLSL
#define ASTRAL_FOG_VOLUME_GLSL
#include <astral/smoke_atlas.glsl>

struct SmokeVolume {
    vec3 corner;
    float cell_size;
    float extinction;
    uint side, first_word, first_brick, modulation, sun_shadow;
};
ASTRAL_BUFFER(SmokeVolumes, SmokeVolume);

vec4 smoke_texel(uint index, ivec2 texel) {
    return texelFetch(sampler2D(astral_textures[nonuniformEXT(index)], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]),
                      texel, 0);
}

bool smoke_occupied(SmokeVolume v, SmokeWords words, uint index) {
    uint word = words.data[v.first_word + index / 32u];
    return (word & (1u << (index % 32u))) != 0u;
}

vec4 march_smoke(SmokeVolume v, SmokeWords words, vec3 eye, vec3 ray, float enter, float exit,
                bool shadow_ray, out vec3 emitted, out vec3 sunlit) {
    emitted = vec3(0.0);
    sunlit = vec3(0.0);
    vec3 origin = (eye - v.corner) / v.cell_size;
    vec3 dir = ray / v.cell_size;
    for (int a = 0; a < 3; ++a) {
        if (abs(dir[a]) < 1e-8) {
            if (origin[a] < 0.0 || origin[a] >= float(v.side)) return vec4(0.0);
        } else {
            float t0 = -origin[a] / dir[a];
            float t1 = (float(v.side) - origin[a]) / dir[a];
            enter = max(enter, min(t0, t1));
            exit = min(exit, max(t0, t1));
        }
    }
    if (enter >= exit) return vec4(0.0);

    vec3 p = clamp(origin + dir * enter, vec3(0), vec3(v.side));
    ivec3 step_dir = ivec3(greaterThanEqual(dir, vec3(0))) * 2 - 1;
    ivec3 cell = clamp(ivec3(mix(ceil(p) - 1.0, floor(p), greaterThanEqual(dir, vec3(0)))),
                       ivec3(0), ivec3(v.side - 1u));
    vec3 edge = vec3(1e30);
    for (int a = 0; a < 3; ++a) {
        if (abs(dir[a]) >= 1e-8) edge[a] = (float(cell[a] + (step_dir[a] > 0 ? 1 : 0)) - origin[a]) / dir[a];
    }
    vec4 filled = vec4(0.0);
    for (uint i = 0u; i < v.side * 3u && enter < exit; ++i) {
        float next = min(exit, min(edge.x, min(edge.y, edge.z)));
        uint index = (uint(cell.z) * v.side + uint(cell.y)) * v.side + uint(cell.x);
        if (smoke_occupied(v, words, index)) {
            uint brick = v.first_brick == NO_TEXTURE ? NO_TEXTURE : smoke_brick(words, v.first_brick, v.side, uvec3(cell));
            ivec2 texel = brick == NO_TEXTURE ? ivec2(0) : smoke_atlas_texel(brick, uvec3(cell), 2u);
            vec4 material = v.modulation == NO_TEXTURE ? vec4(1) : smoke_texel(v.modulation, texel);
            float distance_m = max(0.0, next - enter);
            vec3 tint = material.rgb * (material.a * distance_m);
            filled += vec4(tint, material.a * distance_m);
            if (!shadow_ray) {
                if (v.modulation != NO_TEXTURE)
                    emitted += distance_m * smoke_texel(v.modulation, texel + ivec2(0, 64)).rgb;
                if (material.a > 0.0) {
                    float light = v.sun_shadow == NO_TEXTURE ? 1.0 :
                        smoke_texel(v.sun_shadow, smoke_atlas_texel(brick, uvec3(cell), 1u)).r;
                    sunlit += light * tint;
                }
            }
        }
        if (next >= exit) break;
        for (int a = 0; a < 3; ++a) {
            if (edge[a] > next) continue;
            cell[a] += step_dir[a];
            if (cell[a] < 0 || cell[a] >= int(v.side)) return filled;
            edge[a] += 1.0 / abs(dir[a]);
        }
        enter = next;
    }
    return filled;
}

#endif
