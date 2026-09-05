#version 460
//
// SDFGBuffer.glsl — Deferred SDF G-Buffer Compute Shader (Faz 1)
//
// Gorev: Sahne SDF'ini raymarch edip geometrik verileri ayri
// depolama goruntulerune yazar. Shading, golge ve AO YAPILMAZ.
//
// G-Buffer Ciktilari:
//   binding 0: g_Albedo     (rgba8)   — RGB: albedo, A: materialID / 255
//   binding 1: g_Normal     (rgba16f) — RGB: world normal (ham, encode yok), A: 1.0
//   binding 2: g_Material   (rgba8)   — R: roughness, G: metallic, B: hitIndex/255, A: 0
//   binding 3: g_Depth      (r32f)    — R: linear ray distance (t)
//   binding 4: g_Motion     (rg16f)   — RG: 2D screen-space motion vector (UV delta)
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// =================== G-Buffer Output Images ===================

layout(binding = 0, rgba8)   uniform writeonly image2D g_Albedo;
layout(binding = 1, rgba16f) uniform writeonly image2D g_Normal;
layout(binding = 2, rgba8)   uniform writeonly image2D g_Material;
layout(binding = 3, r32f)    uniform writeonly image2D g_Depth;
layout(binding = 4, rg16f)   uniform writeonly image2D g_Motion;

// =================== Dynamic SSBO Edit Buffer ===================
#define MAX_EDITS 256

struct SDFEditGPU {
    vec3 position; float pad1;
    vec4 rotation; // quaternion (x, y, z, w)
    vec3 scale; uint primitiveType;
    uint operation; float blendFactor; uint isDynamic; float pad2;
    vec3 albedo; float roughness;
    float metallic;
    float prevPosX;
    float prevPosY;
    float prevPosZ;
};

layout(std430, binding = 5) readonly buffer EditBuffer {
    SDFEditGPU edits[];
};

// =================== Two-Level Grid Buffer (PR-6) ===================

layout(std430, binding = 6) readonly buffer GridBuffer {
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
} camera;

// =================== Push Constants (SDFPushConstants — 96 byte) ===================

layout(push_constant) uniform PushConstants {
    vec4 camPos;      // xyz: position, w: time
    vec4 camDir;      // xyz: forward direction, w: normalMode (0=central, 1=tetrahedron)
    vec4 screenRes;   // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    vec4 gridParams;  // x: dimX (32), y: dimY (16), z: dimZ, w: cellSize (0.75)
    vec4 taaParams;   // x: jitterX, y: jitterY, z: taaEnabled (1=on, 0=off), w: blendAlpha
    vec4 mouseParams; // x: mouseX, y: mouseY, z: pickRequested (0/1), w: selectedHitIndex
};

// =================== Quaternion Vector Rotation ===================

vec3 rotateVec(vec3 v, vec4 q) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

vec3 invRotateVec(vec3 v, vec4 q) {
    return rotateVec(v, vec4(-q.xyz, q.w));
}

// =================== SDF Primitives ===================

float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sdPlane(vec3 p, vec3 n, float h) {
    return dot(p, n) + h;
}

float sdCapsule(vec3 p, float h, float r) {
    p.y -= clamp(p.y, 0.0, h);
    return length(p) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// =================== CSG Operators ===================

float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSub(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d1 + d2) / k, 0.0, 1.0);
    return mix(d1, -d2, h) + k * h * (1.0 - h);
}

struct HitInfo {
    float dist;
    int index;
    vec3 albedo;
    float roughness;
    float metallic;
};

// =================== Single Primitive Evaluation ===================

