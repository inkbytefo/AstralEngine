#pragma once

#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Astral::Geometry {

/// Kalite profili parametreleri (A4 Spesifikasyonu: shadowMaxSteps=96, shadowMaxDist=50m, aoSamples=8, aoRadius=1m)
struct SDFQualityProfile {
    uint32_t shadowMaxSteps = 96;
    float shadowMaxDistance = 50.0f;
    uint32_t aoSamples = 8;
    float aoRadius = 1.0f;
    float shadowK = 24.0f;
    float surfaceBias = 0.015f; // Dunya biriminde normal offset
};

/// Isik kaynagi tanimi (CPU visibility ve test fixture'lari icin)
struct LightSource {
    enum class Type : uint32_t {
        Directional = 0,
        Point = 1
    };

    Type type = Type::Directional;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
};

/// G-Buffer'dan cikarilan yuzey parametreleri
struct DeferredSurface {
    glm::vec3 worldPos{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 albedo{1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
};

/// Analitik SDF koni takipli yumusak golge (Cone-traced Soft Shadow)
/// ro: Dunya koordinatlarinda isin baslangici (yuzey + normal * bias)
/// rd: Isik yonune dogru birim vektor
/// mint: Baslangic adim mesafesi
/// maxt: Maksimum golge mesafesi (Yonlu icin maxDistance, Noktasal icin min(isiga_mesafe, maxDistance))
/// k: Penumbra keskinlik faktoru (orn. 24.0)
/// maxSteps: Maksimum isin adim sayisi (varsayilan 96)
inline float EvaluateSDFSoftShadow(
    const SDFSceneSnapshot& snapshot,
    const glm::vec3& ro,
    const glm::vec3& rd,
    float mint,
    float maxt,
    float k,
    uint32_t maxSteps = 96
) {
    if (snapshot.GetRecordCount() == 0) {
        return 1.0f;
    }

    // Gecersiz/NaN/Sifir yon kontrolu (deterministik fail-safe)
    if (!std::isfinite(ro.x) || !std::isfinite(ro.y) || !std::isfinite(ro.z) ||
        !std::isfinite(rd.x) || !std::isfinite(rd.y) || !std::isfinite(rd.z)) {
        return 1.0f;
    }

    float rdLenSq = glm::dot(rd, rd);
    if (rdLenSq < 1e-6f) {
        return 1.0f;
    }

    float res = 1.0f;
    float t = std::max(mint, 0.001f);

    for (uint32_t i = 0; i < maxSteps && t < maxt; ++i) {
        glm::vec3 p = ro + rd * t;
        float h = snapshot.EvaluateDistance(p);

        if (!std::isfinite(h)) {
            return 1.0f; // Tekil durumda guvenli fail-safe
        }

        // Blocker ile tam temas
        if (h < 0.001f) {
            return 0.0f;
        }

        // Penumbra yumusaklik hesabi
        res = std::min(res, k * h / t);

        // Guvenli konservatif adim
        t += std::clamp(h, 0.015f, 0.5f);
    }

    return std::clamp(res, 0.0f, 1.0f);
}

/// Analitik SDF coklu-ornek Ortam Karartmasi (Ambient Occlusion)
/// pos: Dunya koordinatlarinda yuzey konumu
/// normal: Normalize edilmis yuzey normali
/// samples: Ornek sayisi (varsayilan 8)
/// radius: Maksimum etki yaricapi (varsayilan 1.0m)
/// surfaceBias: Yuzey bias mesafesi (varsayilan 0.015m)
inline float EvaluateSDFAO(
    const SDFSceneSnapshot& snapshot,
    const glm::vec3& pos,
    const glm::vec3& normal,
    uint32_t samples = 8,
    float radius = 1.0f,
    float surfaceBias = 0.015f
) {
    if (snapshot.GetRecordCount() == 0 || samples == 0) {
        return 1.0f;
    }

    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) ||
        !std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z)) {
        return 1.0f;
    }

    float occ = 0.0f;
    float weight = 1.0f;
    float totalWeight = 0.0f;

    for (uint32_t i = 0; i < samples; ++i) {
        float h = radius * (static_cast<float>(i + 1) / static_cast<float>(samples));
        glm::vec3 samplePos = pos + normal * (surfaceBias + h);

        float d = snapshot.EvaluateDistance(samplePos);
        if (!std::isfinite(d)) continue;

        // Eger d < h ise bu hacimde geometri vardir ve oklüzyon olusur
        occ += std::max(0.0f, h - d) * weight;
        totalWeight += h * weight;
        weight *= 0.75f;
    }

    float ao = 1.0f - (occ / std::max(totalWeight, 1e-4f));
    return std::clamp(ao, 0.0f, 1.0f);
}

