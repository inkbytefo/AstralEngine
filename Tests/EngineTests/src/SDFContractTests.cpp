#include "TestFramework.hpp"
#include "Astral/Geometry/SDFKernel.inl"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Scene/Scene.hpp"
#include <random>
#include <cmath>
#include <iostream>

namespace Astral::Test {

// Test 1: Analytical Primitives exact values
static void RunAnalyticalPrimitivesTests(const std::string& suite) {
    using namespace Astral::Geometry;

    // 1. Sphere at origin, radius = 2.0
    {
        glm::vec3 p(0.0f, 0.0f, 3.5f);
        float d = sdSphere(p, 2.0f);
        TEST_CHECK(suite, "SphereOutside", std::abs(d - 1.5f) < 1e-5f);

        glm::vec3 pIn(0.0f, 0.5f, 0.0f);
        float dIn = sdSphere(pIn, 2.0f);
        TEST_CHECK(suite, "SphereInside", std::abs(dIn - (-1.5f)) < 1e-5f);
    }

    // 2. Box at origin, half extents = (1.0, 2.0, 3.0)
    {
        glm::vec3 b(1.0f, 2.0f, 3.0f);
        // Point on surface
        float dSurf = sdBox(glm::vec3(1.0f, 0.0f, 0.0f), b);
        TEST_CHECK(suite, "BoxSurface", std::abs(dSurf) < 1e-5f);

        // Point outside along +Y
        float dOut = sdBox(glm::vec3(0.0f, 4.0f, 0.0f), b);
        TEST_CHECK(suite, "BoxOutside", std::abs(dOut - 2.0f) < 1e-5f);

        // Point inside
        float dIn = sdBox(glm::vec3(0.0f, 0.0f, 0.0f), b);
        TEST_CHECK(suite, "BoxInside", std::abs(dIn - (-1.0f)) < 1e-5f);
    }

    // 3. Torus, major R = 3.0, minor r = 0.5
    {
        glm::vec2 t(3.0f, 0.5f);
        // Point at center of tube on +X
        float dTube = sdTorus(glm::vec3(3.0f, 0.0f, 0.0f), t);
        TEST_CHECK(suite, "TorusTubeCenter", std::abs(dTube - (-0.5f)) < 1e-5f);

        // Point on tube surface
        float dSurf = sdTorus(glm::vec3(3.5f, 0.0f, 0.0f), t);
        TEST_CHECK(suite, "TorusSurface", std::abs(dSurf) < 1e-5f);
    }

    // 4. Plane Y=0, offset = 1.0 (surface at Y = -1.0)
    {
        float d1 = sdPlane(glm::vec3(0.0f, 2.0f, 0.0f), 1.0f);
        TEST_CHECK(suite, "PlaneAbove", std::abs(d1 - 3.0f) < 1e-5f);
    }

    // 5. Capsule radius = 0.5, height = 2.0 along +Y
    {
        float d0 = sdCapsule(glm::vec3(0.0f, 1.0f, 0.0f), 0.5f, 2.0f);
        TEST_CHECK(suite, "CapsuleCore", std::abs(d0 - (-0.5f)) < 1e-5f);

        float dSurf = sdCapsule(glm::vec3(0.5f, 1.0f, 0.0f), 0.5f, 2.0f);
        TEST_CHECK(suite, "CapsuleSurface", std::abs(dSurf) < 1e-5f);
    }

    // 6. Cylinder radius = 1.0, half height = 2.0
    {
        float dCenter = sdCylinder(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 2.0f);
        TEST_CHECK(suite, "CylinderCenter", std::abs(dCenter - (-1.0f)) < 1e-5f);

        float dSide = sdCylinder(glm::vec3(1.5f, 0.0f, 0.0f), 1.0f, 2.0f);
        TEST_CHECK(suite, "CylinderOutsideSide", std::abs(dSide - 0.5f) < 1e-5f);

        float dTop = sdCylinder(glm::vec3(0.0f, 3.0f, 0.0f), 1.0f, 2.0f);
        TEST_CHECK(suite, "CylinderOutsideTop", std::abs(dTop - 1.0f) < 1e-5f);
    }
}

// Test 2: CSG operations and smooth blending attribution
static void RunCsgOperationsTests(const std::string& suite) {
    using namespace Astral::Geometry;

    // Union
    {
        SDFHitResult hit{};
        combinePrimitive(hit, true, 2.0f, glm::vec3(1, 0, 0), 0.5f, 0.0f, 10u, 0u, 0.0f);
        combinePrimitive(hit, false, 1.0f, glm::vec3(0, 1, 0), 0.2f, 1.0f, 20u, 0u, 0.0f);
        TEST_CHECK(suite, "UnionMinDistance", std::abs(hit.distance - 1.0f) < 1e-5f);
        TEST_CHECK(suite, "UnionAttribution", hit.surfaceId == 20u);
        TEST_CHECK(suite, "UnionConfidence", hit.confidence == 1.0f);
    }

    // Subtract
    {
        SDFHitResult hit{};
        combinePrimitive(hit, true, 2.0f, glm::vec3(1, 0, 0), 0.5f, 0.0f, 10u, 0u, 0.0f);
        // Subtract object with d = -0.5 (cutting inside)
        combinePrimitive(hit, false, -0.5f, glm::vec3(0, 0, 1), 0.1f, 0.0f, 30u, 1u, 0.0f);
        TEST_CHECK(suite, "SubtractDistance", std::abs(hit.distance - 2.0f) < 1e-5f);

        // Subtract with cutting tool having d = -3.0 -> -d = 3.0 > 2.0
        combinePrimitive(hit, false, -3.0f, glm::vec3(0, 0, 1), 0.1f, 0.0f, 30u, 1u, 0.0f);
        TEST_CHECK(suite, "SubtractCarveDistance", std::abs(hit.distance - 3.0f) < 1e-5f);
        TEST_CHECK(suite, "SubtractCarveSurface", hit.surfaceId == 30u);
    }

    // Smooth Union & Confidence attenuation
    {
        SDFHitResult hit{};
        combinePrimitive(hit, true, 1.0f, glm::vec3(1, 0, 0), 0.5f, 0.0f, 10u, 0u, 0.0f);
        // At d2 = 1.0, blend is symmetric (h = 0.5)
        combinePrimitive(hit, false, 1.0f, glm::vec3(0, 1, 0), 0.5f, 0.0f, 20u, 3u, 0.5f);
        // Distance should be smooth-min: 1.0 - k * 0.25 = 1.0 - 0.125 = 0.875
        TEST_CHECK(suite, "SmoothUnionDistance", std::abs(hit.distance - 0.875f) < 1e-4f);
        // Confidence drops in the blend zone!
        TEST_CHECK_MSG(suite, "SmoothUnionConfidenceDrops", hit.confidence < 0.1f,
                       "Confidence must drop in smooth CSG transition region!");
    }
}

// Test 3: Lipschitz continuity verification across 10,000 random query pairs
static void RunLipschitzContinuity10kTests(const std::string& suite) {
    auto scene = std::make_shared<Scene>("LipschitzScene");

    // Entity 1: Sphere at (0, 0, 0)
    Entity e1 = scene->CreateEntity("Sphere");
    e1.AddComponent<TransformComponent>(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.5f));
    auto& s1 = e1.AddComponent<SDFComponent>();
    s1.primitiveType = 0;
    s1.operation = 0;
    s1.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    s1.encoding = SDFShapeEncoding::ExplicitShape;
    s1.csgOrder = 1;

