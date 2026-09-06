#include "TestFramework.hpp"
#include "Astral/Renderer/SDFTemporalHistory.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Astral::Test {

void RunSDFTemporalTests() {
    const std::string suite = "SDF Temporal Confidence Suite";
    std::cout << "\n--- [A4-P4] SDF Temporal Confidence Suite ---\n";

    // =========================================================================
    // 1. Same Depth, Different Object (Hard Rejection via Surface ID)
    // =========================================================================
    {
        SDFTemporalHistory history;
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

        // Frame 1: Object A rendered
        history.BeginFrame(view, proj, glm::vec3(0, 0, 5), glm::vec2(0.0f), 1001);
        history.CommitRender();

        // Frame 2: Object B occupies the same pixel at the exact same depth!
        history.BeginFrame(view, proj, glm::vec3(0, 0, 5), glm::vec2(0.0f), 1001);

        uint32_t surfaceIdObjectA = 0xAAAA1111;
        uint32_t surfaceIdObjectB = 0xBBBB2222;
        float depth = 3.5f;

        SDFTemporalHistory::ReprojectionQuery query{};
        query.currentWorldPos = glm::vec3(0.0f, 0.0f, 1.5f);
        query.currentNormal = glm::vec3(0.0f, 0.0f, 1.0f);
        query.currentSurfaceId = surfaceIdObjectB; // Current object is B
        query.currentDepth = depth;
        query.currentUV = glm::vec2(0.5f, 0.5f);
        query.motionVector = glm::vec2(0.0f);

        // Evaluate against history from Frame 1 which recorded Object A
        auto conf = history.EvaluateConfidence(query, surfaceIdObjectA, depth, glm::vec3(0, 0, 1), 1.0f);

        TEST_CHECK_MSG(suite, "SameDepthDifferentObjectHardRejected", conf.hardRejected,
                       "Different surface identity at same depth must be HARD REJECTED!");
        TEST_CHECK_MSG(suite, "SameDepthIdentityInvalid", !conf.identityValid,
                       "Identity validity flag must be false!");
        TEST_CHECK_MSG(suite, "SameDepthZeroHistoryWeight", conf.historyWeight == 0.0f,
                       "History weight must be strictly 0.0 to prevent ghosting!");
    }

    // =========================================================================
    // 2. Entity Slot Reuse with New Generation (Generational Surface ID)
    // =========================================================================
    {
        SDFTemporalHistory history;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);

        history.BeginFrame(view, proj, glm::vec3(0), glm::vec2(0), 1002);
        history.CommitRender();
        history.BeginFrame(view, proj, glm::vec3(0), glm::vec2(0), 1002);

        // Old entity at slot 5, gen 1
        SDFSurfaceKey keyGen1{.entityIndex = 5, .entityGeneration = 1, .sceneInstanceId = 1002, .subPrimitiveIndex = 0};
        // New entity reused slot 5, gen 2
        SDFSurfaceKey keyGen2{.entityIndex = 5, .entityGeneration = 2, .sceneInstanceId = 1002, .subPrimitiveIndex = 0};

        TEST_CHECK_MSG(suite, "GenerationalKeysDifferent", keyGen1.Hash() != keyGen2.Hash(),
                       "Reused entity slot with different generation must have distinct hash!");

        SDFTemporalHistory::ReprojectionQuery query{};
        query.currentSurfaceId = keyGen2.Hash();
        query.currentDepth = 2.0f;
        query.currentUV = glm::vec2(0.5f);
        query.motionVector = glm::vec2(0.0f);

        auto conf = history.EvaluateConfidence(query, keyGen1.Hash(), 2.0f, glm::vec3(0, 1, 0), 1.0f);
        TEST_CHECK_MSG(suite, "SlotReuseHardRejected", conf.hardRejected && conf.historyWeight == 0.0f,
                       "Reused entity slot with different generation must be hard rejected!");
    }

    // =========================================================================
    // 3. Decoupled Extraction: Multiple Extractions in One Frame
    // =========================================================================
    {
        SDFTemporalHistory history;
        glm::mat4 view1 = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -1));
        glm::mat4 proj = glm::mat4(1.0f);

        // Render Frame 1
        history.BeginFrame(view1, proj, glm::vec3(0, 0, 1), glm::vec2(0), 1003);
        history.CommitRender();

        // Frame 2: BeginFrame with view2
        glm::mat4 view2 = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -2));
        history.BeginFrame(view2, proj, glm::vec3(0, 0, 2), glm::vec2(0), 1003);

        // Multiple extractions happen in this frame (e.g. physics query, debug inspect)
        // Previous state should STILL be Frame 1 (camPos = (0, 0, 1))!
        TEST_CHECK_MSG(suite, "DecoupledExtractionPreservesPrevious",
                       history.GetPreviousState().cameraPosition == glm::vec3(0, 0, 1),
                       "Multiple extractions before CommitRender must NOT overwrite previous history!");

        // Only after CommitRender does it advance to (0, 0, 2)
        history.CommitRender();
        TEST_CHECK_MSG(suite, "CommitRenderAdvancesHistory",
                       history.GetPreviousState().cameraPosition == glm::vec3(0, 0, 2),
                       "CommitRender must advance previous history!");
    }

    // =========================================================================
    // 4. Unrendered Simulation Updates Do Not Advance History
    // =========================================================================
    {
        SDFTemporalHistory history;
        glm::mat4 view1 = glm::translate(glm::mat4(1.0f), glm::vec3(1, 0, 0));
        history.BeginFrame(view1, glm::mat4(1.0f), glm::vec3(1, 0, 0), glm::vec2(0), 1004);
        history.CommitRender();

        // Simulate 3 ticks without rendering
        for (int i = 0; i < 3; ++i) {
            glm::mat4 viewTick = glm::translate(glm::mat4(1.0f), glm::vec3(2 + i, 0, 0));
            history.BeginFrame(viewTick, glm::mat4(1.0f), glm::vec3(2 + i, 0, 0), glm::vec2(0), 1004);
            // No CommitRender()!
        }

        // Previous committed render must still be tick 1
        TEST_CHECK_MSG(suite, "UnrenderedUpdatePreservesCommittedHistory",
                       history.GetPreviousState().cameraPosition == glm::vec3(1, 0, 0),
                       "Unrendered frames must not advance history!");
    }

    // =========================================================================
    // 5. Camera Cut Hard Rejection
    // =========================================================================
    {
        SDFTemporalHistory history;
        history.BeginFrame(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0), glm::vec2(0), 1005);
        history.CommitRender();

        // Camera cut triggered
        history.SetCameraCut(true);
        history.BeginFrame(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(100, 0, 0), glm::vec2(0), 1005);

        SDFTemporalHistory::ReprojectionQuery query{};
        query.currentSurfaceId = 42;
        query.currentDepth = 5.0f;
        query.currentUV = glm::vec2(0.5f);

        auto conf = history.EvaluateConfidence(query, 42, 5.0f, glm::vec3(0, 1, 0), 1.0f);
        TEST_CHECK_MSG(suite, "CameraCutHardRejection", conf.hardRejected && conf.historyWeight == 0.0f,
                       "Camera cut must hard reject temporal history with zero weight!");
    }

    // =========================================================================
    // 6. Scene Instance Switch (Save/Load / Scene Change)
    // =========================================================================
    {
        SDFTemporalHistory history;
        history.BeginFrame(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0), glm::vec2(0), 1006);
        history.CommitRender();

        // New scene instance loaded (id 1007)
        history.SetSceneInstanceId(1007);
        TEST_CHECK_MSG(suite, "SceneSwitchInvalidatesHistory", !history.HasValidHistory(),
                       "Changing scene instance must immediately invalidate temporal history!");
    }

    // =========================================================================
    // 7. Soft Attenuation: Normal Mismatch & CSG Blend & Lighting Delta
    // =========================================================================
    {
        SDFTemporalHistory history;
        history.BeginFrame(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0), glm::vec2(0), 1008);
        history.CommitRender();
        history.BeginFrame(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0), glm::vec2(0), 1008);

        uint32_t surfId = 999;
        float depth = 4.0f;

        // a) Perfect match: full 0.88 weight
        SDFTemporalHistory::ReprojectionQuery queryMatch{};
        queryMatch.currentSurfaceId = surfId;
        queryMatch.currentDepth = depth;
        queryMatch.currentNormal = glm::vec3(0, 1, 0);
        queryMatch.currentAttributionConfidence = 1.0f;
        queryMatch.currentLightingSignal = 1.0f;
        queryMatch.currentUV = glm::vec2(0.5f);

        auto confMatch = history.EvaluateConfidence(queryMatch, surfId, depth, glm::vec3(0, 1, 0), 1.0f);
        TEST_CHECK_MSG(suite, "PerfectMatchMaxWeight", std::abs(confMatch.historyWeight - 0.88f) < 1e-4f,
                       "Perfect surface, normal, and lighting match must give 0.88 history weight!");

        // b) Normal deviation (dot = 0.5)
        glm::vec3 devNormal = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
        auto confNormal = history.EvaluateConfidence(queryMatch, surfId, depth, devNormal, 1.0f);
        TEST_CHECK_MSG(suite, "NormalDeviationReducesWeight", confNormal.historyWeight < confMatch.historyWeight,
                       "Normal deviation must reduce history weight!");

        // c) Smooth CSG blend uncertainty (attribution confidence = 0.4)
        SDFTemporalHistory::ReprojectionQuery queryBlend = queryMatch;
        queryBlend.currentAttributionConfidence = 0.4f;
        auto confBlend = history.EvaluateConfidence(queryBlend, surfId, depth, glm::vec3(0, 1, 0), 1.0f);
        TEST_CHECK_MSG(suite, "CsgBlendReducesWeight", confBlend.historyWeight < confMatch.historyWeight,
                       "CSG blend boundary must reduce history weight proportionally!");

        // d) Lighting change (signal changed from 1.0 to 0.2)
        auto confLighting = history.EvaluateConfidence(queryMatch, surfId, depth, glm::vec3(0, 1, 0), 0.2f);
        TEST_CHECK_MSG(suite, "LightingDeltaReducesWeight", confLighting.historyWeight < confMatch.historyWeight,
                       "Sudden lighting change must reduce history weight to prevent shadow trailing!");
    }

    std::cout << "--- [A4-P4] SDF Temporal Confidence Suite Basariyla Tamamlandi! ---\n";
}

} // namespace Astral::Test
