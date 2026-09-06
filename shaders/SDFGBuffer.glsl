#version 460
//
// SDFGBuffer.glsl — Deferred SDF G-Buffer Compute Shader (Faz 1)
//
// Gorev: Sahne SDF'ini raymarch edip geometrik verileri ayri
// depolama goruntulerune yazar. Shading, golge ve AO YAPILMAZ.
//
// G-Buffer Ciktilari:
//   binding 0: g_Albedo     (rgba8)    — RGB: albedo, A: surface validity
//   binding 1: g_Normal     (rgba16f)  — RGB: world normal, A: expected previous ray distance
//   binding 2: g_Material   (rgba32ui) — X: roughness (floatBits), Y: metallic (floatBits), Z: hitIndex (uint32), W: surfaceId (uint32)
//   binding 3: g_Depth      (r32f)     — R: linear ray distance (t)
//   binding 4: g_Motion     (rg16f)    — RG: 2D screen-space motion vector (UV delta)
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// =================== G-Buffer Output Images ===================

layout(binding = 0, rgba8)   uniform writeonly image2D g_Albedo;
layout(binding = 1, rgba16f) uniform writeonly image2D g_Normal;
// x/y: float bit patterns (roughness/metallic), z: hit index, w: full surface ID.
layout(binding = 2, rgba32ui) uniform writeonly uimage2D g_Material;
layout(binding = 3, r32f)    uniform writeonly image2D g_Depth;
layout(binding = 4, rg16f)   uniform writeonly image2D g_Motion;

// =================== Dynamic SSBO Edit Buffer ===================
#include "SDFScene.glsl"

layout(std430, binding = 5) readonly buffer EditBuffer {
    SDFPrimitiveRecord edits[];
};

// =================== Two-Level Grid Buffer (PR-6) ===================

layout(std430, binding = 6) readonly buffer GridBuffer {
    vec4 gridMinBounds;
    vec4 gridMaxBounds;
    vec4 gridHeaderData; // x: dimX, y: dimY, z: dimZ, w: cellSize
    float cellDistances[];
};

// =================== Selection Buffer (PR-9) ===================

struct SelectionData {
    int hitIndex;
    int pad0;
    int pad1;
    int pad2;
    vec4 hitPoint; // xyz: hitPoint, w: hitDistance
};

layout(std430, binding = 7) buffer SelectionBuffer {
    SelectionData selection;
};

// =================== Camera UBO (Motion Vectors) ===================

layout(std140, binding = 8) uniform CameraUBO {
    mat4 currViewProj;
    mat4 prevViewProj;
    vec4 prevCameraPosition;
    vec4 jitter;
} camera;

// =================== Object Motion History Buffer (Binding 9) ===================

layout(std430, binding = 9) readonly buffer HistoryBuffer {
    mat4 prevTransforms[];
};

// =================== Push Constants (SDFPushConstants — 128 byte) ===================

layout(push_constant) uniform PushConstants {
    vec4 camPos;      // xyz: position, w: near clip
    vec4 camDir;      // xyz: forward direction, w: normalMode (0=central, 1=tetrahedron)
    vec4 screenRes;   // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    vec4 gridParams;  // x: dimX (32), y: dimY (16), z: dimZ, w: cellSize (0.75)
    vec4 taaParams;   // x: jitterX, y: jitterY, z: taaEnabled (1=on, 0=off), w: blendAlpha
    vec4 mouseParams; // x: mouseX, y: mouseY, z: pickRequested (0/1), w: selectedHitIndex
    vec4 cameraRight; // xyz: right, w: focal length
    vec4 cameraUp; // xyz: up, w: far clip
};

// =================== Scene Map ===================

SDFHitResult mapScene(vec3 p) {
    SDFHitResult res;
    res.distance = 1000.0;
    res.surfaceId = 0u;
    res.hitIndex = -1;
    res.albedo = vec3(0.5);
    res.roughness = 0.5;
    res.metallic = 0.0;
    res.confidence = 1.0;

    int editCount = int(screenRes.z);

    if (editCount > 0) {
        for (int i = 0; i < editCount && i < MAX_EDITS; ++i) {
            SDFPrimitiveRecord rec = edits[i];
            float worldDist = evaluatePrimitiveDistance(p, rec);

            combinePrimitive(
                res,
                i == 0,
                worldDist,
                rec.albedoRoughness.rgb,
                rec.albedoRoughness.a,
                rec.metallicParams.x,
                rec.surfaceId,
                i,
                rec.operation,
                rec.metallicParams.y
            );
        }
    }

    return res;
}

// =================== Coarse Grid Sampling (PR-6) ===================

float sampleCoarseGrid(vec3 p) {
    vec3 minB = gridMinBounds.xyz;
    vec3 maxB = gridMaxBounds.xyz;

    if (any(lessThan(p, minB)) || any(greaterThan(p, maxB))) {
        return 0.0; // Outside the grid, evaluate the actual SDF.
    }

    vec3 dim = gridHeaderData.xyz;
    vec3 norm = (p - minB) / (maxB - minB);
    ivec3 cell = clamp(ivec3(norm * dim), ivec3(0), ivec3(dim) - 1);
    int idx = cell.x + int(dim.x) * (cell.y + int(dim.y) * cell.z);
    return cellDistances[idx];
}

// =================== Normal Calculation Stencils ===================