    // Entity 2: Box at (1.5, 0.5, 0.0)
    Entity e2 = scene->CreateEntity("Box");
    e2.AddComponent<TransformComponent>(glm::vec3(1.5f, 0.5f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    auto& s2 = e2.AddComponent<SDFComponent>();
    s2.primitiveType = 1;
    s2.operation = 3; // Smooth Union
    s2.blendFactor = 0.3f;
    s2.shape.dimensions = glm::vec4(0.8f, 0.6f, 0.7f, 0.0f);
    s2.encoding = SDFShapeEncoding::ExplicitShape;
    s2.csgOrder = 2;

    const auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());
    TEST_CHECK(suite, "SnapshotRecordCount", snapshot.GetRecordCount() == 2);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> deltaDist(-0.1f, 0.1f);

    size_t violations = 0;
    float maxRatio = 0.0f;

    for (int i = 0; i < 10000; ++i) {
        glm::vec3 p1(dist(rng), dist(rng), dist(rng));
        glm::vec3 p2 = p1 + glm::vec3(deltaDist(rng), deltaDist(rng), deltaDist(rng));

        float step = glm::length(p1 - p2);
        if (step < 1e-6f) continue;

        float d1 = snapshot.EvaluateDistance(p1);
        float d2 = snapshot.EvaluateDistance(p2);

        float absDiff = std::abs(d1 - d2);
        float ratio = absDiff / step;
        if (ratio > maxRatio) maxRatio = ratio;

        // Lipschitz constant must be <= 1.0 (with 1.001 allowance for floating point rounding)
        if (ratio > 1.001f) {
            violations++;
        }
    }

    TEST_CHECK_MSG(suite, "Lipschitz10kViolations", violations == 0,
                   "Zero Lipschitz violations allowed (|d1 - d2| <= ||p1 - p2||) across 10,000 queries!");
    std::cout << "  [INFO] 10,000 Point Lipschitz Test: max |d1-d2|/||p1-p2|| = " << maxRatio
              << ", violations = " << violations << "\n";
}

// Test 4: Singular matrix exclusion and deterministic ordering
static void RunSnapshotRobustnessTests(const std::string& suite) {
    auto scene = std::make_shared<Scene>("RobustnessScene");

    // 1. Normal sphere
    Entity e1 = scene->CreateEntity("Sphere1");
    e1.AddComponent<TransformComponent>(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    auto& s1 = e1.AddComponent<SDFComponent>();
    s1.csgOrder = 10;

    // 2. Degenerate entity with zero scale (singular matrix)
    Entity e2 = scene->CreateEntity("Degenerate");
    e2.AddComponent<TransformComponent>(glm::vec3(2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.0f));
    auto& s2 = e2.AddComponent<SDFComponent>();
    s2.csgOrder = 5;

    // 3. Normal box with lower csgOrder (must sort first)
    Entity e3 = scene->CreateEntity("BoxFirst");
    e3.AddComponent<TransformComponent>(glm::vec3(5.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    auto& s3 = e3.AddComponent<SDFComponent>();
    s3.csgOrder = 2;

    const auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

    // Degenerate entity must be excluded identically
    TEST_CHECK_MSG(suite, "SingularMatrixExcluded", snapshot.GetRecordCount() == 2,
                   "Zero-scale singular transform must be cleanly excluded!");

    // Records must be strictly sorted by csgOrder
    if (snapshot.GetRecordCount() == 2) {
        TEST_CHECK(suite, "CsgOrderSorted0", snapshot.GetRecords()[0].csgOrder == 2);
        TEST_CHECK(suite, "CsgOrderSorted1", snapshot.GetRecords()[1].csgOrder == 10);
    }
}

// Test 5: Hierarchy transform inheritance and persistent identity
static void RunSnapshotHierarchyAndIdentityTests(const std::string& suite) {
    auto scene = std::make_shared<Scene>("HierarchyScene");

    // 1. Parent entity at (10, 0, 0)
    Entity parent = scene->CreateEntity("Parent");
    parent.AddComponent<TransformComponent>(glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

    // 2. Child entity at local (0, 5, 0) with parent
    Entity child = scene->CreateEntity("ChildSphere");
    child.AddComponent<TransformComponent>(glm::vec3(0.0f, 5.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    auto& hier = child.AddComponent<HierarchyComponent>();
    hier.parent = parent.GetHandle();

    auto& sdf = child.AddComponent<SDFComponent>();
    sdf.primitiveType = 0; // Sphere
    sdf.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // radius = 1.0
    sdf.encoding = SDFShapeEncoding::ExplicitShape;
    sdf.csgOrder = 1;

    const auto snap1 = SDFSceneSnapshot::Extract(scene->GetRegistry());
    TEST_CHECK_MSG(suite, "HierarchyRecordExtracted", snap1.GetRecordCount() == 1,
                   "Child entity must be extracted!");

    if (snap1.GetRecordCount() == 1) {
        // Center of sphere should be at world (10, 5, 0)
        float dCenter = snap1.EvaluateDistance(glm::vec3(10.0f, 5.0f, 0.0f));
        TEST_CHECK_MSG(suite, "HierarchyWorldCenterDistance", std::abs(dCenter - (-1.0f)) < 1e-4f,
                       "Child sphere center must be at world (10, 5, 0), distance must be -1.0!");

        float dSurface = snap1.EvaluateDistance(glm::vec3(10.0f, 6.0f, 0.0f));
        TEST_CHECK_MSG(suite, "HierarchyWorldSurfaceDistance", std::abs(dSurface) < 1e-4f,
                       "Child sphere surface at (10, 6, 0) must have distance 0.0!");

        // Entities mapping must match
        TEST_CHECK(suite, "HierarchyEntitiesMatched", snap1.GetEntities().size() == 1 && snap1.GetEntities()[0] == child.GetHandle());

        uint32_t originalSurfaceId = snap1.GetRecords()[0].surfaceId;

        // 3. Add an unrelated entity with lower csgOrder (will sort before child)
        Entity other = scene->CreateEntity("OtherBox");
        other.AddComponent<TransformComponent>(glm::vec3(-5.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& otherSdf = other.AddComponent<SDFComponent>();
        otherSdf.csgOrder = 0; // Will be first record

        const auto snap2 = SDFSceneSnapshot::Extract(scene->GetRegistry());
        TEST_CHECK(suite, "TwoRecordsExtracted", snap2.GetRecordCount() == 2);

        // Find child record in snap2
        bool foundChild = false;
        for (size_t i = 0; i < snap2.GetRecordCount(); ++i) {
            if (snap2.GetEntities()[i] == child.GetHandle()) {
                foundChild = true;
                TEST_CHECK_MSG(suite, "PersistentSurfaceIdMaintained",
                               snap2.GetRecords()[i].surfaceId == originalSurfaceId,
                               "Child surfaceId must remain identical even when other entities are added or reordered!");
            }
        }
        // 4. Test LegacyPackedScale hierarchy inheritance
        Entity legacyChild = scene->CreateEntity("LegacyChildSphere");
        legacyChild.AddComponent<TransformComponent>(glm::vec3(0.0f, -3.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(2.0f));
        auto& legacyHier = legacyChild.AddComponent<HierarchyComponent>();
        legacyHier.parent = parent.GetHandle();

        auto& legacySdf = legacyChild.AddComponent<SDFComponent>();
        legacySdf.primitiveType = 0; // Sphere
        legacySdf.encoding = SDFShapeEncoding::LegacyPackedScale;
        legacySdf.csgOrder = 3;

        const auto snap3 = SDFSceneSnapshot::Extract(scene->GetRegistry());
        // Parent is at (10, 0, 0), legacyChild is at local (0, -3, 0) -> world (10, -3, 0)
        float dLegacyCenter = snap3.EvaluateDistance(glm::vec3(10.0f, -3.0f, 0.0f));
        TEST_CHECK_MSG(suite, "LegacyHierarchyWorldCenter", std::abs(dLegacyCenter - (-2.0f)) < 1e-4f,
                       "LegacyPackedScale child sphere center must be at world (10, -3, 0), distance must be -2.0!");
    }
}

void RunSDFContractTests() {
    const std::string suite = "SDF Contract Suite";
    RunAnalyticalPrimitivesTests(suite);
    RunCsgOperationsTests(suite);
    RunLipschitzContinuity10kTests(suite);
    RunSnapshotRobustnessTests(suite);
    RunSnapshotHierarchyAndIdentityTests(suite);
}

} // namespace Astral::Test