/// Deferred Lighting Kombinasyon Sozlesmesi
/// direct += evaluateDirectLight(light, surface) * shadowVisibility;
/// ambientDiffuse *= ambientOcclusion;
/// No finalColor *= ambientOcclusion shortcut.
inline glm::vec3 EvaluateDeferredLighting(
    const DeferredSurface& surface,
    const LightSource* lights,
    uint32_t lightCount,
    const SDFSceneSnapshot& snapshot,
    const SDFQualityProfile& profile,
    const glm::vec3& ambientDiffuseIBL,
    const glm::vec3& ambientSpecularIBL,
    float iblIntensity = 1.0f,
    float exposure = 1.0f
) {
    // 1. Ortam Karartmasi (AO) yalinizca difuz IBL'e uygulanir
    float ao = EvaluateSDFAO(
        snapshot, surface.worldPos, surface.normal,
        profile.aoSamples, profile.aoRadius, profile.surfaceBias
    );

    // 2. Dogrudan isiklar ve SDF golge oklüzyonu
    glm::vec3 directLo(0.0f);

    for (uint32_t i = 0; i < lightCount; ++i) {
        const auto& light = lights[i];
        glm::vec3 L;
        glm::vec3 radiance;
        float distToLight = 0.0f;

        if (light.type == LightSource::Type::Directional) {
            L = -glm::normalize(light.direction);
            radiance = light.color * light.intensity;
            distToLight = profile.shadowMaxDistance;
        } else {
            glm::vec3 lightVec = light.position - surface.worldPos;
            distToLight = glm::length(lightVec);
            if (distToLight < 1e-5f) {
                L = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                L = lightVec / distToLight;
            }

            float range = std::max(light.range, 0.001f);
            float atten = 1.0f / (1.0f + (distToLight * distToLight) / (range * range));
            radiance = light.color * light.intensity * atten;
        }

        float NdotL = std::max(glm::dot(surface.normal, L), 0.0f);
        if (NdotL > 0.0f) {
            float shadowVisibility = 1.0f;

            // Isigin arkasindaki nesne golge uretmez (maxt = min(distToLight, shadowMaxDistance))
            if (distToLight > profile.surfaceBias) {
                glm::vec3 ro = surface.worldPos + surface.normal * profile.surfaceBias;
                float maxt = (light.type == LightSource::Type::Directional)
                    ? profile.shadowMaxDistance
                    : std::min(distToLight, profile.shadowMaxDistance);

                shadowVisibility = EvaluateSDFSoftShadow(
                    snapshot, ro, L, profile.surfaceBias, maxt, profile.shadowK, profile.shadowMaxSteps
                );
            }

            directLo += radiance * NdotL * shadowVisibility;
        }
    }

    // 3. Sozlesme:
    // ambientDiffuse *= ambientOcclusion;
    // Emissive/direct light ve specular asla AO ile korlemesine carpilmaz.
    glm::vec3 ambient = (ambientDiffuseIBL * ao + ambientSpecularIBL) * iblIntensity;

    return (directLo + ambient) * exposure;
}

} // namespace Astral::Geometry
