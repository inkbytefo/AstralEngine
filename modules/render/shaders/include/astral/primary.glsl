// Primary surface of a pixel: the nearer of the mesh G-buffer and the SDF G-buffer.
#ifndef ASTRAL_PRIMARY_GLSL
#define ASTRAL_PRIMARY_GLSL
#include <astral/rt.glsl>

struct GBufferIds { uint albedo, normal, emissive, depth, sdf_albedo, sdf_normal, sdf_depth; };

vec4 fetch_texel(uint index, ivec2 p) {
    return texelFetch(sampler2D(astral_textures[index], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), p, 0);
}

// Returns false for sky pixels. `depth` is the device depth used for reconstruction.
bool load_primary(Frame f, GBufferIds g, ivec2 pixel, out Surface s, out float depth) {
    depth = fetch_texel(g.depth, pixel).r;
    vec4 albedo_r = fetch_texel(g.albedo, pixel);
    vec4 normal_m = fetch_texel(g.normal, pixel);
    vec3 emissive = fetch_texel(g.emissive, pixel).rgb;
    if (f.counts.y > 0u) {
        float sd = fetch_texel(g.sdf_depth, pixel).r;
        if (sd < depth) {
            depth = sd;
            albedo_r = fetch_texel(g.sdf_albedo, pixel);
            normal_m = fetch_texel(g.sdf_normal, pixel);
            emissive = vec3(0);
        }
    }
    if (depth >= 1.0) return false;
    vec2 uv = (vec2(pixel) + 0.5) * f.screen.zw;
    s.p = reconstruct_world(f, uv, depth);
    s.n = normalize(normal_m.xyz);
    s.albedo = albedo_r.rgb;
    s.roughness = clamp(albedo_r.a, 0.02, 1.0);
    s.metallic = normal_m.w;
    s.emissive = emissive;
    s.backface = false;
    return true;
}

vec3 camera_ray(Frame f, ivec2 pixel) {
    vec2 uv = (vec2(pixel) + 0.5) * f.screen.zw;
    return normalize(reconstruct_world(f, uv, 1.0) - f.camera_position.xyz);
}

// Screen uv of a world point in the previous frame (unjittered). False if off screen.
bool reproject(Frame f, vec3 world, out vec2 uv) {
    vec4 clip = f.prev_view_projection * vec4(world, 1.0);
    if (clip.w <= 0.0) return false;
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return all(greaterThanEqual(uv, vec2(0))) && all(lessThanEqual(uv, vec2(1)));
}

#endif
