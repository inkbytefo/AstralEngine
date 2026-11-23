#version 460
#include <astral/bsdf.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance; } pc;
layout(location = 0) in vec3 in_normal;
layout(location = 1) flat in uint in_instance;
layout(location = 2) in vec4 in_clip_now;
layout(location = 3) in vec4 in_clip_prev;
layout(location = 4) in vec2 in_uv;
layout(location = 5) in vec3 in_world;
layout(location = 0) out vec4 out_albedo;       // rgb albedo, a roughness
layout(location = 1) out vec4 out_normal;       // xyz world normal, w metallic
layout(location = 2) out vec4 out_emissive;     // rgb emissive, a encoded shading model (model + 1)
layout(location = 3) out vec2 out_velocity;     // screen uv now - uv last frame
layout(location = 4) out vec4 out_material_ext; // per-model M13 PBR parameters

vec4 tex(uint index, vec2 uv) {
    return texture(sampler2D(astral_textures[nonuniformEXT(index)], astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), uv);
}

mat3 cotangent_frame(vec3 n, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p), dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float inv = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    return mat3(t * inv, b * inv, n);
}

void main() {
    Material m = pc.materials.data[pc.instances.data[in_instance].material];
    Frame f = pc.frame.f;
    vec3 n = normalize(in_normal);
    if (!gl_FrontFacing) n = -n;

    vec2 uv = in_uv;
    bool need_tbn = (m.textures.z != NO_TEXTURE) || (m.detail_textures.y != NO_TEXTURE) ||
                    (m.detail_textures.z != NO_TEXTURE && m.detail_params.z > 1e-5) || (m.custom.z == 3u);
    mat3 tbn = need_tbn ? cotangent_frame(n, in_world, in_uv) : mat3(1.0);

    // Parallax Occlusion Mapping (POM) displaces UVs along tangent-space view ray.
    if (m.detail_textures.z != NO_TEXTURE && m.detail_params.z > 1e-5) {
        vec3 v_world = normalize(f.camera_position.xyz - in_world);
        vec3 v_ts = transpose(tbn) * v_world;
        uv = astral_parallax_uv(m.detail_textures.z, uv, v_ts, m.detail_params.z, 12);
    }

    vec3 albedo = m.base_color.rgb;
    float roughness = m.emissive_roughness.a;
    float metallic = m.params.x;
    vec3 emissive = m.emissive_roughness.rgb;
    if (m.textures.x != NO_TEXTURE) albedo *= tex(m.textures.x, uv).rgb;
    if (m.detail_textures.x != NO_TEXTURE) albedo *= tex(m.detail_textures.x, uv * m.detail_params.x).rgb;
    if (m.textures.y != NO_TEXTURE) {
        vec4 mr = tex(m.textures.y, uv);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    if (m.textures.z != NO_TEXTURE || m.detail_textures.y != NO_TEXTURE) {
        vec3 n_ts = vec3(0.0, 0.0, 1.0);
        if (m.textures.z != NO_TEXTURE) {
            vec2 xy = tex(m.textures.z, uv).xy * 2.0 - 1.0;
            n_ts = normalize(vec3(xy * m.params.y, sqrt(max(0.0, 1.0 - dot(xy, xy)))));
        }
        if (m.detail_textures.y != NO_TEXTURE) {
            vec2 dxy = tex(m.detail_textures.y, uv * m.detail_params.x).xy * 2.0 - 1.0;
            vec3 d_ts = normalize(vec3(dxy, sqrt(max(0.0, 1.0 - dot(dxy, dxy)))));
            n_ts = astral_detail_normal(n_ts, d_ts, m.detail_params.y);
        }
        n = normalize(tbn * n_ts);
    }
    if (m.textures.w != NO_TEXTURE) emissive *= tex(m.textures.w, uv).rgb;

    float coat = m.coat_aniso.x;
    float coat_r = m.coat_aniso.y;
    astral_apply_wetness(albedo, roughness, coat, coat_r, metallic, m.params.z);

    // Map ShadingModel enum (0..5) -> SHADING_* code (0, 2..6; 1 is reserved for Unshaded).
    uint model = m.custom.z == 0u ? SHADING_STANDARD : (m.custom.z + 1u);
    if (model == SHADING_STANDARD && coat > 1e-4) model = SHADING_CLEARCOAT;

    vec4 mat_ext = vec4(0.0);
    if (model == SHADING_CLEARCOAT) {
        mat_ext = vec4(clamp(coat, 0.0, 1.0), clamp(coat_r, ASTRAL_MIN_ROUGHNESS, 1.0), 0.0, 0.0);
    } else if (model == SHADING_SHEEN) {
        mat_ext = vec4(max(m.sheen.rgb, vec3(0.0)), clamp(m.sheen.a, 0.05, 1.0));
    } else if (model == SHADING_ANISOTROPIC) {
        vec3 t_base = length(tbn[0]) > 1e-4 ? normalize(tbn[0] - n * dot(tbn[0], n)) : bsdf_default_tangent(n, 0.0);
        float rot = m.coat_aniso.w;
        vec3 b_base = cross(n, t_base);
        vec3 t_rot = normalize(t_base * cos(rot) + b_base * sin(rot));
        mat_ext = vec4(t_rot, clamp(m.coat_aniso.z, -0.95, 0.95));
    } else if (model == SHADING_TRANSMISSION) {
        float tr = clamp(m.transmission.x, 0.0, 1.0);
        albedo = mix(albedo, max(m.attenuation.rgb, vec3(1e-4)), tr);
        mat_ext = vec4(tr, max(m.transmission.y, 1.01),
                       max(m.transmission.z, 0.001), max(m.transmission.w, 0.001));
    } else if (model == SHADING_SUBSURFACE) {
        mat_ext = vec4(max(m.subsurface.rgb, vec3(0.0)), clamp(m.subsurface.a, 0.0, 1.0));
    }

    out_albedo = vec4(albedo, clamp(roughness, ASTRAL_MIN_ROUGHNESS, 1.0));
    out_normal = vec4(n, clamp(metallic, 0.0, 1.0));
    out_emissive = vec4(emissive, float(model + 1u));
    out_velocity = compute_velocity(in_clip_now, in_clip_prev);
    out_material_ext = mat_ext;
}
