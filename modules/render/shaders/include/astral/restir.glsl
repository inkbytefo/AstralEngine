// Spatiotemporal ReSTIR DI (Bitterli et al. 2020; Wyman & Panteleev 2021) shared types and helpers.
// Compact 16-byte GPU reservoir (matches GpuReservoir in src/gpu_layouts.hpp) over point + rect lights
// in the light-surface domain (unit shift-mapping Jacobian).
#ifndef ASTRAL_RESTIR_GLSL
#define ASTRAL_RESTIR_GLSL
#include <astral/rt.glsl>

const uint RESTIR_EMPTY_LIGHT = 0xFFFFFFFFu;
const uint RESTIR_RECT_BIT = 0x80000000u;
const uint RESTIR_INDEX_MASK = 0x7FFFFFFFu;
const uint RESTIR_INIT_CANDIDATES = 24u;
const uint RESTIR_MAX_TEMPORAL_M = 20u;
const uint RESTIR_MAX_SPATIAL_M = 60u;

struct Reservoir {
    uint light_id;  // bit 31 = is_rect, bits 0..30 = light index; RESTIR_EMPTY_LIGHT = empty
    uint uv_packed; // packUnorm2x16(uv) in [0, 1]^2 (rect surface UV or point-bulb sphere sample)
    float W;        // unbiased contribution weight (0 when empty or occluded)
    uint M;         // confidence / effective sample count
};
ASTRAL_BUFFER(Reservoirs, Reservoir);

float restir_rect_prob(uint pt_count, uint rect_count) {
    if (rect_count == 0u) return 0.0;
    if (pt_count == 0u) return 1.0;
    return clamp(float(rect_count) / float(pt_count + rect_count), 0.25, 0.75);
}

// Deterministic bulb point on a point light's source sphere from packed UV.
vec3 restir_point_target(PointLight light, vec2 uv, vec3 origin, vec3 ng) {
    if (light.source.x <= 1e-5) return light.position_radius.xyz;
    float z = uv.x * 2.0 - 1.0;
    float sr = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * PI * uv.y;
    vec3 bulb = light.position_radius.xyz + vec3(cos(phi) * sr, sin(phi) * sr, z) * light.source.x;
    return dot(ng, bulb - origin) > 0.0 ? bulb : light.position_radius.xyz;
}