float evalPrimitive(vec3 p, SDFEditGPU e) {
    vec3 lp = p - e.position;
    if (dot(e.rotation, e.rotation) > 0.001) {
        lp = invRotateVec(lp, e.rotation);
    }

    switch (e.primitiveType) {
        case 0: return sdSphere(lp, e.scale.x);
        case 1: return sdBox(lp, e.scale);
        case 2: return sdTorus(lp, vec2(e.scale.x, e.scale.y));
        case 3: return sdPlane(lp, vec3(0.0, 1.0, 0.0), e.scale.y);
        case 4: return sdCapsule(lp, e.scale.y, e.scale.x);
        case 5: return sdCylinder(lp, e.scale.y, e.scale.x);
        default: return sdSphere(lp, e.scale.x);
    }
}

// =================== Scene Map ===================

HitInfo mapScene(vec3 p) {
    HitInfo res;
    res.dist = 1000.0;
    res.index = -1;
    res.albedo = vec3(0.5);
    res.roughness = 0.5;
    res.metallic = 0.0;

    int editCount = int(screenRes.z);

    if (editCount > 0) {
        for (int i = 0; i < editCount && i < MAX_EDITS; ++i) {
            SDFEditGPU e = edits[i];
            float d = evalPrimitive(p, e);

            if (i == 0) {
                res.dist = d;
                res.index = 0;
                res.albedo = e.albedo;
                res.roughness = e.roughness;
                res.metallic = e.metallic;
            } else {
                float k = max(e.blendFactor, 0.001);
                float h = clamp(0.5 + 0.5 * (res.dist - d) / k, 0.0, 1.0);

                switch (e.operation) {
                    case 0: // Union
                        if (d < res.dist) {
                            res.dist = d;
                            res.index = i;
                            res.albedo = e.albedo;
                            res.roughness = e.roughness;
                            res.metallic = e.metallic;
                        }
                        break;
                    case 1: // Subtract
                        res.dist = max(res.dist, -d);
                        break;
                    case 2: // Intersect
                        if (d > res.dist) {
                            res.dist = d;
                            res.index = i;
                            res.albedo = e.albedo;
                            res.roughness = e.roughness;
                            res.metallic = e.metallic;
                        }
                        break;
                    case 3: // Smooth Union
                        res.dist = mix(res.dist, d, h) - k * h * (1.0 - h);
                        if (h > 0.5) {
                            res.index = i;
                        }
                        res.albedo = mix(res.albedo, e.albedo, h);
                        res.roughness = mix(res.roughness, e.roughness, h);
                        res.metallic = mix(res.metallic, e.metallic, h);
                        break;
                    case 4: // Smooth Subtract
                        float hs = clamp(0.5 - 0.5 * (res.dist + d) / k, 0.0, 1.0);
                        res.dist = mix(res.dist, -d, hs) + k * hs * (1.0 - hs);
                        break;
                }
            }
        }
        return res;
    }

    return res;
}

// =================== Coarse Grid Sampling (PR-6) ===================

float sampleCoarseGrid(vec3 p) {
    const vec3 minB = vec3(-12.0, -1.0, -12.0);
    const vec3 maxB = vec3( 12.0, 11.0,  12.0);

    if (any(lessThan(p, minB)) || any(greaterThan(p, maxB))) {
        return 10.0;
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
        mapScene(p + vec3(h, 0.0, 0.0)).dist - mapScene(p - vec3(h, 0.0, 0.0)).dist,
        mapScene(p + vec3(0.0, h, 0.0)).dist - mapScene(p - vec3(0.0, h, 0.0)).dist,
        mapScene(p + vec3(0.0, 0.0, h)).dist - mapScene(p - vec3(0.0, 0.0, h)).dist
    ));
}

vec3 calcNormalTetrahedron(vec3 p) {
    const float h = 0.001;
    const vec3 k0 = vec3( 1.0, -1.0, -1.0);
    const vec3 k1 = vec3(-1.0, -1.0,  1.0);
    const vec3 k2 = vec3(-1.0,  1.0, -1.0);
    const vec3 k3 = vec3( 1.0,  1.0,  1.0);

    return normalize(
        k0 * mapScene(p + k0 * h).dist +
        k1 * mapScene(p + k1 * h).dist +
        k2 * mapScene(p + k2 * h).dist +
        k3 * mapScene(p + k3 * h).dist
    );
}

