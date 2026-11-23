// Ray-traced lighting helpers: random numbers, robust ray offsets (Wachter & Binder 2019),
// ray queries against the TLAS (+ SDF scene), hit surface reconstruction, light sampling (with
// RectLight MIS and unbiased single-frame RIS for high light counts), octahedral maps and DDGI.
#ifndef ASTRAL_RT_GLSL
#define ASTRAL_RT_GLSL
#extension GL_EXT_ray_query : require
#include <astral/sdf.glsl>
#include <astral/brdf.glsl>
#include <astral/color.glsl>

struct RectLight {
    vec4 position_two_sided; // xyz center, w 1 = emits on both sides
    vec4 right;              // xyz half extent along the first axis
    vec4 up;                 // xyz half extent along the second axis (normal = cross(right, up))
    vec4 radiance;           // rgb emitted radiance
};
ASTRAL_BUFFER(RectLights, RectLight);
ASTRAL_BUFFER(Indices, uint);
ASTRAL_BUFFER(ProbeRays, vec4);

// --- Random -------------------------------------------------------------------------------
uint pcg(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint w = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint state) { return float(pcg(state)) * (1.0 / 4294967296.0); }
uint make_seed(uvec2 p, uint frame, uint salt) {
    uint s = p.x * 1973u + p.y * 9277u + frame * 26699u + salt * 104729u;
    pcg(s);
    return s;
}
// R2 low-discrepancy sequence shifted per pixel (Cranley-Patterson): well stratified over frames.
vec2 r2(uint frame, inout uint state) {
    vec2 shift = vec2(rnd(state), rnd(state));
    return fract(shift + vec2(0.7548776662, 0.5698402910) * float(frame & 1023u));
}

vec3 tangent_of(vec3 n) { return abs(n.y) < 0.999 ? normalize(cross(vec3(0, 1, 0), n)) : vec3(1, 0, 0); }
vec3 to_world(vec3 local, vec3 n) {
    vec3 t = tangent_of(n);
    vec3 b = cross(n, t);
    return t * local.x + b * local.y + n * local.z;
}
// Uniform direction inside a cone of half-angle `angle` around `axis`.
vec3 sample_cone(vec3 axis, float angle, vec2 u) {
    float cos_max = cos(angle);
    float c = mix(1.0, cos_max, u.x);
    float s = sqrt(max(0.0, 1.0 - c * c));
    float phi = 2.0 * PI * u.y;
    return to_world(vec3(cos(phi) * s, sin(phi) * s, c), axis);
}

// --- Robust ray origin offsets (Wachter & Binder 2019, Ray Tracing Gems Ch. 6) -----------
vec3 offset_ray(vec3 p, vec3 ng) {
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    const float int_scale = 256.0;
    ivec3 of_i = ivec3(int_scale * ng.x, int_scale * ng.y, int_scale * ng.z);
    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + (p.x < 0.0 ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + (p.y < 0.0 ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + (p.z < 0.0 ? -of_i.z : of_i.z)));
    return vec3(
        abs(p.x) < origin ? p.x + float_scale * ng.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * ng.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * ng.z : p_i.z);
}

// Primary G-buffer hits are reconstructed from D32Float depth whose quantization error grows with
// camera distance; secondary hits use exact triangle/SDF positions and call offset_ray() directly.
vec3 primary_ray_origin(Frame f, vec3 p, vec3 n) {
    vec3 robust = offset_ray(p, n);
    float depth_bias = 0.001 + 0.0004 * distance(p, f.camera_position.xyz);
    return dot(robust - p, n) > depth_bias ? robust : p + n * depth_bias;
}

// --- Surfaces -----------------------------------------------------------------------------
struct Surface {
    vec3 p;
    vec3 n;         // shading normal, facing the incoming ray
    vec3 ng;        // geometric normal, facing the incoming ray (for self-intersection-free offsets)
    vec3 albedo;
    float roughness;
    float metallic;
    vec3 emissive;
    bool backface;  // the ray hit the back of a mesh triangle or inside of an SDF
    bool unshaded;  // render_mode unshaded: bypass lighting and output albedo + emissive directly
};

