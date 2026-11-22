// Unified physically based BSDF contract (GGX NDF, height-correlated Smith G2, Schlick Fresnel,
// Heitz 2018 VNDF sampling, Karis split-sum DFG). Shared by raster, RT, and forward .ashader passes.
//
// Units convention:
//   - DirectionalLight (sun) and PointLight intensities are pre-multiplied by PI so a white diffuse
//     surface facing a light of intensity 1 reflects 1.0.
//   - RectLight, emissive surfaces, sky_radiance, and DDGI (E/PI) are in true radiance units.
#ifndef ASTRAL_BRDF_GLSL
#define ASTRAL_BRDF_GLSL
#include <astral/scene.glsl>

const float ASTRAL_MIN_ROUGHNESS = 0.02;
const float ASTRAL_MIRROR_ROUGHNESS = 0.04;

// Trowbridge-Reitz (GGX) normal distribution function D(h).
float ggx_d(float nh, float a) {
    float a2 = a * a;
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-12);
}

// Smith-GGX lambda(cos_theta) for height-correlated masking-shadowing (Heitz 2014).
float smith_lambda(float nx, float a) {
    float a2 = a * a;
    float nx2 = max(nx * nx, 1e-8);
    return 0.5 * (sqrt(1.0 + a2 * (1.0 - nx2) / nx2) - 1.0);
}

// Exact Smith-GGX masking function G1(x) = 1 / (1 + lambda(x)).
float smith_g1(float nx, float a) {
    float a2 = a * a;
    return 2.0 * nx / max(nx + sqrt(a2 + (1.0 - a2) * nx * nx), 1e-7);
}

// Height-correlated Smith-GGX visibility term V(v, l) = G2(v, l) / (4 * nv * nl).
// Singularity-free form (Lagarde & de Rousiers 2014 / Heitz 2014).
float smith_v_correlated(float nv, float nl, float a) {
    float a2 = a * a;
    float ggxv = nl * sqrt(a2 + (1.0 - a2) * nv * nv);
    float ggxl = nv * sqrt(a2 + (1.0 - a2) * nl * nl);
    return 0.5 / max(ggxv + ggxl, 1e-7);
}

// Exact height-correlated VNDF sample throughput ratio G2(v, l) / G1(v) = (1 + L(v)) / (1 + L(v) + L(l)).
float smith_vndf_weight(float nv, float nl, float a) {
    float lv = smith_lambda(nv, a);
    float ll = smith_lambda(nl, a);
    return (1.0 + lv) / max(1.0 + lv + ll, 1e-7);
}

// Reflection direction PDF of Heitz 2018 VNDF sampling: p(l) = G1(v) * D(h) / (4 * nv).
float vndf_reflection_pdf(float nv, float nh, float a) {
    return smith_g1(nv, a) * ggx_d(nh, a) / max(4.0 * nv, 1e-7);
}

vec3 fresnel_schlick(vec3 f0, float cos_theta) {
    float x = clamp(1.0 - cos_theta, 0.0, 1.0);
    float x2 = x * x;
    return f0 + (1.0 - f0) * (x2 * x2 * x);
}

// Pre-integrated specular reflectance (Karis 2014 analytic fit of the split-sum DFG term).
vec3 env_brdf(vec3 f0, float roughness, float nv) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
    vec2 ab = max(vec2(-1.04, 1.04) * a004 + r.zw, vec2(0.0));
    return f0 * ab.x + ab.y;
}

// Visible-normal sampling (Heitz 2018, JCGT). `v` in the local frame (z = normal).
vec3 sample_vndf(vec3 v, float a, vec2 u) {
    vec3 vh = normalize(vec3(a * v.x, a * v.y, max(v.z, 1e-5)));
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
    return normalize(vec3(a * nh.x, a * nh.y, max(0.0, nh.z)));
}

// Specular BRDF * (n.l) in true radiance units (NOT multiplied by PI).
vec3 specular_brdf_radiance(vec3 n, vec3 v, vec3 l, vec3 f0, float roughness) {
    float nl = max(dot(n, l), 0.0);
    if (nl <= 0.0) return vec3(0.0);
    vec3 h = normalize(v + l);
    float nv = max(dot(n, v), 1e-4);
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);
    float a = max(roughness * roughness, 0.002);
    return ggx_d(nh, a) * smith_v_correlated(nv, nl, a) * fresnel_schlick(f0, vh) * nl;
}

// Specular BRDF * (n.l) * PI for PI-premultiplied lights (sun and point lights).
vec3 specular_brdf(vec3 n, vec3 v, vec3 l, vec3 f0, float roughness) {
    return specular_brdf_radiance(n, v, l, f0, roughness) * PI;
}

// Full direct PBR BRDF * (n.l) * PI for PI-premultiplied lights (raster deferred & forward transparent).
vec3 eval_direct_pbr(vec3 n, vec3 v, vec3 l, vec3 albedo, float roughness, float metallic) {
    float nl = max(dot(n, l), 0.0);
    if (nl <= 0.0) return vec3(0.0);
    vec3 h = normalize(v + l);
    float nv = max(dot(n, v), 1e-4);
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);
    float a = max(roughness * roughness, 0.002);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnel_schlick(f0, vh);
    vec3 specular = ggx_d(nh, a) * smith_v_correlated(nv, nl, a) * F;
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;
    return (diffuse + specular) * nl * PI;
}

// Procedural hemisphere sky/ground radiance.
vec3 sky_radiance(Frame f, vec3 dir) {
    return mix(f.ground_color.rgb, f.sky_color.rgb, smoothstep(-0.2, 0.4, dir.y));
}

#endif
