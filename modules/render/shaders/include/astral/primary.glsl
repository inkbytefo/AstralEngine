// Primary surface of a pixel: the nearer of the mesh G-buffer and the SDF G-buffer.
#ifndef ASTRAL_PRIMARY_GLSL
#define ASTRAL_PRIMARY_GLSL
#include <astral/rt.glsl>

struct GBufferIds { uint albedo, normal, emissive, depth, velocity, sdf_albedo, sdf_normal, sdf_depth, sdf_velocity; };

vec4 fetch_texel(uint index, ivec2 p) {
    return texelFetch(sampler2D(astral_textures[index], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), p, 0);
}

// Returns false for sky pixels. `depth` is the device depth used for reconstruction.
bool load_primary(Frame f, GBufferIds g, ivec2 pixel, out Surface s, out float depth) {
    depth = fetch_texel(g.depth, pixel).r;
    vec4 albedo_r = fetch_texel(g.albedo, pixel);
    vec4 normal_m = fetch_texel(g.normal, pixel);
    vec4 emissive_u = fetch_texel(g.emissive, pixel);
    if (f.counts.y > 0u) {
        float sd = fetch_texel(g.sdf_depth, pixel).r;
        if (sd < depth) {
            depth = sd;
            albedo_r = fetch_texel(g.sdf_albedo, pixel);
            normal_m = fetch_texel(g.sdf_normal, pixel);
            emissive_u = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
    if (depth >= 1.0) return false;
    vec2 uv = (vec2(pixel) + 0.5) * f.screen.zw;
    s.p = reconstruct_world(f, uv, depth);
    float nlen = length(normal_m.xyz);
    s.n = nlen > 1e-6 ? normal_m.xyz / nlen : vec3(0.0, 1.0, 0.0);
    s.ng = s.n;
    s.albedo = albedo_r.rgb;
    s.roughness = clamp(albedo_r.a, ASTRAL_MIN_ROUGHNESS, 1.0);
    s.metallic = normal_m.w;
    s.emissive = emissive_u.rgb;
    s.backface = false;
    s.unshaded = emissive_u.a > 1.5;
    return true;
}

vec3 camera_ray(Frame f, ivec2 pixel) {
    vec2 uv = (vec2(pixel) + 0.5) * f.screen.zw;
    return normalize(reconstruct_world(f, uv, 1.0) - f.camera_position.xyz);
}

// Unjittered world-space view ray for sky reprojection (eliminates subpixel jitter drift on sky/stars).
vec3 unjittered_camera_ray(Frame f, ivec2 pixel) {
    vec2 uv = (vec2(pixel) + 0.5 + f.rt_params.yz) * f.screen.zw;
    return normalize(reconstruct_world(f, uv, 1.0) - f.camera_position.xyz);
}

// Motion of the pixel's surface since last frame (screen uv delta): per object for meshes.
vec2 primary_velocity(Frame f, GBufferIds g, ivec2 pixel) {
    if (f.counts.y > 0u && fetch_texel(g.sdf_depth, pixel).r < fetch_texel(g.depth, pixel).r) {
        return fetch_texel(g.sdf_velocity, pixel).xy;
    }
    return fetch_texel(g.velocity, pixel).xy;
}

// Screen uv of a world point in the previous frame (unjittered). False if off screen.
bool reproject(Frame f, vec3 world, out vec2 uv) {
    vec4 clip = f.prev_view_projection * vec4(world, 1.0);
    if (clip.w <= 0.0) return false;
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return all(greaterThanEqual(uv, vec2(0))) && all(lessThanEqual(uv, vec2(1)));
}

#endif