// Custom (.ashader) surfaces, ADR-0022: the renderer compiles every shader's surface() into
// <astral/rt_custom.glsl> (ASTRAL_RT_CUSTOM) and these compute shaders again; without custom shaders
// the stubs at the end of this file do nothing. astral_rt_begin() is the first call of main().
ASTRAL_BUFFER(AstralParams, uint);
AstralParams astral_params;
uint astral_base;
float astral_time;
Instances astral_rt_instances;
Materials astral_rt_materials;
float astral_pf(uint o) { return uintBitsToFloat(astral_params.data[astral_base + o]); }
uint astral_pu(uint o) { return astral_params.data[astral_base + o]; }
#define TIME astral_time
void astral_rt_begin(Frame f, Instances instances, Materials materials) {
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_time = uintBitsToFloat(f.custom.z);
    astral_rt_instances = instances;
    astral_rt_materials = materials;
}
void astral_custom_surface(uint shader, uint base, vec2 uv, vec3 view_dir, inout Surface s);
bool astral_custom_alpha_test(uint shader, uint base, vec2 uv, vec3 p, vec3 n, vec3 view_dir, bool backface);

// Triangle hit -> interpolated surface. `dir` is the ray direction.
Surface triangle_surface(Instances instances, Materials materials, uint instance, uint prim, vec2 bary,
                         vec3 origin, vec3 dir, float t) {
    Instance inst = instances.data[instance];
    Material mat = materials.data[inst.material];
    Indices idx = Indices(address_of(inst.indices));
    Vertices verts = Vertices(address_of(inst.vertices));
    Vertex v0 = verts.data[idx.data[prim * 3u + 0u]];
    Vertex v1 = verts.data[idx.data[prim * 3u + 1u]];
    Vertex v2 = verts.data[idx.data[prim * 3u + 2u]];
    float b0 = 1.0 - bary.x - bary.y;
    mat3 normal_matrix = normal_matrix_of(inst.model);
    vec3 raw_n = normal_matrix * (v0.normal * b0 + v1.normal * bary.x + v2.normal * bary.y);
    vec3 p0 = (inst.model * vec4(v0.position, 1)).xyz;
    vec3 p1 = (inst.model * vec4(v1.position, 1)).xyz;
    vec3 p2 = (inst.model * vec4(v2.position, 1)).xyz;
    vec3 cross_e = cross(p1 - p0, p2 - p0);
    float cross_len2 = dot(cross_e, cross_e);
    vec3 ng = cross_len2 > 1e-20 ? cross_e * inversesqrt(cross_len2) : -dir;
    float n_len2 = dot(raw_n, raw_n);
    vec3 n = n_len2 > 1e-20 ? raw_n * inversesqrt(n_len2) : ng;
    Surface s;
    // Exact barycentric interpolation avoids o + d * t floating-point drift off the triangle plane.
    s.p = p0 * b0 + p1 * bary.x + p2 * bary.y;
    s.backface = dot(ng, dir) > 0.0;
    s.ng = s.backface ? -ng : ng;
    s.n = dot(n, dir) > 0.0 ? -n : n;
    s.albedo = mat.base_color.rgb;
    float roughness = mat.emissive_roughness.a;
    s.metallic = mat.params.x;
    s.emissive = mat.emissive_roughness.rgb;
    // Textures at a fixed coarse mip: compute shaders have no screen-space derivatives.
    vec2 uv = v0.uv * b0 + v1.uv * bary.x + v2.uv * bary.y;
    const float lod = 2.0;
    if (mat.textures.x != NO_TEXTURE) s.albedo *= textureLod(sampler2D(astral_textures[nonuniformEXT(mat.textures.x)], astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), uv, lod).rgb;
    if (mat.textures.y != NO_TEXTURE) {
        vec4 mr = textureLod(sampler2D(astral_textures[nonuniformEXT(mat.textures.y)], astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), uv, lod);
        roughness *= mr.g;
        s.metallic *= mr.b;
    }
    if (mat.textures.w != NO_TEXTURE) s.emissive *= textureLod(sampler2D(astral_textures[nonuniformEXT(mat.textures.w)], astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), uv, lod).rgb;
    s.roughness = clamp(roughness, ASTRAL_MIN_ROUGHNESS, 1.0);
    s.unshaded = false;
    if (mat.custom.y != 0u) astral_custom_surface(mat.custom.y - 1u, mat.custom.x, uv, -dir, s);
    return s;
}

