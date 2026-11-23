// Advanced PBR BSDF layers (M13 / ADR-0033):
//   - Clearcoat (dielectric top coat, F0 = 0.04, energy-conserving base attenuation)
//   - Sheen (Estevez & Kulla 2017 Charlie NDF + Ashikhmin visibility)
//   - Anisotropic GGX (Heitz 2014/2018 NDF, correlated Smith V, anisotropic VNDF sampling)
//   - Transmission (exact dielectric Fresnel, Snell refraction, Beer-Lambert absorption)
//   - Subsurface Scattering (wrap diffuse + backlit translucency)
//   - Wetness, RNM detail normal blending, and Parallax Occlusion Mapping (POM)
#ifndef ASTRAL_BSDF_GLSL
#define ASTRAL_BSDF_GLSL
#include <astral/brdf.glsl>

// Shading model codes (encoded in G-buffer emissive.a as float(code + 1u); 0.0 = sky).
const uint SHADING_STANDARD     = 0u;
const uint SHADING_UNSHADED     = 1u;
const uint SHADING_CLEARCOAT    = 2u;
const uint SHADING_SHEEN        = 3u;
const uint SHADING_ANISOTROPIC  = 4u;
const uint SHADING_TRANSMISSION = 5u;
const uint SHADING_SUBSURFACE   = 6u;

uint decode_shading_model(float emissive_a) {
    int id = int(round(emissive_a)) - 1;
    return id < 0 ? SHADING_STANDARD : uint(id);
}

// --- Wetness, RNM Detail Normals & Parallax Occlusion Mapping -----------------------------

// Porosity darkening + roughness reduction + water-film clearcoat (IOR 1.33 -> F0 ~0.02..0.04).
void astral_apply_wetness(inout vec3 albedo, inout float roughness, inout float clearcoat,
                          inout float clearcoat_roughness, float metallic, float wetness) {
    float w = clamp(wetness, 0.0, 1.0);
    if (w <= 1e-4) return;
    float porous = (1.0 - metallic) * w;
    vec3 darkened = pow(max(albedo, vec3(1e-4)), vec3(1.55)) * 0.6;
    albedo = mix(albedo, min(albedo, darkened), porous);
    roughness = mix(roughness, min(roughness, 0.14), w * 0.75);
    clearcoat = max(clearcoat, w);
    clearcoat_roughness = mix(clearcoat_roughness, 0.04, w);
}

// Reoriented Normal Mapping (RNM) blending of two tangent-space unit normals.
vec3 astral_detail_normal(vec3 base_n, vec3 detail_n, float strength) {
    vec3 d = normalize(vec3(detail_n.xy * strength, detail_n.z));
    vec3 t = base_n + vec3(0.0, 0.0, 1.0);
    vec3 u = d * vec3(-1.0, -1.0, 1.0);
    float dtu = dot(t, u);
    return normalize(t * dtu - u * max(t.z, 1e-4));
}

// Steep Parallax Occlusion Mapping with linear refinement (height = 1 at top, 0 at bottom).
vec2 astral_parallax_uv(uint height_tex, vec2 uv, vec3 view_ts, float scale, int steps) {
    if (scale <= 1e-5 || steps <= 0 || height_tex == NO_TEXTURE) return uv;
    float vz = max(abs(view_ts.z), 0.15);
    vec2 p = (view_ts.xy / vz) * scale;
    float layer_depth = 1.0 / float(steps);
    vec2 delta_uv = p * layer_depth;
    vec2 curr_uv = uv;
    float curr_layer = 0.0;
    float curr_h = 1.0 - textureLod(sampler2D(astral_textures[nonuniformEXT(height_tex)],
                                              astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), curr_uv, 0.0).r;
    for (int i = 0; i < steps && curr_layer < curr_h; ++i) {
        curr_uv -= delta_uv;
        curr_h = 1.0 - textureLod(sampler2D(astral_textures[nonuniformEXT(height_tex)],
                                            astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), curr_uv, 0.0).r;
        curr_layer += layer_depth;
    }
    vec2 prev_uv = curr_uv + delta_uv;
    float after_d = curr_h - curr_layer;
    float before_d = (1.0 - textureLod(sampler2D(astral_textures[nonuniformEXT(height_tex)],
                                                 astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), prev_uv, 0.0).r) -
                     (curr_layer - layer_depth);
    float denom = after_d - before_d;
    float weight = abs(denom) > 1e-5 ? clamp(after_d / denom, 0.0, 1.0) : 0.0;
    return mix(curr_uv, prev_uv, weight);
}