vec3 calcNormalCentral(vec3 p) {
    const float h = 0.001;
    return normalize(vec3(
        mapScene(p + vec3(h, 0.0, 0.0)).distance - mapScene(p - vec3(h, 0.0, 0.0)).distance,
        mapScene(p + vec3(0.0, h, 0.0)).distance - mapScene(p - vec3(0.0, h, 0.0)).distance,
        mapScene(p + vec3(0.0, 0.0, h)).distance - mapScene(p - vec3(0.0, 0.0, h)).distance
    ));
}

vec3 calcNormalTetrahedron(vec3 p) {
    const float h = 0.001;
    const vec3 k0 = vec3( 1.0, -1.0, -1.0);
    const vec3 k1 = vec3(-1.0, -1.0,  1.0);
    const vec3 k2 = vec3(-1.0,  1.0, -1.0);
    const vec3 k3 = vec3( 1.0,  1.0,  1.0);

    return normalize(
        k0 * mapScene(p + k0 * h).distance +
        k1 * mapScene(p + k1 * h).distance +
        k2 * mapScene(p + k2 * h).distance +
        k3 * mapScene(p + k3 * h).distance
    );
}

// =================== Main: G-Buffer Generation ===================

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    // 1. Isin uretimi (jitter dahil — TAA uyumlulugu)
    vec2 jitterOffset = (taaParams.z > 0.5) ? taaParams.xy : vec2(0.0);
    vec2 uv = (vec2(pixel) + vec2(0.5) + jitterOffset - 0.5 * vec2(res)) / float(res.y);
    uv.y = -uv.y;

    vec3 ro = camPos.xyz;
    vec3 fwd = normalize(camDir.xyz);
    vec3 right = cameraRight.xyz;
    vec3 up = cameraUp.xyz;
    vec3 rd = normalize(fwd * cameraRight.w + right * uv.x + up * uv.y);

    // 2. Two-Level Hierarchical Raymarching
    float t = camPos.w;
    float tMax = cameraUp.w;
    SDFHitResult hit;
    bool found = false;
    int maxSteps = int(gridParams.z > 0.5 ? gridParams.z : 96.0);
    for (int i = 0; i < maxSteps && t < tMax; ++i) {
        vec3 p = ro + rd * t;

        // Seviye 1: Empty Space Skipping
        if (screenRes.w > 0.5) {
            float cellD = sampleCoarseGrid(p);
            if (cellD > gridParams.w) {
                t += max(cellD * 0.85, gridParams.w);
                continue;
            }
        }

        // Seviye 2: Ince SDF hesabi
        hit = mapScene(p);

        if (hit.distance < 0.001) {
            found = true;
            break;
        }
        t += hit.distance;
    }

    // 3. G-Buffer ciktilari
    if (found) {
        vec3 hitPos = ro + rd * t;
        vec3 n = (camDir.w > 0.5) ? calcNormalTetrahedron(hitPos) : calcNormalCentral(hitPos);

        // --- Motion Vector Hesabi ---
        // History is accumulated on the unjittered output grid. Sampling jitter
        // must not look like camera motion (a static surface has zero velocity).
        vec2 currUV = (vec2(pixel) + 0.5 + jitterOffset) / vec2(res);
        vec3 prevHitPos = hitPos;
        if (hit.hitIndex >= 0 && hit.hitIndex < int(screenRes.z)) {
            // Static objects have zero object-motion; avoid floating-point matrix roundtrip noise.
            if (edits[hit.hitIndex].metallicParams.w > 0.5) {
                vec3 localP = (edits[hit.hitIndex].invTransform * vec4(hitPos, 1.0)).xyz;
                prevHitPos = (prevTransforms[hit.hitIndex] * vec4(localP, 1.0)).xyz;
            }
        }

        vec4 clipPrev = camera.prevViewProj * vec4(prevHitPos, 1.0);
        vec2 uvPrev = (clipPrev.xy / max(clipPrev.w, 1e-6)) * 0.5 + 0.5;
        vec2 motionVec = currUV - uvPrev;

        // --- G-Buffer Yazimi ---
        imageStore(g_Albedo,   pixel, vec4(hit.albedo, hit.confidence));
        imageStore(g_Normal,   pixel, vec4(n, clipPrev.w > 0.0 ? distance(prevHitPos, camera.prevCameraPosition.xyz) : -1.0));
        imageStore(g_Material, pixel, uvec4(floatBitsToUint(hit.roughness), floatBitsToUint(hit.metallic), uint(hit.hitIndex), hit.surfaceId));
        imageStore(g_Depth,    pixel, vec4(t, 0.0, 0.0, 0.0));
        imageStore(g_Motion,   pixel, vec4(motionVec, 0.0, 0.0));
    } else {
        // Gokyuzu / bosluk — sentinel degerler
        float skyT = 0.5 * (rd.y + 1.0);
        vec3 skyColor = mix(vec3(0.15, 0.2, 0.3), vec3(0.02, 0.03, 0.06), skyT);
        imageStore(g_Albedo,   pixel, vec4(skyColor, 0.0));     // A=0: gokyuzu maskesi
        imageStore(g_Normal,   pixel, vec4(0.0));
        imageStore(g_Material, pixel, uvec4(0u));
        imageStore(g_Depth,    pixel, vec4(0.0));
        imageStore(g_Motion,   pixel, vec4(0.0));
    }

    // 4. PR-9: Thread-Safe Scene Picking
    if (mouseParams.z > 0.5 && pixel == ivec2(mouseParams.xy)) {
        if (found) {
            selection.hitIndex = hit.hitIndex;
            selection.hitPoint = vec4(ro + rd * t, t);
        } else {
            selection.hitIndex = -1;
            selection.hitPoint = vec4(0.0);
        }
    }
}