#ifdef ASTRAL_RT_ALPHA_TEST
bool confirm_candidate_hit(rayQueryEXT q, vec3 dir) {
    uint instance = rayQueryGetIntersectionInstanceCustomIndexEXT(q, false);
    uint prim = rayQueryGetIntersectionPrimitiveIndexEXT(q, false);
    vec2 bary = rayQueryGetIntersectionBarycentricsEXT(q, false);
    Instance inst = astral_rt_instances.data[instance];
    Material mat = astral_rt_materials.data[inst.material];
    if (mat.custom.y == 0u) return true;
    Indices idx = Indices(address_of(inst.indices));
    Vertices verts = Vertices(address_of(inst.vertices));
    Vertex v0 = verts.data[idx.data[prim * 3u + 0u]];
    Vertex v1 = verts.data[idx.data[prim * 3u + 1u]];
    Vertex v2 = verts.data[idx.data[prim * 3u + 2u]];
    float b0 = 1.0 - bary.x - bary.y;
    vec2 uv = v0.uv * b0 + v1.uv * bary.x + v2.uv * bary.y;
    vec3 p0 = (inst.model * vec4(v0.position, 1)).xyz;
    vec3 p1 = (inst.model * vec4(v1.position, 1)).xyz;
    vec3 p2 = (inst.model * vec4(v2.position, 1)).xyz;
    vec3 p = p0 * b0 + p1 * bary.x + p2 * bary.y;
    vec3 ce = cross(p1 - p0, p2 - p0);
    vec3 ng = dot(ce, ce) > 1e-20 ? normalize(ce) : -dir;
    bool backface = dot(ng, dir) > 0.0;
    vec3 n = backface ? -ng : ng;
    return astral_custom_alpha_test(mat.custom.y - 1u, mat.custom.x, uv, p, n, -dir, backface);
}
#endif

struct TraceHit {
    bool hit;
    bool sdf;
    float t;
    uint instance;
    uint prim;
    vec2 bary;
};

