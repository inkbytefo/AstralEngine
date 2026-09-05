#version 460
//
// DeferredLighting.glsl — Cook-Torrance GGX & Split-Sum IBL Compute Shader (Faz 2)
//
// Gorev: G-Buffer ciktilarini (Albedo, Normal, Material, Depth) okuyup Cook-Torrance
// mikro-yuzey BRDF ve Split-Sum IBL (Irradiance + Prefiltered + BRDF LUT) ile fiziksel
// tabanli aydinlatmayi hesaplar.
//
// KRITIK MIMARI KURAL:
// Bu shader tonemapping veya gamma duzeltmesi UYGULAMAZ. Cikti saf Dogrusal HDR (rgba16f)
// formatinda outColor hedefine yazilir. ACES Tonemapping ve sRGB donusumu downstream
// TAAResolve pass'inde veya nihai composite asamasinda gerceklestirilir.
//
// Baglantilar:
//   binding 0: g_Albedo        (rgba8,   readonly image2D)
//   binding 1: g_Normal        (rgba16f, readonly image2D)
//   binding 2: g_Material      (rgba8,   readonly image2D)
//   binding 3: g_Depth         (r32f,    readonly image2D)
//   binding 4: outColor        (rgba16f, writeonly image2D) — Linear HDR
//   binding 5: u_IrradianceMap (samplerCube)
//   binding 6: u_PrefilteredMap(samplerCube)
//   binding 7: u_BRDFLut       (sampler2D)
//   binding 8: LightBuffer     (SSBO std430)
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// 1. G-Buffer Girdileri
layout(binding = 0, rgba8)   uniform readonly image2D g_Albedo;
layout(binding = 1, rgba16f) uniform readonly image2D g_Normal;
layout(binding = 2, rgba8)   uniform readonly image2D g_Material;
layout(binding = 3, r32f)    uniform readonly image2D g_Depth;

// 2. Dogrusal HDR Ciktisi
layout(binding = 4, rgba16f) uniform writeonly image2D outColor;

// 3. IBL Ortam Dokulari
layout(binding = 5) uniform samplerCube u_IrradianceMap;
layout(binding = 6) uniform samplerCube u_PrefilteredMap;
layout(binding = 7) uniform sampler2D   u_BRDFLut;

// 4. Isik Tamponu (LightBuffer SSBO)
struct Light {
    vec4 position;  // xyz: pos, w: type (0 = Directional, 1 = Point)
    vec4 direction; // xyz: dir, w: intensity
    vec4 color;     // rgb: color, w: radius / range
};

layout(std430, binding = 8) readonly buffer LightBuffer {
    uint lightCount;
    uint pad0;
    uint pad1;
    uint pad2;
    Light lights[];
};

// 5. Push Sabitleri
layout(push_constant) uniform PushConstants {
    vec4 camPos;    // xyz: camPos, w: maxMipLevel (orn. 5.0)
    vec4 camDir;    // xyz: camDir, w: exposure (orn. 1.0)
    vec4 screenRes; // xy: resolution, z: iblIntensity (orn. 1.0), w: unused
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 rayParams;
};

const float PI = 3.14159265358979323846;

// =================== PBR Mikro-Yuzey Fonksiyonlari ===================

// Normal Dagilim Fonksiyonu: Trowbridge-Reitz GGX
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

// Geometrik Golgeleme: Smith Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Fonksiyonu: Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// =================== Main Compute ===================

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    // G-Buffer verilerini oku
    vec4 albedoData = imageLoad(g_Albedo, pixel);
    vec4 normalData = imageLoad(g_Normal, pixel);
    vec4 matData    = imageLoad(g_Material, pixel);
    float depth     = imageLoad(g_Depth, pixel).r;

    // Isin Yonu (Camera Ray) Rekonstruksiyonu
    vec2 uv = (vec2(pixel) + rayParams.xy - 0.5 * vec2(res)) / float(res.y);
    uv.y = -uv.y;

    vec3 ro = camPos.xyz;
    vec3 fwd = normalize(camDir.xyz);
    vec3 right = cameraRight.xyz;
    vec3 up = cameraUp.xyz;
    vec3 rd = normalize(fwd * cameraRight.w + right * uv.x + up * uv.y);

    // 1. Gokyuzu / Bosluk Kontrolu
    if (albedoData.a < 0.5) {
        // Gokyuzunu prefiltered cubemap'ten ornekle
        vec3 skyColor = textureLod(u_PrefilteredMap, rd, 0.0).rgb * camDir.w;
        imageStore(outColor, pixel, vec4(skyColor, 1.0));
        return;
    }

    // 2. Dunya Pozisyonu & Yuzey Parametreleri
    vec3 worldPos = ro + rd * depth;
    vec3 N = normalize(normalData.xyz);
    vec3 V = -rd;
    float NdotV = max(dot(N, V), 0.001);

    vec3 albedo = albedoData.rgb;
    float roughness = clamp(matData.r, 0.04, 1.0);
    float metallic  = clamp(matData.g, 0.0, 1.0);

    // Temel Yansitma Degeri (F0)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 3. Analitik Isiklar (Cook-Torrance BRDF)
    vec3 Lo = vec3(0.0);

    for (uint i = 0u; i < lightCount; ++i) {
        Light light = lights[i];

        vec3 L;
        vec3 radiance;

        if (light.position.w < 0.5) {
            // Yonlu Isik (Directional / Sun)
            L = -normalize(light.direction.xyz);
            radiance = light.color.rgb * light.direction.w;
        } else {
            // Noktasal Isik (Point Light)
            vec3 lightVec = light.position.xyz - worldPos;
            float dist = length(lightVec);
            L = normalize(lightVec);

            float range = max(light.color.w, 0.001);
            float atten = 1.0 / (1.0 + (dist * dist) / (range * range));
            radiance = light.color.rgb * atten * light.direction.w;
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0) {
            float NDF = DistributionGGX(N, H, roughness);
            float G   = GeometrySmith(N, V, L, roughness);
            vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular     = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    // 4. Split-Sum Ortam Isigi (Image-Based Lighting)
    vec3 R = reflect(-V, N);

    // Difuz IBL
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;

    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    // Spekuler IBL
    float maxMipLevel = max(camPos.w, 1.0);
    vec3 prefilteredColor = textureLod(u_PrefilteredMap, R, roughness * (maxMipLevel - 1.0)).rgb;
    vec2 brdf = texture(u_BRDFLut, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_IBL * brdf.x + brdf.y);

    float iblIntensity = max(screenRes.z, 0.0);
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * iblIntensity;

    // 5. Toplam Dogrusal HDR Rengi
    vec3 finalLinearHDR = (Lo + ambient) * camDir.w; // camDir.w = exposure

    // Saf Dogrusal HDR ciktisini yaz (Tonemapping TAA sonrasina birakilir)
    imageStore(outColor, pixel, vec4(finalLinearHDR, 1.0));
}
