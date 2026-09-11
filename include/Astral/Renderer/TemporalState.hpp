#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <glm/glm.hpp>
#include <optional>
#include <cstdint>
#include <stdexcept>
#include "Astral/Renderer/RenderCamera.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include "Astral/Renderer/SDFTemporalHistory.hpp"

namespace Astral {

enum class TemporalResetReason {
    None = 0,
    Explicit,
    Resize,
    SceneChanged,
    CameraCut,
    MissingCamera,
    TaaChanged,
    DebugChanged,
    EnvironmentChanged,
    SubmissionFailed
};

struct TemporalPlan {
    uint32_t readIndex = 0;
    uint32_t writeIndex = 1;
    bool useHistory = false;
    TemporalResetReason resetReason = TemporalResetReason::None;
};

class TemporalState {
public:
    TemporalState();
    ~TemporalState() = default;

    TemporalState(const TemporalState&) = delete;
    TemporalState& operator=(const TemporalState&) = delete;
    TemporalState(TemporalState&&) noexcept = default;
    TemporalState& operator=(TemporalState&&) noexcept = default;

    /// Yeni kare icin temporal plan hazirlar ve aday durumu olusturur.
    /// Onceki aday commit/abort edilmeden cagrilirsa std::logic_error firlatir.
    TemporalPlan Prepare(const RenderFrameSettings& frame);

    /// Basarili gonderim sonrasinda adayi committed duruma gecirir ve ping-pong slotunu ilerletir.
    /// Hazirlanmis aday yoksa std::logic_error firlatir.
    void CommitSubmitted();

    /// Gonderim iptal edilirse adayi temizler, indeks ilerletilmez.
    void AbortPrepared() noexcept;

    /// Tarihceyi sifirlar ve sebebi kaydeder.
    void Reset(TemporalResetReason reason) noexcept;

    [[nodiscard]] TemporalResetReason GetLastResetReason() const noexcept { return m_LastResetReason; }
    [[nodiscard]] bool HasValidHistory() const noexcept { return m_HasCommittedHistory && !m_ResetPending; }
    [[nodiscard]] bool HasPreparedCandidate() const noexcept { return m_HasPreparedCandidate; }

    [[nodiscard]] CameraUBOData GetCameraUBOData() const noexcept { return m_CandidateCameraUBO; }
    [[nodiscard]] const SDFTemporalHistory& GetHistoryEvaluator() const noexcept { return m_TemporalHistory; }
    [[nodiscard]] SDFTemporalHistory& GetHistoryEvaluator() noexcept { return m_TemporalHistory; }

    [[nodiscard]] uint32_t GetCommittedPingPong() const noexcept { return m_CommittedPingPong; }
    [[nodiscard]] glm::mat4 GetCommittedViewProj() const noexcept { return m_CommittedViewProj; }
    [[nodiscard]] glm::vec3 GetCommittedCameraPos() const noexcept { return m_CommittedCameraPos; }
    [[nodiscard]] glm::vec2 GetCommittedJitter() const noexcept { return m_CommittedJitter; }
    [[nodiscard]] const std::optional<RenderCamera>& GetCommittedCamera() const noexcept { return m_CommittedCamera; }

    [[nodiscard]] SDFTemporalHistory::TemporalConfidenceResult EvaluateConfidence(
        const SDFTemporalHistory::ReprojectionQuery& query,
        uint32_t prevSurfaceId,
        float prevDepth,
        const glm::vec3& prevNormal,
        float prevLightingSignal,
        float depthTolerance = 0.05f) const {
        return m_TemporalHistory.EvaluateConfidence(query, prevSurfaceId, prevDepth, prevNormal, prevLightingSignal, depthTolerance);
    }

private:
    bool m_HasPreparedCandidate = false;
    bool m_HasCommittedHistory = false;
    bool m_ResetPending = true;
    TemporalResetReason m_LastResetReason = TemporalResetReason::Explicit;

    // Committed State
    uint32_t m_CommittedPingPong = 0;
    glm::mat4 m_CommittedViewProj{1.0f};
    glm::vec3 m_CommittedCameraPos{0.0f};
    glm::vec2 m_CommittedJitter{0.0f};
    std::optional<RenderCamera> m_CommittedCamera;
    int m_CommittedDebugMode = 0;
    bool m_CommittedTaaEnabled = false;
    uint32_t m_CommittedWidth = 0;
    uint32_t m_CommittedHeight = 0;

    // Candidate State
    TemporalPlan m_CandidatePlan{};
    CameraUBOData m_CandidateCameraUBO{};
    glm::vec3 m_CandidateCamPos{0.0f};
    RenderFrameSettings m_CandidateFrame{};

    SDFTemporalHistory m_TemporalHistory;
};

} // namespace Astral
