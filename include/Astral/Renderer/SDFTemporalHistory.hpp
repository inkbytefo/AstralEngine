#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Astral {

struct TemporalFrameState {
    glm::mat4 viewProj{1.0f};
    glm::mat4 invViewProj{1.0f};
    glm::vec3 cameraPosition{0.0f};
    glm::vec2 jitter{0.0f};
    uint64_t sceneInstanceId = 0;
    uint32_t geometryRevision = 0;
    bool cameraCut = false;
    bool valid = false;
};

class SDFTemporalHistory {
public:
    SDFTemporalHistory() = default;
    ~SDFTemporalHistory() = default;

    /// Kamera kesimi, sahne degisimi veya pencere boyutu degisiminde tarihceyi tamamen sifirlar
    void Reset() {
        m_PrevState = TemporalFrameState{};
        m_CurrState = TemporalFrameState{};
        m_CameraCut = true;
    }

    void SetCameraCut(bool cut) { m_CameraCut = cut; }
    bool IsCameraCut() const { return m_CameraCut; }

    void IncrementGeometryRevision() { m_GeometryRevision++; }
    uint32_t GetGeometryRevision() const { return m_GeometryRevision; }

    void SetSceneInstanceId(uint64_t id) {
        if (m_SceneInstanceId != id) {
            m_SceneInstanceId = id;
            Reset();
        }
    }
    uint64_t GetSceneInstanceId() const { return m_SceneInstanceId; }

    bool HasValidHistory() const {
        return m_PrevState.valid && !m_CameraCut;
    }

    const TemporalFrameState& GetCurrentState() const { return m_CurrState; }
    const TemporalFrameState& GetPreviousState() const { return m_PrevState; }

    /// Yeni kare parametrelerini hazirlar (gecmis tarihceyi ILERLETMES; extraction'dan bagimsizdir)
    void BeginFrame(
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& camPos,
        const glm::vec2& jitter,
        uint64_t sceneInstanceId
    ) {
        if (m_SceneInstanceId != sceneInstanceId) {
            SetSceneInstanceId(sceneInstanceId);
        }

        m_CurrState.viewProj = proj * view;
        m_CurrState.invViewProj = glm::inverse(m_CurrState.viewProj);
        m_CurrState.cameraPosition = camPos;
        m_CurrState.jitter = jitter;
        m_CurrState.sceneInstanceId = m_SceneInstanceId;
        m_CurrState.geometryRevision = m_GeometryRevision;
        m_CurrState.cameraCut = m_CameraCut;
        m_CurrState.valid = true;
    }

    /// Basarili GPU render sonrasi gecmis tarihceyi resmi olarak bir kare ilerletir
    void CommitRender() {
        if (!m_CurrState.valid) return;
        m_PrevState = m_CurrState;
        m_CameraCut = false;
    }

    // =========================================================================
    // Reprojection & Temporal Confidence Hesaplayici (CPU & Test Sozlesmesi)
    // =========================================================================

    struct ReprojectionQuery {
        glm::vec3 currentWorldPos{0.0f};
        glm::vec3 currentNormal{0.0f, 1.0f, 0.0f};
        uint32_t currentSurfaceId = 0;
        float currentDepth = 0.0f;
        float currentAttributionConfidence = 1.0f; // Smooth CSG guveni
        float currentLightingSignal = 1.0f;        // AO ve golge sinyali
        glm::vec2 currentUV{0.5f};
        glm::vec2 motionVector{0.0f};
    };

    struct TemporalConfidenceResult {
        bool identityValid = true;
        bool depthValid = true;
        bool geometryValid = true;
        float normalConfidence = 1.0f;
        float attributionConfidence = 1.0f;
        float lightingConfidence = 1.0f;
        float totalConfidence = 1.0f;
        float historyWeight = 0.88f;
        bool hardRejected = false;
    };

    TemporalConfidenceResult EvaluateConfidence(
        const ReprojectionQuery& query,
        uint32_t prevSurfaceId,
        float prevDepth,
        const glm::vec3& prevNormal,
        float prevLightingSignal,
        float depthTolerance = 0.05f
    ) const {
        TemporalConfidenceResult result{};

        // 1. Gecmis tarihce gecerliligi ve kamera kesim kontrolu
        if (!HasValidHistory() || m_CameraCut) {
            result.geometryValid = false;
            result.hardRejected = true;
            result.totalConfidence = 0.0f;
            result.historyWeight = 0.0f;
            return result;
        }

        // 2. Ekran sinirlari kontrolu (Reprojected UV [0, 1] araliginda olmali)
        glm::vec2 prevUV = query.currentUV - query.motionVector;
        if (!std::isfinite(prevUV.x) || !std::isfinite(prevUV.y) ||
            prevUV.x < 0.0f || prevUV.x > 1.0f || prevUV.y < 0.0f || prevUV.y > 1.0f) {
            result.geometryValid = false;
            result.hardRejected = true;
            result.totalConfidence = 0.0f;
            result.historyWeight = 0.0f;
            return result;
        }

        // 3. HARD REJECTION: Kimlik (Surface ID / Generational Slot) Uyumsuzlugu
        // Ayni derinlikte olsa dahi farkli nesneler asla birbirinin tarihcesini kullanamaz!
        if (query.currentSurfaceId != prevSurfaceId) {
            result.identityValid = false;
            result.hardRejected = true;
            result.totalConfidence = 0.0f;
            result.historyWeight = 0.0f;
            return result;
        }

        // 4. HARD REJECTION: Derinlik Uyumsuzlugu (Depth Discontinuity)
        float maxAllowedDepthDelta = std::max(depthTolerance, 0.03f * query.currentDepth);
        if (std::abs(query.currentDepth - prevDepth) > maxAllowedDepthDelta) {
            result.depthValid = false;
            result.hardRejected = true;
            result.totalConfidence = 0.0f;
            result.historyWeight = 0.0f;
            return result;
        }

        // 5. YUMUSAK GUVEN AZALTIMI (Soft Confidence Attenuation):
        // a) Normal Uyumu
        float normalDot = glm::dot(glm::normalize(query.currentNormal), glm::normalize(prevNormal));
        result.normalConfidence = std::clamp(normalDot, 0.0f, 1.0f);

        // b) Smooth CSG / Yuzey Sahipligi Belirsizlik Guveni
        result.attributionConfidence = std::clamp(query.currentAttributionConfidence, 0.0f, 1.0f);

        // c) Aydinlatma & Golge/AO Degisim Guveni
        float lightingDelta = std::abs(query.currentLightingSignal - prevLightingSignal);
        result.lightingConfidence = std::clamp(1.0f - lightingDelta, 0.0f, 1.0f);

        // 6. Toplam Guven ve Tarihce Agirligi (Tavan: 0.88)
        result.totalConfidence = result.normalConfidence * result.attributionConfidence * result.lightingConfidence;
        result.historyWeight = 0.88f * result.totalConfidence;

        return result;
    }

private:
    TemporalFrameState m_CurrState{};
    TemporalFrameState m_PrevState{};
    uint64_t m_SceneInstanceId = 0;
    uint32_t m_GeometryRevision = 0;
    bool m_CameraCut = false;
};

} // namespace Astral
