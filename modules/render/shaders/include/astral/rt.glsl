// Ray-traced lighting helpers: random numbers, ray queries against the TLAS (+ SDF scene),
// hit surface reconstruction, light sampling, GGX, octahedral maps and DDGI probe lookups.
#ifndef ASTRAL_RT_GLSL
#define ASTRAL_RT_GLSL
#extension GL_EXT_ray_query : require
#include <astral/sdf.glsl>

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

// --- GGX ----------------------------------------------------------------------------------
float ggx_d(float nh, float a) {
    float a2 = a * a;
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float smith_g1(float nx, float a) {
    float a2 = a * a;
    return 2.0 * nx / (nx + sqrt(a2 + (1.0 - a2) * nx * nx));
}
vec3 fresnel(vec3 f0, float c) { return f0 + (1.0 - f0) * pow(1.0 - c, 5.0); }
// Pre-integrated specular reflectance (Karis' analytic fit of the split-sum DFG term).
vec3 env_brdf(vec3 f0, float roughness, float nv) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}
// Visible-normal sampling (Heitz 2018). `v` in the local frame (z = normal).
vec3 sample_vndf(vec3 v, float a, vec2 u) {
    vec3 vh = normalize(vec3(a * v.x, a * v.y, v.z));
    float len2 = vh.x * vh.x + vh.y * vh.y;
    vec3 t1 = len2 > 0.0 ? vec3(-vh.y, vh.x, 0.0) * inversesqrt(len2) : vec3(1, 0, 0);
    vec3 t2 = cross(vh, t1);
    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * sqrt(max(0.0, 1.0 - p1 * p1)) + s * p2;
    vec3 nh = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return normalize(vec3(a * nh.x, a * nh.y, max(0.0, nh.z)));
}

// Specular part of the engine's GGX BRDF times n.l (lights are pre-multiplied by PI).
vec3 specular_brdf(vec3 n, vec3 v, vec3 l, vec3 f0, float roughness) {
    vec3 h = normalize(v + l);
    float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 1e-4), nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
    float a = max(roughness * roughness, 0.002);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float g = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
    return ggx_d(nh, a) * g * fresnel(f0, vh) / (4.0 * nv * nl + 1e-4) * nl * PI;
}

// --- Surfaces -----------------------------------------------------------------------------
struct Surface {
    vec3 p;
    vec3 n;         // shading normal, facing the incoming ray
    vec3 albedo;
    float roughness;
    float metallic;
    vec3 emissive;
    bool backface;  // the ray hit the back of a mesh triangle
};

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
    mat3 normal_matrix = transpose(inverse(mat3(inst.model)));
    vec3 n = normalize(normal_matrix * (v0.normal * b0 + v1.normal * bary.x + v2.normal * bary.y));
    vec3 p0 = (inst.model * vec4(v0.position, 1)).xyz;
    vec3 p1 = (inst.model * vec4(v1.position, 1)).xyz;
    vec3 p2 = (inst.model * vec4(v2.position, 1)).xyz;
    vec3 ng = normalize(cross(p1 - p0, p2 - p0)); // counter-clockwise = front, like the rasterizer
    Surface s;
    s.p = origin + dir * t;
    s.backface = dot(ng, dir) > 0.0;
    s.n = dot(n, dir) > 0.0 ? -n : n;
    s.albedo = mat.base_color.rgb;
    s.roughness = clamp(mat.emissive_roughness.a, 0.02, 1.0);
    s.metallic = mat.params.x;
    s.emissive = mat.emissive_roughness.rgb;
    return s;
}

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
    rayQueryEXT q;
    rayQueryInitializeEXT(q, accelerationStructureEXT(tlas), gl_RayFlagsOpaqueEXT, 0xFF, o, 0.0, d, tmax);
    while (rayQueryProceedEXT(q)) {}
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
            t += abs(dist);
        }
    }
    return h;
}

