#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance; } pc;
layout(location = 0) in vec3 in_normal;
layout(location = 1) flat in uint in_instance;
layout(location = 2) in vec4 in_clip_now;
layout(location = 3) in vec4 in_clip_prev;
layout(location = 4) in vec2 in_uv;
layout(location = 5) in vec3 in_world;
layout(location = 0) out vec4 out_albedo;   // rgb albedo, a roughness
layout(location = 1) out vec4 out_normal;   // xyz world normal, w metallic
layout(location = 2) out vec4 out_emissive; // rgb emissive
layout(location = 3) out vec2 out_velocity; // screen uv now - uv last frame

vec4 tex(uint index, vec2 uv) {
    return texture(sampler2D(astral_textures[nonuniformEXT(index)], astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), uv);
}

// Tangent frame from screen-space derivatives (Schüler, "Followup: Normal Mapping Without
// Precomputed Tangents"): normal maps work on any mesh, no tangent attribute needed.
vec3 perturb(vec3 n, vec3 map, float scale) {
    vec3 dp1 = dFdx(in_world), dp2 = dFdy(in_world);
    vec2 duv1 = dFdx(in_uv), duv2 = dFdy(in_uv);
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float inv = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    map.xy *= scale;
    return normalize(mat3(t * inv, b * inv, n) * map);
}

void main() {
    Material m = pc.materials.data[pc.instances.data[in_instance].material];
    vec3 albedo = m.base_color.rgb;
    float roughness = m.emissive_roughness.a;
    float metallic = m.params.x;
    vec3 emissive = m.emissive_roughness.rgb;
    vec3 n = normalize(in_normal);
    if (!gl_FrontFacing) n = -n;
    if (m.textures.x != NO_TEXTURE) albedo *= tex(m.textures.x, in_uv).rgb;
    if (m.textures.y != NO_TEXTURE) {
        vec4 mr = tex(m.textures.y, in_uv);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    if (m.textures.z != NO_TEXTURE) n = perturb(n, tex(m.textures.z, in_uv).xyz * 2.0 - 1.0, m.params.y);
    if (m.textures.w != NO_TEXTURE) emissive *= tex(m.textures.w, in_uv).rgb;
    out_albedo = vec4(albedo, roughness);
    out_normal = vec4(n, metallic);
    out_emissive = vec4(emissive, 1.0);
    vec2 now = in_clip_now.xy / in_clip_now.w * 0.5 + 0.5;
    vec2 prev = in_clip_prev.xy / max(in_clip_prev.w, 1e-6) * 0.5 + 0.5;
    out_velocity = now - prev;
}
