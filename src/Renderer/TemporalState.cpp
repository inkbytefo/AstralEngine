#include "Astral/Renderer/TemporalState.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace Astral {

TemporalState::TemporalState() {
    Reset(TemporalResetReason::Explicit);
    m_CommittedPingPong = 0;
    m_HasPreparedCandidate = false;
}

TemporalPlan TemporalState::Prepare(const RenderFrameSettings& frame) {
    if (m_HasPreparedCandidate) {
        throw std::logic_error("TemporalState::Prepare: Candidate already prepared without commit or abort!");
    }

    // Reset tetikleyici kontroller
    if (!frame.camera.has_value()) {
        Reset(TemporalResetReason::MissingCamera);
    } else if (m_CommittedWidth > 0 && m_CommittedHeight > 0 &&
               frame.width > 0 && frame.height > 0 &&
               (frame.width != m_CommittedWidth || frame.height != m_CommittedHeight)) {
        Reset(TemporalResetReason::Resize);
    } else if (m_HasCommittedHistory && frame.debugMode != m_CommittedDebugMode) {
        Reset(TemporalResetReason::DebugChanged);
    } else if (m_HasCommittedHistory && frame.taaEnabled != m_CommittedTaaEnabled) {
        Reset(TemporalResetReason::TaaChanged);
    } else if (m_HasCommittedHistory && m_CommittedCamera.has_value()) {
        const auto& cam = *frame.camera;
        const auto& prevCam = *m_CommittedCamera;
        if (cam.sceneInstance != prevCam.sceneInstance) {
            Reset(TemporalResetReason::SceneChanged);
        } else if (cam.entity != prevCam.entity || cam.projection != prevCam.projection) {
            Reset(TemporalResetReason::CameraCut);
        }
    }

    TemporalPlan plan{};
    plan.readIndex = m_CommittedPingPong;
    plan.writeIndex = 1 - m_CommittedPingPong;
    plan.useHistory = m_HasCommittedHistory && !m_ResetPending && frame.camera.has_value();
    plan.resetReason = plan.useHistory ? TemporalResetReason::None : m_LastResetReason;

    m_CandidateFrame = frame;
    m_CandidatePlan = plan;

    if (frame.camera.has_value()) {
        const auto& cam = *frame.camera;
        glm::mat4 vkProj = cam.projection;
        // Vulkan NDC: Y ekseni asagi dogrudur
        if (vkProj[1][1] > 0.0f) {
            vkProj[1][1] *= -1.0f;
        }
        glm::mat4 currVP = vkProj * cam.view;
        glm::vec3 currPos = cam.position;
        glm::vec2 currJitter = frame.jitter;

        glm::mat4 prevVP;
        glm::vec3 prevPos;
        glm::vec2 prevJitter;

        if (plan.useHistory && m_HasCommittedHistory) {
            prevVP = m_CommittedViewProj;
            prevPos = m_CommittedCameraPos;
            prevJitter = m_CommittedJitter;
        } else {
            prevVP = currVP;
            prevPos = currPos;
            prevJitter = currJitter;
        }

        m_CandidateCameraUBO.currViewProj = currVP;
        m_CandidateCameraUBO.prevViewProj = prevVP;
        m_CandidateCameraUBO.prevCameraPosition = glm::vec4(prevPos, 0.0f);
        m_CandidateCameraUBO.jitter = glm::vec4(currJitter, prevJitter);
        m_CandidateCamPos = currPos;

        if (!plan.useHistory) {
            m_TemporalHistory.SetCameraCut(true);
        }
        m_TemporalHistory.BeginFrame(cam.view, cam.projection, cam.position, currJitter, cam.sceneInstance);
    } else {
        m_CandidateCameraUBO = CameraUBOData{};
        m_CandidateCamPos = glm::vec3(0.0f);
    }

    m_HasPreparedCandidate = true;
    return plan;
}

void TemporalState::CommitSubmitted() {
    if (!m_HasPreparedCandidate) {
        throw std::logic_error("TemporalState::CommitSubmitted: No candidate prepared to commit!");
    }

    m_CommittedPingPong = m_CandidatePlan.writeIndex;
    if (m_CandidateFrame.camera.has_value()) {
        m_CommittedViewProj = m_CandidateCameraUBO.currViewProj;
        m_CommittedCameraPos = m_CandidateCamPos;
        m_CommittedJitter = glm::vec2(m_CandidateCameraUBO.jitter.x, m_CandidateCameraUBO.jitter.y);
        m_CommittedCamera = m_CandidateFrame.camera;
        m_HasCommittedHistory = true;
    } else {
        m_HasCommittedHistory = false;
        m_CommittedCamera = std::nullopt;
    }

    m_CommittedDebugMode = m_CandidateFrame.debugMode;
    m_CommittedTaaEnabled = m_CandidateFrame.taaEnabled;
    m_CommittedWidth = m_CandidateFrame.width;
    m_CommittedHeight = m_CandidateFrame.height;
    m_ResetPending = false;

    m_TemporalHistory.CommitRender();
    m_HasPreparedCandidate = false;
}

void TemporalState::AbortPrepared() noexcept {
    m_HasPreparedCandidate = false;
}

void TemporalState::Reset(TemporalResetReason reason) noexcept {
    m_LastResetReason = reason;
    m_HasCommittedHistory = false;
    m_ResetPending = true;
    m_TemporalHistory.Reset();
    if (reason == TemporalResetReason::CameraCut) {
        m_TemporalHistory.SetCameraCut(true);
    }
    if (m_HasPreparedCandidate) {
        m_CandidatePlan.useHistory = false;
        m_CandidatePlan.resetReason = reason;
    }
}

} // namespace Astral
