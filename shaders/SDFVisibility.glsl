#ifndef SDF_VISIBILITY_GLSL
#define SDF_VISIBILITY_GLSL

//
// SDFVisibility.glsl — Analytical SDF Cone-Traced Soft Shadows & AO
//
// Shared analytical visibility module for Deferred Lighting & compute passes.
// Adheres strictly to A4 specification:
// - Directional & Point light cone-traced soft shadows
// - Objects behind point lights do NOT cast shadows (clamped to distToLight)
// - Analytical Multi-tap Ambient Occlusion along surface normal
// - Safe fallbacks on NaN / Inf
// - Optional Two-Level BrickGrid coarse acceleration
//

#include "SDFScene.glsl"

float evaluateSceneDistance(vec3 p, int editCount) {
    float dist = 1000.0;
    if (editCount <= 0) return dist;

    for (int i = 0; i < editCount && i < MAX_EDITS; ++i) {
        SDFPrimitiveRecord rec = edits[i];
        float d = evaluatePrimitiveDistance(p, rec);
        dist = combineDistance(dist, i == 0, d, rec.operation, rec.metallicParams.y);
    }
    return dist;
}

float sampleCoarseGrid(vec3 p, vec3 dim, float cellSize) {
    const vec3 minB = vec3(-12.0, -1.0, -12.0);
    const vec3 maxB = vec3( 12.0, 11.0,  12.0);

    if (any(lessThan(p, minB)) || any(greaterThan(p, maxB))) {
        return 0.0;
    }
    vec3 norm = (p - minB) / (maxB - minB);
    ivec3 cell = clamp(ivec3(norm * dim), ivec3(0), ivec3(dim) - 1);
    int idx = cell.x + int(dim.x) * (cell.y + int(dim.y) * cell.z);
    return cellDistances[idx];
}

float evaluateSDFSoftShadow(
    vec3 ro,
    vec3 rd,
    float mint,
    float maxt,
    float k,
    int maxSteps,
    int editCount,
    bool opt,
    bool useGrid,
    vec3 gridDim,
    float cellSize
) {
    if (editCount <= 0) return 1.0;
    if (isnan(ro.x) || isnan(ro.y) || isnan(ro.z) || isnan(rd.x) || isnan(rd.y) || isnan(rd.z)) return 1.0;

    float res = 1.0;
    float t = max(mint, 0.001);
    const vec3 maxB = vec3(12.0, 11.0, 12.0);

    for (int i = 0; i < maxSteps && t < maxt; ++i) {
        vec3 p = ro + rd * t;

        if (opt) {
            // AABB erken cikis: Sahne tavanini astiysa gokyuzundedir
            if (p.y > 11.0 || any(greaterThan(p.xz, maxB.xz)) || any(lessThan(p.xz, -maxB.xz))) {
                break;
            }

            // Seyrek Izgara Bos Uzay Atlama
            if (useGrid) {
                float cellD = sampleCoarseGrid(p, gridDim, cellSize);
                if (cellD > cellSize * 1.2) {
                    t += max(cellD * 0.85, cellSize);
                    continue;
                }
            }
        }

        float h = evaluateSceneDistance(p, editCount);
        if (isnan(h) || isinf(h)) return 1.0;

        if (h < 0.001) {
            return 0.0; // Blocker ile tam temas
        }

        res = min(res, k * h / t);
        t += clamp(h, 0.015, 0.5);
    }

    return clamp(res, 0.0, 1.0);
}

float evaluateSDFAO(
    vec3 pos,
    vec3 N,
    int samples,
    float radius,
    float surfaceBias,
    int editCount
) {
    if (editCount <= 0 || samples <= 0) return 1.0;
    if (isnan(pos.x) || isnan(pos.y) || isnan(pos.z) || isnan(N.x) || isnan(N.y) || isnan(N.z)) return 1.0;

    float occ = 0.0;
    float weight = 1.0;
    float totalWeight = 0.0;

    for (int i = 0; i < samples; ++i) {
        float h = radius * (float(i + 1) / float(samples));
        vec3 samplePos = pos + N * (surfaceBias + h);
        float d = evaluateSceneDistance(samplePos, editCount);
        if (isnan(d) || isinf(d)) continue;

        occ += max(0.0, h - d) * weight;
        totalWeight += h * weight;
        weight *= 0.75;
    }

    float ao = 1.0 - (occ / max(totalWeight, 1e-4));
    return clamp(ao, 0.0, 1.0);
}

#endif // SDF_VISIBILITY_GLSL