bool occluded(uint64_t tlas, SdfShapes shapes, uint sdf_count, vec3 o, vec3 d, float tmax) {
    rayQueryEXT q;
    rayQueryInitializeEXT(q, accelerationStructureEXT(tlas),
                          gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, o, 0.0, d, tmax);
    while (rayQueryProceedEXT(q)) {}
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
    s.n = dot(n, d) > 0.0 ? -n : n;
    s.albedo = h.albedo;
    s.roughness = clamp(h.roughness, 0.02, 1.0);
    s.metallic = h.metallic;
    s.emissive = vec3(0);
    s.backface = false;
    return s;
}

// Distance along the ray to a rect light (or -1), and its radiance.
float intersect_rect(RectLight l, vec3 o, vec3 d) {
    vec3 n = normalize(cross(l.right.xyz, l.up.xyz));
    float dn = dot(d, n);
    if (abs(dn) < 1e-6) return -1.0;
    if (l.position_two_sided.w < 0.5 && dn > 0.0) return -1.0; // seen from behind
    float t = dot(l.position_two_sided.xyz - o, n) / dn;
    if (t <= 0.0) return -1.0;
    vec3 local = o + d * t - l.position_two_sided.xyz;
    float u = dot(local, l.right.xyz) / dot(l.right.xyz, l.right.xyz);
    float v = dot(local, l.up.xyz) / dot(l.up.xyz, l.up.xyz);
    return abs(u) <= 1.0 && abs(v) <= 1.0 ? t : -1.0;
}

vec3 sky_radiance(Frame f, vec3 dir) {
    return mix(f.ground_color.rgb, f.sky_color.rgb, smoothstep(-0.2, 0.4, dir.y));
}