// =================== Main: G-Buffer Generation ===================

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    // 1. Isin uretimi (jitter dahil — TAA uyumlulugu)
    vec2 jitterOffset = (taaParams.z > 0.5) ? taaParams.xy : vec2(0.0);
    vec2 uv = (vec2(pixel) + jitterOffset - 0.5 * vec2(res)) / float(res.y);
    uv.y = -uv.y;

    vec3 ro = camPos.xyz;
    vec3 fwd = normalize(camDir.xyz);
    vec3 right = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, fwd);
    vec3 rd = normalize(fwd * 1.5 + right * uv.x + up * uv.y);

    // 2. Two-Level Hierarchical Raymarching
    float t = 0.01;
    float tMax = 50.0;
    HitInfo hit;
    bool found = false;

    for (int i = 0; i < 96 && t < tMax; ++i) {
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

        if (hit.dist < 0.001) {
            found = true;
            break;
        }
        t += hit.dist;
    }

    // 3. G-Buffer ciktilari
    if (found) {
        vec3 hitPos = ro + rd * t;
        vec3 n = (camDir.w > 0.5) ? calcNormalTetrahedron(hitPos) : calcNormalCentral(hitPos);

        // --- Motion Vector Hesabi ---
        // Jitter'siz piksel UV'si (ekran uzayi [0,1])
        vec2 currUV = (vec2(pixel) + 0.5) / vec2(res);

        // Nesne bazli hareket: Vurulan nesne dinamik ise onceki konumunu hesaba kat
        vec3 prevHitPos = hitPos;
        if (hit.index >= 0 && hit.index < int(screenRes.z)) {
            SDFEditGPU e = edits[hit.index];
            if (e.isDynamic == 1u) {
                vec3 prevPos = vec3(e.prevPosX, e.prevPosY, e.prevPosZ);
                vec3 deltaPos = e.position - prevPos;
                prevHitPos = hitPos - deltaPos;
            }
        }

        // Onceki kare VP matrisi ile isabet noktasini yansit
        vec4 clipPrev = camera.prevViewProj * vec4(prevHitPos, 1.0);
        vec2 uvPrev = (clipPrev.xy / clipPrev.w) * 0.5 + 0.5;
        vec2 motionVec = currUV - uvPrev;

        // --- G-Buffer Yazimi ---
        imageStore(g_Albedo,   pixel, vec4(hit.albedo, float(hit.index) / 255.0));
        imageStore(g_Normal,   pixel, vec4(n, 1.0));  // Ham world-space normal (encode yok)
        imageStore(g_Material, pixel, vec4(hit.roughness, hit.metallic, float(hit.index) / 255.0, 0.0));
        imageStore(g_Depth,    pixel, vec4(t, 0.0, 0.0, 0.0));
        imageStore(g_Motion,   pixel, vec4(motionVec, 0.0, 0.0));
    } else {
        // Gokyuzu / bosluk — sentinel degerler
        float skyT = 0.5 * (rd.y + 1.0);
        vec3 skyColor = mix(vec3(0.15, 0.2, 0.3), vec3(0.02, 0.03, 0.06), skyT);
        imageStore(g_Albedo,   pixel, vec4(skyColor, 0.0));     // A=0: gokyuzu maskesi
        imageStore(g_Normal,   pixel, vec4(0.0));
        imageStore(g_Material, pixel, vec4(0.0));
        imageStore(g_Depth,    pixel, vec4(0.0));
        imageStore(g_Motion,   pixel, vec4(0.0));
    }

    // 4. PR-9: Thread-Safe Scene Picking
    if (mouseParams.z > 0.5 && pixel == ivec2(mouseParams.xy)) {
        if (found) {
            selection.hitIndex = hit.index;
            selection.hitPoint = vec4(ro + rd * t, t);
        } else {
            selection.hitIndex = -1;
            selection.hitPoint = vec4(0.0);
        }
    }
}
