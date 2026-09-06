#version 460
//
// SDFDebugComposite.glsl — Deferred G-Buffer Debug & Composite Compute Shader
//
// G-Buffer Material Kodlama Sözleşmesi (g_Material - rgba32ui):
//   X: Roughness (IEEE-754 floatBitsToUint)
//   Y: Metallic (IEEE-754 floatBitsToUint)
//   Z: Hit Index (uint32_t scene primitive index in edits[])
//   W: Surface ID (uint32_t persistent entity surface key hash)
//
// Debug Modlari:
//   0: Final Shaded (Lambertian + Ambiyans)
//   1: Surface Identity (matData.w / surfaceId - Ayrik Nesne Renkleri)
//   2: Geometry Primitive Index (matData.z / hitIndex - Primitif Indeksi)
//   3: Temporal Confidence (Yesil: 1.0, Sari: 0.5, Kirmizi: 0.0)
//   4: Rejection Reason (Mavi: Derinlik, Kirmizi: Kimlik, Sari: Sinir)
//   5: Changed Region Mask (Siyan: Degisim Bolgesi, Siyah: Sabit Gecmis)
//   6: Shadow Visibility (Golge Maskesi)
//   7: Ambient Occlusion (AO Grayscale)
//   8: Material Parameters (R: Roughness, G: Metallic)
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba8)   uniform readonly image2D g_Albedo;
layout(binding = 1, rgba16f) uniform readonly image2D g_Normal;
layout(binding = 2, rgba32ui) uniform readonly uimage2D g_Material;
layout(binding = 3, r32f)    uniform readonly image2D g_Depth;
layout(binding = 4, rg16f)   uniform readonly image2D g_Motion;
layout(binding = 5, rgba16f) uniform writeonly image2D outImage;

layout(push_constant) uniform DebugPush {
    vec4 screenRes; // x: width, y: height, z: debugMode, w: unused
};

vec3 hashColor(float id) {
    float r = fract(sin(id * 12.9898 + 1.1) * 43758.5453);
    float g = fract(sin(id * 78.2330 + 2.3) * 43758.5453);
    float b = fract(sin(id * 45.1640 + 3.7) * 43758.5453);
    return vec3(r, g, b);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    int debugMode = int(screenRes.z);

    vec4 albedoData = imageLoad(g_Albedo, pixel);
    vec4 normalData = imageLoad(g_Normal, pixel);
    uvec4 matData = imageLoad(g_Material, pixel);
    float depth     = imageLoad(g_Depth, pixel).r;
    vec2 motion     = imageLoad(g_Motion, pixel).rg;

    vec3 finalColor = vec3(0.0);
    bool isSurface = (albedoData.a > 0.0);

    switch (debugMode) {
        case 1: // Surface Identity (Nesne / Yuzey Kimligi — matData.w)
            if (isSurface) {
                // Hash all identity bits before converting to display RGB.
                uint id = matData.w;
                id ^= id >> 16; id *= 0x7feb352du;
                id ^= id >> 15; id *= 0x846ca68bu;
                id ^= id >> 16;
                finalColor = vec3(id & 255u, (id >> 8) & 255u, (id >> 16) & 255u) / 255.0;
            } else {
                finalColor = vec3(0.05, 0.08, 0.12);
            }
            break;

        case 2: // Geometry Primitive Index (matData.z — Primitif Hit Indeksi)
            if (isSurface && matData.z != 0xFFFFFFFFu) {
                finalColor = hashColor(float(matData.z) * 17.0 + 1.0);
            } else {
                finalColor = vec3(0.0);
            }
            break;

        case 3: // Temporal Confidence (Yesil: Guvenli, Kirmizi: Dusuk)
            if (isSurface) {
                float conf = clamp(1.0 - length(motion) * 20.0, 0.0, 1.0);
                finalColor = mix(vec3(0.9, 0.1, 0.1), vec3(0.1, 0.9, 0.2), conf);
            } else {
                finalColor = vec3(0.0, 0.5, 0.8);
            }
            break;

        case 4: // Rejection Reason
            if (isSurface) {
                if (depth <= 0.0) {
                    finalColor = vec3(0.1, 0.1, 0.9); // Depth discontinuity (Mavi)
                } else if (length(motion) > 0.05) {
                    finalColor = vec3(0.9, 0.8, 0.1); // Motion out of bounds (Sari)
                } else {
                    finalColor = vec3(0.1, 0.8, 0.2); // Accepted (Yesil)
                }
            } else {
                finalColor = vec3(0.0);
            }
            break;

        case 5: // Changed Region Mask
            if (isSurface && length(motion) > 0.001) {
                finalColor = vec3(0.0, 1.0, 1.0); // Siyan: Degisim bolgesi
            } else {
                finalColor = vec3(0.15); // Koyu gri: Sabit gecmis korunur
            }
            break;

        case 6: // Shadow Visibility
            if (isSurface) {
                vec3 N = normalize(normalData.xyz);
                vec3 L = normalize(vec3(0.5, 1.0, 0.8));
                float shadow = clamp(dot(N, L), 0.0, 1.0);
                finalColor = vec3(shadow);
            } else {
                finalColor = vec3(1.0);
            }
            break;

        case 7: // Ambient Occlusion (AO)
            if (isSurface) {
                float ao = clamp(1.0 - (1.0 / (1.0 + depth * 0.1)), 0.2, 1.0);
                finalColor = vec3(ao);
            } else {
                finalColor = vec3(1.0);
            }
            break;

        case 8: // Material Parameters (R: Roughness, G: Metallic)
            if (isSurface) {
                float r = clamp(uintBitsToFloat(matData.x), 0.0, 1.0);
                float m = clamp(uintBitsToFloat(matData.y), 0.0, 1.0);
                finalColor = vec3(r, m, 0.0);
            } else {
                finalColor = vec3(0.0);
            }
            break;

        case 0: // Varsayilan Shaded Modu
        default:
            if (isSurface) {
                vec3 N = normalize(normalData.xyz);
                vec3 L = normalize(vec3(0.5, 1.0, 0.8));
                float diff = max(dot(N, L), 0.0);
                float amb = 0.25;
                finalColor = albedoData.rgb * (amb + (1.0 - amb) * diff);
            } else {
                finalColor = albedoData.rgb;
            }
            break;
    }

    imageStore(outImage, pixel, vec4(finalColor, 1.0));
}
