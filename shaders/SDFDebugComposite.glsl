#version 460
//
// SDFDebugComposite.glsl — Deferred G-Buffer Debug & Composite Compute Shader (Faz 1)
//
// Gorev: G-Buffer ciktilarini (Albedo, Normal, Material, Depth, Motion)
// okuyup hata ayiklama / onizleme modlarina gore tek bir cikti goruntusune
// (outImage) yazar.
//
// Baglantilar:
//   binding 0: g_Albedo    (rgba8)   — Albedo ve nesne maskesi
//   binding 1: g_Normal    (rgba16f) — Ham dunya uzayi normali
//   binding 2: g_Material  (rgba8)   — Roughness, Metallic, HitIndex
//   binding 3: g_Depth     (r32f)    — Dogrusal isin mesafesi (t)
//   binding 4: g_Motion    (rg16f)   — 2D Ekran uzayi hareket vektoru
//   binding 5: outImage    (rgba8)   — Nihai renk / TAA girdi goruntusu
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba8)   uniform readonly image2D g_Albedo;
layout(binding = 1, rgba16f) uniform readonly image2D g_Normal;
layout(binding = 2, rgba8)   uniform readonly image2D g_Material;
layout(binding = 3, r32f)    uniform readonly image2D g_Depth;
layout(binding = 4, rg16f)   uniform readonly image2D g_Motion;
layout(binding = 5, rgba16f) uniform writeonly image2D outImage;

layout(push_constant) uniform DebugPush {
    vec4 screenRes; // x: width, y: height, z: debugMode, w: unused
};

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    int debugMode = int(screenRes.z);

    vec4 albedoData = imageLoad(g_Albedo, pixel);
    vec4 normalData = imageLoad(g_Normal, pixel);
    vec4 matData    = imageLoad(g_Material, pixel);
    float depth     = imageLoad(g_Depth, pixel).r;
    vec2 motion     = imageLoad(g_Motion, pixel).rg;

    vec3 finalColor = vec3(0.0);

    switch (debugMode) {
        case 1: // Albedo
            finalColor = albedoData.rgb;
            break;

        case 2: // World Normal (Ham [-1, 1] normalini [0, 1] renk uzayina esle)
            if (albedoData.a > 0.0) {
                finalColor = normalData.xyz * 0.5 + 0.5;
            } else {
                finalColor = albedoData.rgb; // Gokyuzu
            }
            break;

        case 3: // Dogrusal Derinlik
            if (depth > 0.0) {
                float dNorm = clamp(depth / 25.0, 0.0, 1.0);
                finalColor = vec3(1.0 - dNorm);
            } else {
                finalColor = vec3(0.0);
            }
            break;

        case 4: // 2D Motion Vectors (Hareketsiz = 0.5 gri, hareket = renkli sapma)
            finalColor = vec3(clamp(motion * 50.0 + 0.5, 0.0, 1.0), 0.5);
            break;

        case 5: // Roughness & Metallic & Material ID
            finalColor = vec3(matData.r, matData.g, matData.b);
            break;

        case 0: // Varsayilan Onizleme: Basit Lambertian Difuz + Ambiyans
        default:
            if (albedoData.a > 0.0) {
                vec3 N = normalize(normalData.xyz);
                vec3 L = normalize(vec3(0.5, 1.0, 0.8));
                float diff = max(dot(N, L), 0.0);
                float amb = 0.25;
                finalColor = albedoData.rgb * (amb + (1.0 - amb) * diff);
            } else {
                finalColor = albedoData.rgb; // Gokyuzu
            }
            break;
    }

    imageStore(outImage, pixel, vec4(finalColor, 1.0));
}