// Evaluates unshadowed diffuse (albedo-free) and specular (full radiance, with RectLight MIS) for a reservoir sample.
bool restir_eval_sample(Frame f, PointLights lights, RectLights rects, Surface s, vec3 v,
                        uint light_id, uint uv_packed, out vec3 diffuse, out vec3 specular) {
    diffuse = vec3(0.0);
    specular = vec3(0.0);
    if (light_id == RESTIR_EMPTY_LIGHT) return false;
    bool is_rect = (light_id & RESTIR_RECT_BIT) != 0u;
    uint idx = light_id & RESTIR_INDEX_MASK;
    vec2 uv = unpackUnorm2x16(uv_packed);
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    float kd = 1.0 - s.metallic;

    if (!is_rect) {
        if (idx >= f.counts.x) return false;
        PointLight light = lights.data[idx];
        vec3 to = light.position_radius.xyz - s.p;
        float dist = max(length(to), 1e-4);
        float radius = max(light.position_radius.w, 1e-4);
        if (dist >= radius) return false;
        vec3 l = to / dist;
        float nl = dot(s.n, l);
        if (nl <= 0.0) return false;
        float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
        falloff = falloff * falloff / (dist * dist + 1.0);
        vec3 radiance = light.color_intensity.rgb * light.color_intensity.a * falloff;
        vec3 h = normalize(v + l);
        vec3 F = fresnel_schlick(f0, max(dot(v, h), 0.0));
        diffuse = (1.0 - F) * kd * nl * radiance;
        specular = specular_brdf(s.n, v, l, f0, s.roughness) * radiance;
        return true;
    }

    if (idx >= f.rt_counts.x) return false;
    RectLight r = rects.data[idx];
    vec2 u = uv * 2.0 - 1.0;
    vec3 lp = r.position_two_sided.xyz + r.right.xyz * u.x + r.up.xyz * u.y;
    vec3 to = lp - s.p;
    float d2 = dot(to, to);
    if (d2 < 1e-8) return false;
    float dist = sqrt(d2);
    vec3 l = to / dist;
    float nl = dot(s.n, l);
    vec3 ln = cross(r.right.xyz, r.up.xyz);
    float ln_len = length(ln);
    if (ln_len < 1e-8) return false;
    float area = 4.0 * ln_len;
    ln /= ln_len;
    float cos_l = dot(ln, -l);
    if (r.position_two_sided.w > 0.5) cos_l = abs(cos_l);
    if (nl <= 0.0 || cos_l <= 0.0) return false;
    vec3 h = normalize(v + l);
    vec3 F = fresnel_schlick(f0, max(dot(v, h), 0.0));
    float geom = cos_l * area / d2;
    diffuse = (1.0 - F) * kd * nl * r.radiance.rgb * geom * (1.0 / PI);
    // Deterministic MIS weight matching rt_lighting.comp's VNDF reflection hit on RectLight (w_light + w_brdf == 1).
    if (s.roughness >= ASTRAL_MIRROR_ROUGHNESS) {
        float nv = max(dot(s.n, v), 1e-4);
        float nh = max(dot(s.n, h), 0.0);
        float a = max(s.roughness * s.roughness, 0.002);
        float prob = rect_light_select_prob(f, rects, idx, s.p, s.n);
        float pdf_light = prob / max(geom, 1e-8);
        float pdf_brdf = vndf_reflection_pdf(nv, nh, a);
        float w_mis = mis_power2(pdf_light, pdf_brdf);
        specular = specular_brdf_radiance(s.n, v, l, f0, s.roughness) * r.radiance.rgb * geom * w_mis;
    }
    return true;
}

float restir_target_pdf(Frame f, PointLights lights, RectLights rects, Surface s, vec3 v,
                        uint light_id, uint uv_packed) {
    vec3 d, sp;
    if (!restir_eval_sample(f, lights, rects, s, v, light_id, uv_packed, d, sp)) return 0.0;
    float p = astral_luminance(s.albedo * d + sp);
    return (isnan(p) || isinf(p) || p <= 0.0) ? 0.0 : p;
}

bool restir_visible(Frame f, LightingInputs li, Surface s, uint light_id, uint uv_packed) {
    if (light_id == RESTIR_EMPTY_LIGHT) return false;
    bool is_rect = (light_id & RESTIR_RECT_BIT) != 0u;
    uint idx = light_id & RESTIR_INDEX_MASK;
    vec2 uv = unpackUnorm2x16(uv_packed);
    vec3 origin = primary_ray_origin(f, s.p, s.n);
    vec3 target;
    float pad;
    if (!is_rect) {
        if (idx >= f.counts.x) return false;
        PointLight light = li.lights.data[idx];
        target = restir_point_target(light, uv, origin, s.ng);
        pad = max(light.source.x * 0.25, 0.001);
    } else {
        if (idx >= f.rt_counts.x) return false;
        RectLight r = li.rects.data[idx];
        vec2 u = uv * 2.0 - 1.0;
        target = r.position_two_sided.xyz + r.right.xyz * u.x + r.up.xyz * u.y;
        pad = 0.001;
    }
    vec3 sd = target - origin;
    float sl = length(sd);
    float tmax = sl - pad;
    if (sl <= 1e-4 || tmax <= 0.0) return true;
    return !occluded(li.tlas, li.shapes, li.sdf_count, origin, sd / sl, tmax);
}

float restir_sanitize_weight(float w) {
    return (isnan(w) || isinf(w) || w <= 0.0) ? 0.0 : min(w, 1e4);
}

#endif
