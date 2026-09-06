#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba16f) uniform writeonly image2D outImage;

// =================== Dynamic SSBO Edit Buffer ===================
#include "SDFScene.glsl"

layout(std430, binding = 1) readonly buffer EditBuffer {
    SDFPrimitiveRecord edits[];
};

// =================== Two-Level Grid Buffer (PR-6) ===================

layout(std430, binding = 2) readonly buffer GridBuffer {
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

layout(std430, binding = 3) buffer SelectionBuffer {
    SelectionData selection;
};

layout(push_constant) uniform PushConstants {
    vec4 camPos;      // xyz: position, w: near clip
    vec4 camDir;      // xyz: forward direction, w: normalMode (0=central, 1=tetrahedron)
    vec4 screenRes;   // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    vec4 gridParams;  // x: dimX (32), y: dimY (16), z: optShadow (1=on, 0=off), w: cellSize (0.75)
    vec4 taaParams;   // x: jitterX, y: jitterY, z: taaEnabled (1=on, 0=off), w: blendAlpha (0.12)
    vec4 mouseParams; // x: mouseX, y: mouseY, z: pickRequested (0/1), w: pad
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
    const vec3 minB = vec3(-12.0, -1.0, -12.0);
    const vec3 maxB = vec3( 12.0, 11.0,  12.0);

    if (any(lessThan(p, minB)) || any(greaterThan(p, maxB))) {
        return 0.0; // Outside the grid, evaluate the actual SDF.
    }

    vec3 dim = vec3(gridParams.x, gridParams.y, 32.0);
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

// =================== Optimized Lighting & Shadows (PR-7) ===================

float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k, bool opt) {
    float res = 1.0;
    float t = mint;
    const vec3 maxB = vec3(12.0, 11.0, 12.0);

    for (int i = 0; i < 24 && t < maxt; i++) {
        vec3 p = ro + rd * t;

        if (opt) {
            // 1. Sahne ust sinirini astiysa gokyuzundedir (AABB erken cikis)
            if (p.y > 11.0 || any(greaterThan(p.xz, maxB.xz)) || any(lessThan(p.xz, -maxB.xz))) {
                break;
            }

            // 2. Seyrek Izgara Bos Uzay Atlama
            if (screenRes.w > 0.5) {
                float cellD = sampleCoarseGrid(p);
                if (cellD > gridParams.w * 1.2) {
                    t += max(cellD * 0.85, gridParams.w);
                    continue;
                }
            }
        }

        float h = mapScene(p).distance;
        if (h < 0.001) return 0.0;
        res = min(res, k * h / t);
        t += clamp(h, 0.02, 0.4);
    }
    return clamp(res, 0.0, 1.0);
}

float calcAO(vec3 p, vec3 n, bool opt) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.03 + 0.12 * float(i);
        float d = mapScene(p + n * h).distance;
        occ += (h - d) * sca;
        sca *= 0.75;
        // Erken cikis: Duz veya tamamen acik yuzeylerde erken bitir
        if (opt && i == 1 && occ < 0.005) {
            break;
        }
    }
    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// =================== Main Compute ===================

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    vec2 jitterOffset = (taaParams.z > 0.5) ? taaParams.xy : vec2(0.0);
    vec2 uv = (vec2(pixel) + vec2(0.5) + jitterOffset - 0.5 * vec2(res)) / float(res.y);
    uv.y = -uv.y;

    vec3 ro = camPos.xyz;
    vec3 fwd = normalize(camDir.xyz);
    vec3 right = cameraRight.xyz;
    vec3 up = cameraUp.xyz;
    vec3 rd = normalize(fwd * cameraRight.w + right * uv.x + up * uv.y);

    float t = camPos.w;
    float tMax = cameraUp.w;
    SDFHitResult hit;
    bool found = false;

    // Two-Level Hierarchical Raymarching
    for (int i = 0; i < 96 && t < tMax; ++i) {
        vec3 p = ro + rd * t;

        // Seviye 1: Empty Space Skipping (screenRes.w == 1.0 ise)
        if (screenRes.w > 0.5) {
            float cellD = sampleCoarseGrid(p);
            if (cellD > gridParams.w) {
                t += max(cellD * 0.85, gridParams.w);
                continue;
            }
        }

        // Seviye 2: Yuzeye yakin bolgelerde ince hesap
        hit = mapScene(p);

        if (hit.distance < 0.001) {
            found = true;
            break;
        }
        t += hit.distance;
    }

    vec3 color;
    if (found) {
        vec3 p = ro + rd * t;
        vec3 n = (camDir.w > 0.5) ? calcNormalTetrahedron(p) : calcNormalCentral(p);

        bool optShadow = (gridParams.z > 0.5);

        vec3 lightDir = normalize(vec3(0.5, 0.8, -0.4));
        vec3 lightCol = vec3(1.4, 1.3, 1.2);
        vec3 viewDir = -rd;
        vec3 halfDir = normalize(lightDir + viewDir);

        float diff = dot(n, lightDir);
        float spec = 0.0;
        float shadow = 0.0;

        // PR-7: Back-Face Culling for Shadows
        if (diff > 0.001) {
            spec = pow(max(dot(n, halfDir), 0.0), 32.0 * (1.0 - hit.roughness));
            shadow = softShadow(p + n * 0.005, lightDir, 0.02, 15.0, 12.0, optShadow);
        } else if (!optShadow) {
            // Optimizasyon kapaliysa kaba kuvvetle her kosulda golge hesapla
            diff = max(diff, 0.0);
            shadow = softShadow(p + n * 0.005, lightDir, 0.02, 15.0, 12.0, false);
        } else {
            diff = 0.0;
            shadow = 0.0;
        }

        float ao = calcAO(p, n, optShadow);

        vec3 diffuse = hit.albedo * lightCol * diff * shadow;
        vec3 specular = vec3(spec * hit.metallic) * lightCol * shadow;
        vec3 ambient = hit.albedo * vec3(0.08, 0.1, 0.15) * ao;

        color = diffuse + specular + ambient;

        // Secili Obje Fresnel Rim-Light (Kenar Isimasi Vurgusu)
        int selectedIndex = int(round(mouseParams.w));
        if (selectedIndex >= 0 && hit.hitIndex == selectedIndex) {
            float fresnel = 1.0 - max(dot(viewDir, n), 0.0);
            fresnel = pow(fresnel, 3.0);
            vec3 rimColor = vec3(1.0, 0.65, 0.1); // Parlak Turuncu / Altin Isima
            color += rimColor * fresnel * 2.5;
        }
    } else {
        float skyT = 0.5 * (rd.y + 1.0);
        color = mix(vec3(0.15, 0.2, 0.3), vec3(0.02, 0.03, 0.06), skyT);
    }

    // Dogrusal HDR ciktisini yaz (Tonemapping TAAResolve pass'inde yapilir)
    imageStore(outImage, pixel, vec4(color, 1.0));

    // PR-9: Thread-Safe Scene Picking
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