// Closest hit among triangles (TLAS) and the SDF scene (raymarched up to the triangle hit).
TraceHit trace(uint64_t tlas, SdfShapes shapes, uint sdf_count, vec3 o, vec3 d, float tmax) {
    TraceHit h = TraceHit(false, false, tmax, 0u, 0u, vec2(0));
    if (tmax <= 0.0) return h;
    rayQueryEXT q;
#ifdef ASTRAL_RT_ALPHA_TEST
    const uint flags = gl_RayFlagsNoneEXT;
#else
    const uint flags = gl_RayFlagsOpaqueEXT;
#endif
    rayQueryInitializeEXT(q, accelerationStructureEXT(tlas), flags, 0xFF, o, 0.0, d, tmax);
    while (rayQueryProceedEXT(q)) {
#ifdef ASTRAL_RT_ALPHA_TEST
        if (rayQueryGetIntersectionTypeEXT(q, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            if (confirm_candidate_hit(q, d)) rayQueryConfirmIntersectionEXT(q);
        }
#endif
    }
    if (rayQueryGetIntersectionTypeEXT(q, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
        h.hit = true;
        h.t = rayQueryGetIntersectionTEXT(q, true);
        h.instance = rayQueryGetIntersectionInstanceCustomIndexEXT(q, true);
        h.prim = rayQueryGetIntersectionPrimitiveIndexEXT(q, true);
        h.bary = rayQueryGetIntersectionBarycentricsEXT(q, true);
    }
    if (sdf_count > 0u) {
        float t = 0.0;
        for (int i = 0; i < 128 && t < h.t; ++i) {
            float dist = sdf_distance(shapes, sdf_count, o + d * t);
            if (abs(dist) < 0.0004 * max(t, 1.0)) {
                h = TraceHit(true, true, t, 0u, 0u, vec2(0));
                break;
            }
            t += max(abs(dist), 0.0002 * max(t, 1.0));
        }
    }
    return h;
}

bool occluded(uint64_t tlas, SdfShapes shapes, uint sdf_count, vec3 o, vec3 d, float tmax) {
    if (tmax <= 0.0) return false;
    rayQueryEXT q;
#ifdef ASTRAL_RT_ALPHA_TEST
    const uint flags = gl_RayFlagsTerminateOnFirstHitEXT;
#else
    const uint flags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT;
#endif
    rayQueryInitializeEXT(q, accelerationStructureEXT(tlas), flags, 0xFF, o, 0.0, d, tmax);
    while (rayQueryProceedEXT(q)) {
#ifdef ASTRAL_RT_ALPHA_TEST
        if (rayQueryGetIntersectionTypeEXT(q, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            if (confirm_candidate_hit(q, d)) rayQueryConfirmIntersectionEXT(q);
        }
#endif
    }
    if (rayQueryGetIntersectionTypeEXT(q, true) != gl_RayQueryCommittedIntersectionNoneEXT) return true;
    if (sdf_count > 0u) {
        float t = 0.0;
        for (int i = 0; i < 96 && t < tmax; ++i) {
            float dist = sdf_distance(shapes, sdf_count, o + d * t);
            if (dist < 0.0004 * max(t, 1.0)) return true;
            t += dist;
        }
    }
    return false;
}

Surface sdf_surface(SdfShapes shapes, uint count, vec3 o, vec3 d, float t) {
    Surface s;
    s.p = o + d * t;
    SdfHit h = sdf_scene(shapes, count, s.p);
    vec3 n = sdf_normal(shapes, count, s.p, 0.001 * max(t, 1.0));
    s.backface = dot(n, d) > 0.0;
    s.n = s.backface ? -n : n;
    s.ng = s.n;
    s.albedo = h.albedo;
    s.roughness = clamp(h.roughness, ASTRAL_MIN_ROUGHNESS, 1.0);
    s.metallic = h.metallic;
    s.emissive = vec3(0);
    s.unshaded = false;
    return s;
}

// Distance along the ray to a rect light (or -1).
float intersect_rect(RectLight l, vec3 o, vec3 d) {
    vec3 cross_ru = cross(l.right.xyz, l.up.xyz);
    float len2 = dot(cross_ru, cross_ru);
    if (len2 < 1e-12) return -1.0;
    vec3 n = cross_ru * inversesqrt(len2);
    float dn = dot(d, n);
    if (abs(dn) < 1e-6) return -1.0;
    if (l.position_two_sided.w < 0.5 && dn > 0.0) return -1.0; // seen from behind
    float t = dot(l.position_two_sided.xyz - o, n) / dn;
    if (t <= 0.0) return -1.0;
    vec3 local = o + d * t - l.position_two_sided.xyz;
    float u = dot(local, l.right.xyz) / max(dot(l.right.xyz, l.right.xyz), 1e-12);
    float v = dot(local, l.up.xyz) / max(dot(l.up.xyz, l.up.xyz), 1e-12);
    return abs(u) <= 1.0 && abs(v) <= 1.0 ? t : -1.0;
}

// --- Octahedral mapping --------------------------------------------------------------------
vec2 sign_not_zero(vec2 v) { return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }
vec2 oct_encode(vec3 n) {
    n /= max(abs(n.x) + abs(n.y) + abs(n.z), 1e-8);
    return n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign_not_zero(n.xy);
}
vec3 oct_decode(vec2 e) {
    vec3 v = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);
    return normalize(v);
}

// --- DDGI ----------------------------------------------------------------------------------
const int DDGI_IRR = 8;   // interior texels per probe (irradiance)
const int DDGI_DIST = 16; // interior texels per probe (distance moments)

uint ddgi_probe_count(Frame f) { return f.ddgi_counts.x * f.ddgi_counts.y * f.ddgi_counts.z; }
ivec3 ddgi_probe_coord(Frame f, uint index) {
    uint nx = f.ddgi_counts.x, ny = f.ddgi_counts.y;
    return ivec3(index % nx, (index / nx) % ny, index / (nx * ny));
}
uint ddgi_probe_index(Frame f, ivec3 c) {
    return uint(c.x) + uint(c.y) * f.ddgi_counts.x + uint(c.z) * f.ddgi_counts.x * f.ddgi_counts.y;
}
vec3 ddgi_probe_position(Frame f, ivec3 c) { return f.ddgi_origin.xyz + vec3(c) * f.ddgi_spacing.xyz; }
float ddgi_max_dist(Frame f) { return max(1.5 * length(f.ddgi_spacing.xyz), 1e-3); }
// Atlas: probes in rows of nx*ny tiles, nz rows; each tile has a 1-texel border.
ivec2 ddgi_tile_origin(Frame f, uint index, int interior) {
    uint cols = f.ddgi_counts.x * f.ddgi_counts.y;
    return ivec2(int(index % cols), int(index / cols)) * (interior + 2);
}
ivec2 ddgi_atlas_size(Frame f, int interior) {
    return ivec2(int(f.ddgi_counts.x * f.ddgi_counts.y), int(f.ddgi_counts.z)) * (interior + 2);
}
vec2 ddgi_uv(Frame f, uint index, vec3 dir, int interior) {
    vec2 texel = vec2(ddgi_tile_origin(f, index, interior)) + 1.0 + (oct_encode(dir) * 0.5 + 0.5) * float(interior);
    return texel / vec2(ddgi_atlas_size(f, interior));
}

// Spherical Fibonacci ray directions, rotated every frame so probes see the whole sphere over time.
mat3 ddgi_rotation(uint frame) {
    uint s = frame * 2654435761u + 12345u;
    vec3 axis = normalize(vec3(rnd(s), rnd(s), rnd(s)) * 2.0 - 1.0 + vec3(1e-4));
    float angle = rnd(s) * 2.0 * PI;
    float c = cos(angle), si = sin(angle), t = 1.0 - c;
    vec3 a = axis;
    return mat3(t * a.x * a.x + c, t * a.x * a.y + si * a.z, t * a.x * a.z - si * a.y,
                t * a.x * a.y - si * a.z, t * a.y * a.y + c, t * a.y * a.z + si * a.x,
                t * a.x * a.z + si * a.y, t * a.y * a.z - si * a.x, t * a.z * a.z + c);
}
vec3 ddgi_ray_dir(uint ray, uint count, mat3 rotation) {
    float golden = 2.39996323;
    float z = 1.0 - (2.0 * float(ray) + 1.0) / float(count);
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = float(ray) * golden;
    return rotation * vec3(cos(phi) * r, sin(phi) * r, z);
}

// Irradiance (cosine-weighted average radiance, E/PI) at `p` with normal `n`; `v` points to the viewer.
// Distance moments are stored normalized in [0, 1] by ddgi_max_dist(f) so RG16Float never overflows.
vec3 ddgi_irradiance(Frame f, uint irr_tex, uint dist_tex, vec3 p, vec3 n, vec3 v) {
    if (f.ddgi_counts.w == 0u) return vec3(0);
    vec3 spacing = f.ddgi_spacing.xyz;
    float min_spacing = min(spacing.x, min(spacing.y, spacing.z));
    float max_d = ddgi_max_dist(f);
    vec3 biased = p + (n * 0.2 + v * 0.8) * (0.3 * min_spacing);
    ivec3 maxc = ivec3(f.ddgi_counts.xyz) - 1;
    vec3 grid = (biased - f.ddgi_origin.xyz) / spacing;
    ivec3 base = clamp(ivec3(floor(grid)), ivec3(0), max(maxc - 1, ivec3(0)));
    vec3 alpha = clamp(grid - vec3(base), 0.0, 1.0);
    vec3 sum = vec3(0);
    float weight_sum = 0.0;
    for (int i = 0; i < 8; ++i) {
        ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        ivec3 c = clamp(base + offset, ivec3(0), maxc);
        uint index = ddgi_probe_index(f, c);
        vec3 probe = ddgi_probe_position(f, c);
        vec3 to_probe = probe - biased;
        float r = max(length(to_probe), 1e-4);
        vec3 dir = to_probe / r;
        vec3 tri = mix(1.0 - alpha, alpha, vec3(offset));
        float trilinear = tri.x * tri.y * tri.z;
        // Smooth backface test: probes behind the surface barely count.
        vec3 p_to_probe = probe - p;
        float p_len = length(p_to_probe);
        vec3 p_dir = p_len > 1e-5 ? p_to_probe / p_len : n;
        float w = (dot(p_dir, n) + 1.0) * 0.5;
        w = w * w + 0.2;
        // Chebyshev visibility from the probe's normalized distance moments.
        vec2 moments = astral_sample(dist_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, ddgi_uv(f, index, -dir, DDGI_DIST)).rg;
        float r_norm = r / max_d;
        if (r_norm > moments.x) {
            float variance = max(moments.y - moments.x * moments.x, 0.0);
            float diff = r_norm - moments.x;
            float cheb = variance / max(variance + diff * diff, 1e-8);
            w *= max(cheb * cheb * cheb, 0.0);
        }
        w = max(w, 1e-6);
        if (w < 0.2) w *= w * w / 0.04; // crush tiny weights (light leaks)
        w *= trilinear;
        vec3 irr = astral_sample(irr_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, ddgi_uv(f, index, n, DDGI_IRR)).rgb;
        sum += sqrt(max(irr, vec3(0))) * w; // blend perceptually
        weight_sum += w;
    }
    vec3 e = sum / max(weight_sum, 1e-6);
    return e * e;
}

// --- Direct lighting (with RectLight MIS & unbiased RIS for high light counts) ------------
struct LightingInputs {
    uint64_t tlas;
    SdfShapes shapes;
    uint sdf_count;
    PointLights lights;
    RectLights rects;
};

float mis_power2(float pdf_a, float pdf_b) {
    float a2 = pdf_a * pdf_a;
    return a2 / max(a2 + pdf_b * pdf_b, 1e-12);
}

// Cheap unshadowed importance proxy for a point light (strictly positive whenever contribution > 0).
float point_light_ris_weight(PointLight light, vec3 p, vec3 n) {
    vec3 to = light.position_radius.xyz - p;
    float dist = max(length(to), 1e-4);
    float radius = max(light.position_radius.w, 1e-4);
    if (dist >= radius) return 0.0;
    float nl = dot(n, to / dist);
    if (nl <= 0.0) return 0.0;
    float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    falloff = falloff * falloff / (dist * dist + 1.0);
    return nl * falloff * max(astral_luminance(light.color_intensity.rgb * light.color_intensity.a), 1e-4);
}

// Cheap unshadowed importance proxy for a rect light (used by both RIS and MIS).
float rect_light_ris_weight(RectLight r, vec3 p, vec3 n) {
    vec3 to = r.position_two_sided.xyz - p;
    float d2 = max(dot(to, to), 0.04);
    vec3 cross_ru = cross(r.right.xyz, r.up.xyz);
    float area = 4.0 * length(cross_ru);
    if (area <= 1e-8) return 0.0;
    // Allow partially above-horizon panels to retain positive weight.
    float max_ext = length(r.right.xyz) + length(r.up.xyz);
    float nl_bound = max(dot(n, to) + max_ext, 0.0) / sqrt(d2);
    if (nl_bound <= 0.0) return 0.0;
    return max(astral_luminance(r.radiance.rgb), 1e-4) * area * nl_bound / d2;
}

// Effective probability that rect light `target_idx` was selected for light sampling on a primary hit.
float rect_light_select_prob(Frame f, RectLights rects, uint target_idx, vec3 p, vec3 n) {
    if ((f.counts.w & 1u) != 0u) return 1.0 / float(max(f.rt_counts.x, 1u));
    if (f.rt_counts.x <= 2u) return 1.0;
    float w_sum = 0.0;
    float w_target = 0.0;
    for (uint i = 0u; i < f.rt_counts.x; ++i) {
        float w = rect_light_ris_weight(rects.data[i], p, n);
        w_sum += w;
        if (i == target_idx) w_target = w;
    }
    return w_sum > 0.0 ? w_target / w_sum : 0.0;
}

void eval_point_light(LightingInputs li, PointLight light, Surface s, vec3 v, vec3 origin, vec3 f0, float kd,
                      float scale, inout uint seed, inout vec3 diffuse, inout vec3 specular) {
    vec3 to = light.position_radius.xyz - s.p;
    float dist = max(length(to), 1e-4);
    float radius = max(light.position_radius.w, 1e-4);
    if (dist >= radius) return;
    vec3 l = to / dist;
    float nl = dot(s.n, l);
    if (nl <= 0.0) return;
    float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    falloff = falloff * falloff / (dist * dist + 1.0);
    vec3 radiance = light.color_intensity.rgb * light.color_intensity.a * falloff * scale;
    vec2 u = vec2(rnd(seed), rnd(seed));
    vec3 jitter = sample_cone(-l, PI * 0.5, u) * light.source.x * sqrt(rnd(seed));
    vec3 target = light.position_radius.xyz + jitter;
    vec3 sd = target - origin;
    float sl = length(sd);
    if (sl > 1e-4) {
        vec3 sdir = sd / sl;
        if (dot(s.ng, sdir) <= 0.0) {
            sd = light.position_radius.xyz - origin;
            sl = length(sd);
            sdir = sl > 1e-4 ? sd / sl : l;
        }
        float tmax = sl - max(light.source.x * 0.25, 0.001);
        if (tmax > 0.0 && occluded(li.tlas, li.shapes, li.sdf_count, origin, sdir, tmax)) return;
    }
    vec3 h = normalize(v + l);
    vec3 F = fresnel_schlick(f0, max(dot(v, h), 0.0));
    diffuse += (1.0 - F) * kd * nl * radiance;
    specular += specular_brdf(s.n, v, l, f0, s.roughness) * radiance;
}

void eval_rect_light(LightingInputs li, RectLight r, Surface s, vec3 v, vec3 origin, vec3 f0, float kd,
                     float scale, float select_prob, bool primary_hit, inout uint seed,
                     inout vec3 diffuse, inout vec3 specular) {
    vec2 u = vec2(rnd(seed), rnd(seed)) * 2.0 - 1.0;
    vec3 lp = r.position_two_sided.xyz + r.right.xyz * u.x + r.up.xyz * u.y;
    vec3 to = lp - origin;
    float d2 = dot(to, to);
    if (d2 < 1e-8) return;
    float dist = sqrt(d2);
    vec3 l = to / dist;
    float nl = dot(s.n, l);
    vec3 ln = cross(r.right.xyz, r.up.xyz);
    float ln_len = length(ln);
    if (ln_len < 1e-8) return;
    float area = 4.0 * ln_len;
    ln /= ln_len;
    float cos_l = dot(ln, -l);
    if (r.position_two_sided.w > 0.5) cos_l = abs(cos_l);
    if (nl <= 0.0 || cos_l <= 0.0) return;
    float tmax = dist - 0.001;
    if (tmax > 0.0 && occluded(li.tlas, li.shapes, li.sdf_count, origin, l, tmax)) return;
    vec3 h = normalize(v + l);
    vec3 F = fresnel_schlick(f0, max(dot(v, h), 0.0));
    float geom = cos_l * area / d2;
    diffuse += (1.0 - F) * kd * nl * r.radiance.rgb * geom * (scale / PI);
    // Specular: on primary hits, MIS with the VNDF reflection ray (disabled on mirrors where VNDF is delta).
    // On secondary hits (no second reflection ray), evaluate directly with conservative clamp.
    if (primary_hit) {
        if (s.roughness >= ASTRAL_MIRROR_ROUGHNESS) {
            float nv = max(dot(s.n, v), 1e-4);
            float nh = max(dot(s.n, h), 0.0);
            float a = max(s.roughness * s.roughness, 0.002);
            float pdf_light = select_prob / max(geom, 1e-8);
            float pdf_brdf = vndf_reflection_pdf(nv, nh, a);
            float w_mis = mis_power2(pdf_light, pdf_brdf);
            specular += specular_brdf_radiance(s.n, v, l, f0, s.roughness) * r.radiance.rgb * geom * scale * w_mis;
        }
    } else {
        specular += min(specular_brdf_radiance(s.n, v, l, f0, s.roughness) * r.radiance.rgb * geom * scale, vec3(16.0));
    }
}

// Direct illumination with ray-traced visibility. Returns albedo-free diffuse (for denoising) and
// full specular radiance (for demodulation on primary hits or direct addition on secondary hits).
void direct_light(Frame f, LightingInputs li, Surface s, vec3 v, bool primary_hit, inout uint seed,
                  out vec3 diffuse, out vec3 specular) {
    diffuse = vec3(0);
    specular = vec3(0);
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    float kd = 1.0 - s.metallic;
    vec3 origin = primary_hit ? primary_ray_origin(f, s.p, s.n) : offset_ray(s.p, s.ng);
    float max_t = f.rt_params.w;

    // Sun: cone of angular half-angle rt_params.x gives physically soft shadows.
    vec3 to_sun = -normalize(f.sun_direction.xyz);
    if (dot(s.n, to_sun) > 0.0 && dot(f.sun_color.rgb, vec3(1)) > 0.0) {
        vec3 l = sample_cone(to_sun, f.rt_params.x, vec2(rnd(seed), rnd(seed)));
        float nl = max(dot(s.n, l), 0.0);
        if (nl > 0.0 && !occluded(li.tlas, li.shapes, li.sdf_count, origin, l, max_t)) {
            vec3 h_d = normalize(v + l);
            vec3 F_d = fresnel_schlick(f0, max(dot(v, h_d), 0.0));
            diffuse += (1.0 - F_d) * kd * nl * f.sun_color.rgb;
            // Evaluate specular along to_sun (or broadened sun cone) to prevent 138x mirror fireflies.
            specular += specular_brdf(s.n, v, to_sun, f0, s.roughness) * f.sun_color.rgb;
        }
    }

    // When spatiotemporal ReSTIR DI is active on primary hits, local point + rect lights are shaded
    // from the finalized ReSTIR reservoir in rt_lighting.comp; secondary hits still evaluate directly.
    if (primary_hit && (f.counts.w & 1u) != 0u) return;

    // Point lights: exact loop for small counts, unbiased RIS for large counts / secondary hits.
    uint pt_count = f.counts.x;
    uint pt_direct_limit = primary_hit ? 4u : 2u;
    if (pt_count <= pt_direct_limit) {
        for (uint i = 0u; i < pt_count; ++i) {
            eval_point_light(li, li.lights.data[i], s, v, origin, f0, kd, 1.0, seed, diffuse, specular);
        }
    } else {
        uint samples = primary_hit ? 2u : 1u;
        float inv_samples = 1.0 / float(samples);
        for (uint k = 0u; k < samples; ++k) {
            float w_sum = 0.0;
            uint chosen = 0u;
            float chosen_w = 0.0;
            for (uint i = 0u; i < pt_count; ++i) {
                float w = point_light_ris_weight(li.lights.data[i], s.p, s.n);
                if (w <= 0.0) continue;
                w_sum += w;
                if (rnd(seed) * w_sum < w) { chosen = i; chosen_w = w; }
            }
            if (chosen_w > 0.0) {
                eval_point_light(li, li.lights.data[chosen], s, v, origin, f0, kd,
                                 (w_sum / chosen_w) * inv_samples, seed, diffuse, specular);
            }
        }
    }

    // Rect area lights: exact loop for <= 2 (primary) / <= 1 (secondary), unbiased RIS otherwise.
    uint rect_count = f.rt_counts.x;
    uint rect_direct_limit = primary_hit ? 2u : 1u;
    if (rect_count <= rect_direct_limit) {
        for (uint i = 0u; i < rect_count; ++i) {
            eval_rect_light(li, li.rects.data[i], s, v, origin, f0, kd, 1.0, 1.0, primary_hit, seed, diffuse, specular);
        }
    } else {
        float w_sum = 0.0;
        uint chosen = 0u;
        float chosen_w = 0.0;
        for (uint i = 0u; i < rect_count; ++i) {
            float w = rect_light_ris_weight(li.rects.data[i], s.p, s.n);
            if (w <= 0.0) continue;
            w_sum += w;
            if (rnd(seed) * w_sum < w) { chosen = i; chosen_w = w; }
        }
        if (chosen_w > 0.0) {
            float prob = chosen_w / w_sum;
            eval_rect_light(li, li.rects.data[chosen], s, v, origin, f0, kd,
                            w_sum / chosen_w, prob, primary_hit, seed, diffuse, specular);
        }
    }
}

// Full outgoing radiance at a secondary hit (reflection, near-GI, or probe ray): direct + DDGI + emission.
// `use_ddgi` MUST be false on frame 0 / post-reset inside ddgi_trace so uninitialized atlases are never read.
vec3 shade_secondary(Frame f, LightingInputs li, Surface s, vec3 v, uint irr_tex, uint dist_tex,
                     bool use_ddgi, inout uint seed) {
    if (s.unshaded) return s.albedo + s.emissive;
    vec3 d, sp;
    direct_light(f, li, s, v, false, seed, d, sp);
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    vec3 dfg = env_brdf(f0, s.roughness, max(dot(s.n, v), 1e-4));
    vec3 indirect = vec3(0);
    vec3 mirror = vec3(0);
    if (use_ddgi) {
        indirect = ddgi_irradiance(f, irr_tex, dist_tex, s.p, s.n, v);
        // Intentional blurry stand-in for 2nd-bounce environment specular (radiance-scaled E/PI).
        mirror = ddgi_irradiance(f, irr_tex, dist_tex, s.p, reflect(-v, s.n), v);
    }
    return s.albedo * (d + indirect * (1.0 - dfg) * (1.0 - s.metallic)) + sp + mirror * dfg + s.emissive;
}

#ifdef ASTRAL_RT_CUSTOM
#include <astral/rt_custom.glsl>
#else
void astral_custom_surface(uint shader, uint base, vec2 uv, vec3 view_dir, inout Surface s) {}
bool astral_custom_alpha_test(uint shader, uint base, vec2 uv, vec3 p, vec3 n, vec3 view_dir, bool backface) { return true; }
#endif

#endif