// Single-sample parallax offset helper usable directly inside custom .ashader surface() functions.
vec2 astral_parallax_offset(vec2 uv, vec3 view_ts, float height, float scale) {
    float vz = max(abs(view_ts.z), 0.15);
    return uv - (view_ts.xy / vz) * ((1.0 - clamp(height, 0.0, 1.0)) * scale);
}

// --- Clearcoat (IOR 1.5, F0 = 0.04) -------------------------------------------------------

float clearcoat_brdf_radiance(vec3 n, vec3 v, vec3 l, float strength, float coat_roughness, out float base_atten) {
    float nl = max(dot(n, l), 0.0);
    float nv = max(dot(n, v), 1e-4);
    float c = clamp(strength, 0.0, 1.0);
    if (c <= 1e-4 || nl <= 0.0) {
        base_atten = 1.0;
        return 0.0;
    }
    vec3 h = normalize(v + l);
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);
    float rc = clamp(coat_roughness, ASTRAL_MIN_ROUGHNESS, 1.0);
    float ac = max(rc * rc, 0.002);
    float F = fresnel_schlick(vec3(0.04), vh).x;
    float F_view = fresnel_schlick(vec3(0.04), max(nv, nl)).x;
    base_atten = 1.0 - c * F_view;
    return c * ggx_d(nh, ac) * smith_v_correlated(nv, nl, ac) * F * nl;
}

// --- Sheen (Estevez & Kulla 2017 Charlie NDF + Ashikhmin visibility) ----------------------

float charlie_d(float nh, float roughness) {
    float a = max(roughness * roughness, 0.02);
    float inv_r = 1.0 / a;
    float sin2h = max(1.0 - nh * nh, 0.0078125);
    return (2.0 + inv_r) * pow(sin2h, inv_r * 0.5) / (2.0 * PI);
}

float sheen_v(float nv, float nl) {
    return 1.0 / max(4.0 * (nl + nv - nl * nv), 1e-4);
}

vec3 sheen_brdf_radiance(vec3 n, vec3 v, vec3 l, vec3 sheen_color, float sheen_roughness, out float base_atten) {
    float nl = max(dot(n, l), 0.0);
    float nv = max(dot(n, v), 1e-4);
    float peak = max(sheen_color.r, max(sheen_color.g, sheen_color.b));
    if (peak <= 1e-4 || nl <= 0.0) {
        base_atten = 1.0;
        return vec3(0.0);
    }
    vec3 h = normalize(v + l);
    float nh = max(dot(n, h), 0.0);
    float r = clamp(sheen_roughness, 0.05, 1.0);
    float D = charlie_d(nh, r);
    float V = sheen_v(nv, nl);
    // Analytic directional sheen albedo approximation for energy-conserving base attenuation.
    float E_v = clamp(0.15 + 0.65 * pow(1.0 - nv, 2.2) * (1.0 - 0.35 * r), 0.0, 0.85);
    float E_l = clamp(0.15 + 0.65 * pow(1.0 - nl, 2.2) * (1.0 - 0.35 * r), 0.0, 0.85);
    base_atten = clamp(1.0 - peak * max(E_v, E_l), 0.0, 1.0);
    return sheen_color * (D * V * nl);
}

// --- Anisotropic GGX (Heitz 2014 / Heitz 2018) --------------------------------------------

void aniso_alpha(float roughness, float anisotropy, out float ax, out float ay) {
    float a = max(roughness * roughness, 0.002);
    float an = clamp(anisotropy, -0.95, 0.95);
    ax = max(a * (1.0 + an), 0.002);
    ay = max(a * (1.0 - an), 0.002);
}

float ggx_aniso_d(vec3 h, float ax, float ay) {
    float hx = h.x / ax;
    float hy = h.y / ay;
    float d = hx * hx + hy * hy + h.z * h.z;
    return 1.0 / max(PI * ax * ay * d * d, 1e-12);
}

float smith_lambda_aniso(vec3 w, float ax, float ay) {
    float wz2 = max(w.z * w.z, 1e-8);
    float a2 = (ax * w.x) * (ax * w.x) + (ay * w.y) * (ay * w.y);
    return 0.5 * (sqrt(1.0 + a2 / wz2) - 1.0);
}

float smith_v_aniso(vec3 v, vec3 l, float ax, float ay) {
    float lv = l.z * sqrt(ax * ax * v.x * v.x + ay * ay * v.y * v.y + v.z * v.z);
    float ll = v.z * sqrt(ax * ax * l.x * l.x + ay * ay * l.y * l.y + l.z * l.z);
    return 0.5 / max(lv + ll, 1e-7);
}