// --- Octahedral mapping --------------------------------------------------------------------
vec2 sign_not_zero(vec2 v) { return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }
vec2 oct_encode(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
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

// Irradiance (cosine-weighted average radiance) at `p` with normal `n`; `v` points to the viewer.
vec3 ddgi_irradiance(Frame f, uint irr_tex, uint dist_tex, vec3 p, vec3 n, vec3 v) {
    if (f.ddgi_counts.w == 0u) return vec3(0);
    vec3 spacing = f.ddgi_spacing.xyz;
    float min_spacing = min(spacing.x, min(spacing.y, spacing.z));
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
        float w = (dot(normalize(probe - p), n) + 1.0) * 0.5;
        w = w * w + 0.2;
        // Chebyshev visibility from the probe's distance moments.
        vec2 moments = astral_sample(dist_tex, ASTRAL_SAMPLER_LINEAR_CLAMP, ddgi_uv(f, index, -dir, DDGI_DIST)).rg;
        if (r > moments.x) {
            float variance = abs(moments.x * moments.x - moments.y);
            float diff = r - moments.x;
            float cheb = variance / (variance + diff * diff);
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

// --- Direct lighting -------------------------------------------------------------------------
struct LightingInputs {
    uint64_t tlas;
    SdfShapes shapes;
    uint sdf_count;
    PointLights lights;
    RectLights rects;
};

// One stochastic sample per light, visibility by shadow ray. Returns the diffuse part divided by
// albedo (`diffuse`, for denoising) and the full specular part of the sun and point lights
// (rect light highlights come from reflection rays that hit the rect).
void direct_light(Frame f, LightingInputs li, Surface s, vec3 v, inout uint seed, out vec3 diffuse, out vec3 specular) {
    diffuse = vec3(0);
    specular = vec3(0);
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    float kd = 1.0 - s.metallic;
    vec3 origin = s.p + s.n * 0.002;
    float max_t = f.rt_params.w;

    // Sun: a cone of the sun's angular radius gives physically soft shadows.
    vec3 to_sun = -normalize(f.sun_direction.xyz);
    if (dot(s.n, to_sun) > 0.0 && dot(f.sun_color.rgb, vec3(1)) > 0.0) {
        vec3 l = sample_cone(to_sun, f.rt_params.x, vec2(rnd(seed), rnd(seed)));
        float nl = max(dot(s.n, l), 0.0);
        if (nl > 0.0 && !occluded(li.tlas, li.shapes, li.sdf_count, origin, l, max_t)) {
            vec3 h = normalize(v + to_sun);
            vec3 F = fresnel(f0, max(dot(v, h), 0.0));
            diffuse += (1.0 - F) * kd * nl * f.sun_color.rgb;
            specular += specular_brdf(s.n, v, to_sun, f0, s.roughness) * f.sun_color.rgb;
        }
    }
    // Point lights (same falloff as the raster path), shadow ray toward a point on the bulb.
    for (uint i = 0u; i < f.counts.x; ++i) {
        PointLight light = li.lights.data[i];
        vec3 to = light.position_radius.xyz - s.p;
        float dist = max(length(to), 1e-4);
        float radius = max(light.position_radius.w, 1e-4);
        if (dist >= radius) continue;
        vec3 l = to / dist;
        float nl = dot(s.n, l);
        if (nl <= 0.0) continue;
        float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
        falloff = falloff * falloff / (dist * dist + 1.0);
        vec3 radiance = light.color_intensity.rgb * light.color_intensity.a * falloff;
        vec2 u = vec2(rnd(seed), rnd(seed));
        vec3 jitter = sample_cone(-l, PI * 0.5, u) * light.source.x * sqrt(rnd(seed));
        vec3 target = light.position_radius.xyz + jitter;
        vec3 sd = target - origin;
        float sl = length(sd);
        if (occluded(li.tlas, li.shapes, li.sdf_count, origin, sd / sl, sl - light.source.x * 0.5)) continue;
        vec3 h = normalize(v + l);
        vec3 F = fresnel(f0, max(dot(v, h), 0.0));
        diffuse += (1.0 - F) * kd * nl * radiance;
        specular += specular_brdf(s.n, v, l, f0, s.roughness) * radiance;
    }
    // Rect area lights: one uniform area sample, converted to solid angle.
    for (uint i = 0u; i < f.rt_counts.x; ++i) {
        RectLight r = li.rects.data[i];
        vec2 u = vec2(rnd(seed), rnd(seed)) * 2.0 - 1.0;
        vec3 lp = r.position_two_sided.xyz + r.right.xyz * u.x + r.up.xyz * u.y;
        vec3 to = lp - origin;
        float d2 = dot(to, to);
        float dist = sqrt(d2);
        vec3 l = to / dist;
        float nl = dot(s.n, l);
        vec3 ln = cross(r.right.xyz, r.up.xyz);
        float area = 4.0 * length(ln);
        ln = normalize(ln);
        float cos_l = dot(ln, -l);
        if (r.position_two_sided.w > 0.5) cos_l = abs(cos_l);
        if (nl <= 0.0 || cos_l <= 0.0) continue;
        if (occluded(li.tlas, li.shapes, li.sdf_count, origin, l, dist - 0.001)) continue;
        vec3 h = normalize(v + l);
        vec3 F = fresnel(f0, max(dot(v, h), 0.0));
        diffuse += (1.0 - F) * kd * nl * r.radiance.rgb * cos_l * area / (PI * d2);
    }
}

// Full outgoing radiance at a secondary hit (reflection or probe ray): direct + DDGI + emission.
vec3 shade_secondary(Frame f, LightingInputs li, Surface s, vec3 v, uint irr_tex, uint dist_tex, inout uint seed) {
    vec3 d, sp;
    direct_light(f, li, s, v, seed, d, sp);
    vec3 indirect = ddgi_irradiance(f, irr_tex, dist_tex, s.p, s.n, v);
    // Environment specular at secondary hits: probe light seen in the mirror direction is a
    // blurry stand-in for a second reflection ray (keeps metals from going black in reflections).
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    vec3 dfg = env_brdf(f0, s.roughness, max(dot(s.n, v), 1e-4));
    vec3 mirror = ddgi_irradiance(f, irr_tex, dist_tex, s.p, reflect(-v, s.n), v);
    return s.albedo * (d + indirect * (1.0 - dfg) * (1.0 - s.metallic)) + sp + mirror * dfg + s.emissive;
}

#endif
