#include "TestFramework.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Astral::Test {

void RunSDFChangeSetTests() {
    const std::string suite = "SDF Local ChangeSet & Invalidation Suite";
    std::cout << "\n--- [A4-P5] SDF Local ChangeSet & Invalidation Suite ---\n";

    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    // Invert Y for Vulkan NDC
    proj[1][1] *= -1.0f;

    // =========================================================================
    // 1. Static Scene Identity: Identical Snapshots Yield Zero Changes
    // =========================================================================
    {
        Registry reg;
        auto e = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(e, TransformComponent{ glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent sdf{};
        sdf.shape.dimensions = glm::vec4(1.0f);
        sdf.primitiveType = 0; // Sphere
        sdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(e, sdf);

        auto snap1 = SDFSceneSnapshot::Extract(reg, 1);
        auto snap2 = SDFSceneSnapshot::Extract(reg, 1);

        auto changeSet = SDFChangeSet::Compare(snap1, snap2, proj * view, proj * view);

        TEST_CHECK_MSG(suite, "StaticSceneNoChanges", !changeSet.HasChanges(),
                       "Two identical snapshots must report no changes!");
        TEST_CHECK_MSG(suite, "StaticSceneConfidenceOne", changeSet.GetConfidence(glm::vec2(0.5f, 0.5f)) == 1.0f,
                       "Confidence on static scene must be exactly 1.0!");
    }

    // =========================================================================
    // 2. Door Carve Acceptance Fixture: Carved Region Rejected, Distant Preserved >= 99%
    // =========================================================================
    {
        Registry reg;

        // Static Wall at Origin: Box of width 10m (half-extents 5), height 6m (half-extents 3), thickness 1m
        auto wall = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(wall, TransformComponent{ glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent wallSdf{};
        wallSdf.shape.dimensions = glm::vec4(5.0f, 3.0f, 0.5f, 0.0f); // Box
        wallSdf.primitiveType = 1;
        wallSdf.operation = 0; // Union
        wallSdf.csgOrder = 0;
        wallSdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(wall, wallSdf);

        // Distant control object (Sphere at x = -15m, y = 5m, far away from origin)
        auto controlObj = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(controlObj, TransformComponent{ glm::vec3(-15.0f, 5.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent ctrlSdf{};
        ctrlSdf.shape.dimensions = glm::vec4(1.0f);
        ctrlSdf.primitiveType = 0; // Sphere
        ctrlSdf.operation = 0;
        ctrlSdf.csgOrder = 1;
        ctrlSdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(controlObj, ctrlSdf);

        auto snapBefore = SDFSceneSnapshot::Extract(reg, 1);

        // Perform door carve: Add cutter box with subtractive operation at wall center
        auto cutter = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(cutter, TransformComponent{ glm::vec3(0.0f, -0.5f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent cutterSdf{};
        cutterSdf.shape.dimensions = glm::vec4(0.8f, 1.8f, 1.0f, 0.0f); // Door opening
        cutterSdf.primitiveType = 1; // Box
        cutterSdf.operation = 2;     // OpSubtract
        cutterSdf.csgOrder = 2;
        cutterSdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(cutter, cutterSdf);

        auto snapAfter = SDFSceneSnapshot::Extract(reg, 1);

        auto changeSet = SDFChangeSet::Compare(snapBefore, snapAfter, proj * view, proj * view);

        TEST_CHECK_MSG(suite, "DoorCarveDetected", changeSet.HasChanges(),
                       "Door carve must be detected as a change!");
        TEST_CHECK_MSG(suite, "DoorCarveNotGlobal", !changeSet.IsGlobalChange(),
                       "Localized door carve must not trigger global screen wipe!");

        // Center of screen (UV ~ (0.5, 0.5)) is directly where the door is carved
        bool centerInvalidated = changeSet.IsPixelInvalidated(glm::vec2(0.5f, 0.5f));
        TEST_CHECK_MSG(suite, "DoorCarveCenterInvalidated", centerInvalidated,
                       "The carved door area on screen must be invalidated!");
        TEST_CHECK_MSG(suite, "DoorCarveCenterZeroConfidence", changeSet.GetConfidence(glm::vec2(0.5f, 0.5f)) == 0.0f,
                       "Confidence at the carved region must be 0.0!");

        // Far right of wall (UV (0.9, 0.5)) or top corner (UV (0.1, 0.1)):
        // Test 1000 sample points in distant control region (x in [0.0, 0.25], y in [0.0, 0.3])
        int totalControlSamples = 0;
        int preservedControlSamples = 0;
        for (int iy = 0; iy < 20; ++iy) {
            for (int ix = 0; ix < 50; ++ix) {
                float u = 0.01f + 0.20f * (float(ix) / 49.0f);
                float v = 0.01f + 0.25f * (float(iy) / 19.0f);
                totalControlSamples++;
                if (!changeSet.IsPixelInvalidated(glm::vec2(u, v))) {
                    preservedControlSamples++;
                }
            }
        }

        float preservationRatio = static_cast<float>(preservedControlSamples) / static_cast<float>(totalControlSamples);
        TEST_CHECK_MSG(suite, "DistantControlPreservation99Percent", preservationRatio >= 0.99f,
                       "Distant control region must preserve >= 99% of temporal history! (Actual: " + std::to_string(preservationRatio * 100.0f) + "%)");
    }

    // =========================================================================
    // 3. Transform Motion: Moving Object Covers Both Old & New Screen Footprints
    // =========================================================================
    {
        Registry reg;
        auto obj = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(obj, TransformComponent{ glm::vec3(-2.0f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent sdf{};
        sdf.shape.dimensions = glm::vec4(0.8f);
        sdf.primitiveType = 0; // Sphere
        sdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(obj, sdf);

        auto snap1 = SDFSceneSnapshot::Extract(reg, 1);

        // Move sphere to +2m on X
        reg.GetComponent<TransformComponent>(obj).position = glm::vec3(2.0f, 0.0f, 0.0f);
        auto snap2 = SDFSceneSnapshot::Extract(reg, 1);

        auto changeSet = SDFChangeSet::Compare(snap1, snap2, proj * view, proj * view);

        TEST_CHECK_MSG(suite, "TransformMotionDetected", changeSet.HasChanges(),
                       "Object translation must be detected!");
        TEST_CHECK_MSG(suite, "TransformMotionChangeCount", changeSet.GetChanges().size() == 1,
                       "Exactly 1 primitive change must be reported!");
        TEST_CHECK_MSG(suite, "TransformMotionTypeCorrect",
                       HasFlag(changeSet.GetChanges()[0].changeType, SDFChangeType::TransformMotion),
                       "Change type must have TransformMotion flag!");

        // Screen projection of (-2, 0, 0) and (+2, 0, 0) should both be invalidated
        glm::vec4 clipOld = (proj * view) * glm::vec4(-2.0f, 0.0f, 0.0f, 1.0f);
        glm::vec2 uvOld = (glm::vec2(clipOld.x, clipOld.y) / clipOld.w) * 0.5f + 0.5f;

        glm::vec4 clipNew = (proj * view) * glm::vec4(2.0f, 0.0f, 0.0f, 1.0f);
        glm::vec2 uvNew = (glm::vec2(clipNew.x, clipNew.y) / clipNew.w) * 0.5f + 0.5f;

        TEST_CHECK_MSG(suite, "OldPositionInvalidated", changeSet.IsPixelInvalidated(uvOld),
                       "Old position footprint must be invalidated (disocclusion)!");
        TEST_CHECK_MSG(suite, "NewPositionInvalidated", changeSet.IsPixelInvalidated(uvNew),
                       "New position footprint must be invalidated (occlusion)!");
    }

    // =========================================================================
    // 4. Shape Dimension Change vs Material Change
    // =========================================================================
    {
        Registry reg;
        auto obj = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(obj, TransformComponent{ glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent sdf{};
        sdf.shape.dimensions = glm::vec4(1.0f);
        sdf.primitiveType = 0; // Sphere
        sdf.albedo = glm::vec3(0.8f, 0.2f, 0.2f);
        sdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(obj, sdf);

        auto snapBase = SDFSceneSnapshot::Extract(reg, 1);

        // Material change only
        reg.GetComponent<SDFComponent>(obj).albedo = glm::vec3(0.1f, 0.9f, 0.1f);
        auto snapMat = SDFSceneSnapshot::Extract(reg, 1);

        auto csMat = SDFChangeSet::Compare(snapBase, snapMat, proj * view, proj * view);
        TEST_CHECK_MSG(suite, "MaterialChangeDetected", csMat.HasChanges(),
                       "Material change must be detected!");
        TEST_CHECK_MSG(suite, "MaterialChangeTypeOnly",
                       csMat.GetChanges()[0].changeType == SDFChangeType::MaterialChange,
                       "Change type must be strictly MaterialChange!");

        // Shape parameter change
        reg.GetComponent<SDFComponent>(obj).shape.dimensions.x = 2.5f;
        auto snapShape = SDFSceneSnapshot::Extract(reg, 1);

        auto csShape = SDFChangeSet::Compare(snapMat, snapShape, proj * view, proj * view);
        TEST_CHECK_MSG(suite, "ShapeChangeDetected",
                       HasFlag(csShape.GetChanges()[0].changeType, SDFChangeType::ShapeOrCSGChange),
                       "Shape change must be classified as ShapeOrCSGChange!");
    }

    // =========================================================================
    // 5. Infinite Plane Change Triggers Global Invalidation
    // =========================================================================
    {
        Registry reg;
        auto p = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(p, TransformComponent{ glm::vec3(0.0f, -1.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
        SDFComponent sdf{};
        sdf.primitiveType = 3; // Infinite plane
        sdf.encoding = SDFShapeEncoding::ExplicitShape;
        reg.AddComponent<SDFComponent>(p, sdf);

        auto snap1 = SDFSceneSnapshot::Extract(reg, 1);

        // Move plane
        reg.GetComponent<TransformComponent>(p).position = glm::vec3(0.0f, -2.0f, 0.0f);
        auto snap2 = SDFSceneSnapshot::Extract(reg, 1);

        auto changeSet = SDFChangeSet::Compare(snap1, snap2, proj * view, proj * view);
        TEST_CHECK_MSG(suite, "PlaneChangeIsGlobal", changeSet.IsGlobalChange(),
                       "Modifying an infinite plane must trigger GlobalChange (conservative safety fallback)!");
        TEST_CHECK_MSG(suite, "PlaneChangeInvalidatesAllPixels", changeSet.IsPixelInvalidated(glm::vec2(0.1f, 0.9f)),
                       "Global change must invalidate all pixels!");
    }

    // =========================================================================
    // 6. Push Constant Rect Packing
    // =========================================================================
    {
        Registry reg;
        // Create 6 small spheres at different locations
        for (int i = 0; i < 6; ++i) {
            auto e = reg.CreateEntity();
            reg.AddComponent<TransformComponent>(e, TransformComponent{ glm::vec3(-3.0f + i * 1.2f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
            SDFComponent s{};
            s.shape.dimensions = glm::vec4(0.3f);
            s.primitiveType = 0;
            s.encoding = SDFShapeEncoding::ExplicitShape;
            reg.AddComponent<SDFComponent>(e, s);
        }
        auto snap1 = SDFSceneSnapshot::Extract(reg, 1);

        // Add 6 new spheres
        for (int i = 0; i < 6; ++i) {
            auto e = reg.CreateEntity();
            reg.AddComponent<TransformComponent>(e, TransformComponent{ glm::vec3(-3.0f + i * 1.2f, 2.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f) });
            SDFComponent s{};
            s.shape.dimensions = glm::vec4(0.3f);
            s.primitiveType = 0;
            s.encoding = SDFShapeEncoding::ExplicitShape;
            reg.AddComponent<SDFComponent>(e, s);
        }
        auto snap2 = SDFSceneSnapshot::Extract(reg, 1);

        auto changeSet = SDFChangeSet::Compare(snap1, snap2, proj * view, proj * view);
        std::array<glm::vec4, 4> packed{};
        uint32_t count = 0;
        changeSet.GetPackedRects(packed, count);

        TEST_CHECK_MSG(suite, "PackedRectsCountWithinLimit", count <= 4,
                       "Packed rect count must be at most 4!");
        bool allFinite = true;
        for (uint32_t i = 0; i < count; ++i) {
            if (!std::isfinite(packed[i].x) || !std::isfinite(packed[i].y) ||
                !std::isfinite(packed[i].z) || !std::isfinite(packed[i].w)) {
                allFinite = false;
            }
        }
        TEST_CHECK_MSG(suite, "PackedRectsAllFinite", allFinite,
                       "Packed rect coordinates must all be finite!");
    }
}

} // namespace Astral::Test