float smith_vndf_weight_aniso(vec3 v, vec3 l, float ax, float ay) {
    float lv = smith_lambda_aniso(v, ax, ay);
    float ll = smith_lambda_aniso(l, ax, ay);
    return (1.0 + lv) / max(1.0 + lv + ll, 1e-7);
}

vec3 sample_vndf_aniso(vec3 v, float ax, float ay, vec2 u) {
    vec3 vh = normalize(vec3(ax * v.x, ay * v.y, max(v.z, 1e-5)));
    float len2 = vh.x * vh.x + vh.y * vh.y;
    vec3 t1 = len2 > 0.0 ? vec3(-vh.y, vh.x, 0.0) * inversesqrt(len2) : vec3(1, 0, 0);
    vec3 t2 = cross(vh, t1);
    float r = sqrt(clamp(u.x, 0.0, 0.999999));
    float phi = 2.0 * PI * u.y;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * sqrt(max(0.0, 1.0 - p1 * p1)) + s * p2;
    vec3 nh = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return normalize(vec3(ax * nh.x, ay * nh.y, max(0.0, nh.z)));
}

float vndf_aniso_pdf(vec3 v, vec3 h, float ax, float ay) {
    float g1 = 1.0 / (1.0 + smith_lambda_aniso(v, ax, ay));
    return g1 * ggx_aniso_d(h, ax, ay) / max(4.0 * max(v.z, 1e-4), 1e-7);
}

vec3 aniso_specular_brdf_radiance(vec3 n, vec3 t, vec3 b, vec3 v, vec3 l, vec3 f0, float roughness, float anisotropy) {
    float nl = max(dot(n, l), 0.0);
    if (nl <= 0.0) return vec3(0.0);
    vec3 h = normalize(v + l);
    vec3 vl = vec3(dot(v, t), dot(v, b), max(dot(v, n), 1e-4));
    vec3 ll = vec3(dot(l, t), dot(l, b), nl);
    vec3 hl = vec3(dot(h, t), dot(h, b), max(dot(h, n), 0.0));
    float ax, ay;
    aniso_alpha(roughness, anisotropy, ax, ay);
    float vh = max(dot(v, h), 0.0);
    vec3 F = fresnel_schlick(f0, vh);
    vec3 ms = ggx_ms_factor(f0, roughness, vl.z);
    return ggx_aniso_d(hl, ax, ay) * smith_v_aniso(vl, ll, ax, ay) * F * ms * nl;
}

// --- Transmission & Beer-Lambert Volume Absorption ----------------------------------------

// Exact unpolarized dielectric Fresnel reflectance; `eta = n_incident / n_transmitted`.
float fresnel_dielectric(float cos_i, float eta) {
    float ci = clamp(abs(cos_i), 0.0, 1.0);
    float sin2_t = eta * eta * max(0.0, 1.0 - ci * ci);
    if (sin2_t >= 1.0) return 1.0; // Total internal reflection
    float ct = sqrt(max(0.0, 1.0 - sin2_t));
    float rs = (eta * ci - ct) / max(eta * ci + ct, 1e-6);
    float rp = (ci - eta * ct) / max(ci + eta * ct, 1e-6);
    return 0.5 * (rs * rs + rp * rp);
}

vec3 beer_lambert(vec3 attenuation_color, float attenuation_distance, float dist) {
    vec3 col = clamp(attenuation_color, vec3(1e-4), vec3(1.0));
    vec3 sigma_a = -log(col) / max(attenuation_distance, 1e-4);
    return exp(-sigma_a * max(dist, 0.0));
}

// --- Subsurface Scattering (Wrap Diffuse + Backlit Translucency) --------------------------

vec3 sss_direct_radiance(vec3 n, vec3 v, vec3 l, vec3 sss_color, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s <= 1e-4) return vec3(0.0);
    float nl_raw = dot(n, l);
    float wrap = clamp((nl_raw + 0.5) / 1.5, 0.0, 1.0);
    float wrap_lobe = wrap * wrap * (1.0 / PI);
    vec3 back_h = normalize(-l + n * 0.35);
    float backlit = pow(clamp(dot(v, back_h), 0.0, 1.0), 3.0) * 0.35;
    return sss_color * s * (wrap_lobe * 0.65 + backlit);
}

// --- Tangent Frame Helpers ----------------------------------------------------------------

vec3 bsdf_default_tangent(vec3 n, float rotation) {
    vec3 t = abs(n.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), n)) : vec3(1.0, 0.0, 0.0);
    if (abs(rotation) <= 1e-5) return t;
    vec3 b = cross(n, t);
    return normalize(t * cos(rotation) + b * sin(rotation));
}

#endif
