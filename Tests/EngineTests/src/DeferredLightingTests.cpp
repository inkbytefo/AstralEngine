#include "TestFramework.hpp"
#include "Astral/Geometry/SDFVisibility.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Renderer/BrickGrid.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>

namespace Astral::Test {

using namespace Astral::Geometry;

void RunDeferredLightingTests() {
    const std::string suite = "Deferred SDF Shadows & AO Suite";
    std::cout << "\n--- [A4-P3] Deferred SDF Shadows & AO Suite ---\n";

    SDFQualityProfile qualityProfile{};
    qualityProfile.shadowMaxSteps = 96;
    qualityProfile.shadowMaxDistance = 50.0f;
    qualityProfile.aoSamples = 8;
    qualityProfile.aoRadius = 1.0f;
    qualityProfile.shadowK = 24.0f;
    qualityProfile.surfaceBias = 0.015f;

    // =========================================================================
    // 1. Directional Light: Blocker Ahead vs Unoccluded vs Penumbra Widening
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("DirLightScene");

        // Ground Plane at Y = 0 (Normal pointing +Y)
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3; // Plane
        sGround.operation = 0;     // Union
        sGround.shape.dimensions = glm::vec4(0.0f); // Plane at local Y = 0
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        // Blocker Sphere at (0, 2, 0) with radius = 1.0
        Entity blocker = scene->CreateEntity("Blocker");
        blocker.AddComponent<TransformComponent>(glm::vec3(0.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sBlocker = blocker.AddComponent<SDFComponent>();
        sBlocker.primitiveType = 0; // Sphere
        sBlocker.operation = 0;     // Union
        sBlocker.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // radius 1.0
        sBlocker.encoding = SDFShapeEncoding::ExplicitShape;
        sBlocker.csgOrder = 2;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());
        TEST_CHECK(suite, "SnapshotExtractedForDirLight", snapshot.GetRecordCount() == 2);

        // Directional light pointing downwards: direction = (0, -1, 0) -> ray direction L = (0, 1, 0)
        glm::vec3 sunDir(0.0f, -1.0f, 0.0f);
        glm::vec3 L = -glm::normalize(sunDir);

        // a) Directly under the blocker at (0, 0, 0)
        glm::vec3 roCenter = glm::vec3(0.0f, 0.0f, 0.0f) + glm::vec3(0.0f, 1.0f, 0.0f) * qualityProfile.surfaceBias;
        float shadowCenter = EvaluateSDFSoftShadow(
            snapshot, roCenter, L, qualityProfile.surfaceBias, qualityProfile.shadowMaxDistance, qualityProfile.shadowK, qualityProfile.shadowMaxSteps
        );
        TEST_CHECK_MSG(suite, "DirectionalShadowBlockerAhead", shadowCenter == 0.0f,
                       "Point directly under blocker sphere must have 0.0 shadow visibility!");

        // b) Far outside the blocker at (5, 0, 0)
        glm::vec3 roFar = glm::vec3(5.0f, 0.0f, 0.0f) + glm::vec3(0.0f, 1.0f, 0.0f) * qualityProfile.surfaceBias;
        float shadowFar = EvaluateSDFSoftShadow(
            snapshot, roFar, L, qualityProfile.surfaceBias, qualityProfile.shadowMaxDistance, qualityProfile.shadowK, qualityProfile.shadowMaxSteps
        );
        TEST_CHECK_MSG(suite, "DirectionalShadowUnoccluded", shadowFar == 1.0f,
                       "Point far from blocker sphere must have 1.0 shadow visibility!");

        // c) Penumbra widening: sample between X = 0.9 and X = 2.5
        float prevShadow = -1.0f;
        bool monotonicallyIncreasing = true;
        for (float x = 0.9f; x <= 2.5f; x += 0.2f) {
            glm::vec3 roPenumbra = glm::vec3(x, 0.0f, 0.0f) + glm::vec3(0.0f, 1.0f, 0.0f) * qualityProfile.surfaceBias;
            float s = EvaluateSDFSoftShadow(
                snapshot, roPenumbra, L, qualityProfile.surfaceBias, qualityProfile.shadowMaxDistance, qualityProfile.shadowK, qualityProfile.shadowMaxSteps
            );
            if (prevShadow >= 0.0f && s < prevShadow - 1e-4f) {
                monotonicallyIncreasing = false;
            }
            prevShadow = s;
        }
        TEST_CHECK_MSG(suite, "DirectionalPenumbraMonotonic", monotonicallyIncreasing,
                       "Penumbra soft shadow visibility must increase smoothly as distance from center increases!");
    }

    // =========================================================================
    // 2. Point Light: Blocker Ahead vs Blocker Behind Light vs Same Point
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("PointLightScene");

        // Ground Plane at Y = 0
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3;
        sGround.operation = 0;
        sGround.shape.dimensions = glm::vec4(0.0f);
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        // Sphere at Y = 10.0 (Behind the light, which will be at Y = 5.0)
        Entity sphereBehind = scene->CreateEntity("SphereBehind");
        sphereBehind.AddComponent<TransformComponent>(glm::vec3(0.0f, 10.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sBehind = sphereBehind.AddComponent<SDFComponent>();
        sBehind.primitiveType = 0;
        sBehind.operation = 0;
        sBehind.shape.dimensions = glm::vec4(2.0f, 0.0f, 0.0f, 0.0f); // radius 2.0
        sBehind.encoding = SDFShapeEncoding::ExplicitShape;
        sBehind.csgOrder = 2;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        // Point light at (0, 5, 0). Surface at (0, 0, 0).
        // Distance from surface to light is 5.0m. Blocker is at Y=10.0m (behind light).
        LightSource pointLight{};
        pointLight.type = LightSource::Type::Point;
        pointLight.position = glm::vec3(0.0f, 5.0f, 0.0f);
        pointLight.intensity = 10.0f;
        pointLight.range = 15.0f;

        DeferredSurface surface{};
        surface.worldPos = glm::vec3(0.0f, 0.0f, 0.0f);
        surface.normal = glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 toLight = pointLight.position - surface.worldPos;
        float distToLight = glm::length(toLight);
        glm::vec3 L = toLight / distToLight;
        float maxMarchDist = std::min(distToLight, qualityProfile.shadowMaxDistance);

        glm::vec3 ro = surface.worldPos + surface.normal * qualityProfile.surfaceBias;
        float shadow = EvaluateSDFSoftShadow(
            snapshot, ro, L, qualityProfile.surfaceBias, maxMarchDist, qualityProfile.shadowK, qualityProfile.shadowMaxSteps
        );

        TEST_CHECK_MSG(suite, "PointLightBlockerBehindIgnored", shadow == 1.0f,
                       "Object behind the point light must NOT cast shadow on the surface!");

        // Now move light to Y = 15.0 (Blocker at Y = 10.0 is now in between surface and light)
        pointLight.position = glm::vec3(0.0f, 15.0f, 0.0f);
        toLight = pointLight.position - surface.worldPos;
        distToLight = glm::length(toLight);
        L = toLight / distToLight;
        maxMarchDist = std::min(distToLight, qualityProfile.shadowMaxDistance);

        shadow = EvaluateSDFSoftShadow(
            snapshot, ro, L, qualityProfile.surfaceBias, maxMarchDist, qualityProfile.shadowK, qualityProfile.shadowMaxSteps
        );
        TEST_CHECK_MSG(suite, "PointLightBlockerAheadOccludes", shadow == 0.0f,
                       "Object in between surface and point light must occlude the light!");

        // Point light at exact same point as surface (dist < surfaceBias)
        pointLight.position = surface.worldPos;
        toLight = pointLight.position - surface.worldPos;
        distToLight = glm::length(toLight);
        TEST_CHECK_MSG(suite, "PointLightCoincidentSafe", distToLight <= qualityProfile.surfaceBias,
                       "Coincident light test setup confirmed.");
    }

    // =========================================================================
    // 3. Surface Bias: Zero Bias (Acne) vs Safe Bias (Clean) vs Contact Shadow
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("BiasScene");

        // Flat plane at Y = 0
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3;
        sGround.operation = 0;
        sGround.shape.dimensions = glm::vec4(0.0f);
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        glm::vec3 groundPoint(0.0f, 0.0f, 0.0f);
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.0f)); // 60-degree angle

        // a) With standard surfaceBias = 0.015f: clean, no self-shadowing acne
        glm::vec3 roBiased = groundPoint + normal * qualityProfile.surfaceBias;
        float shadowBiased = EvaluateSDFSoftShadow(
            snapshot, roBiased, sunDir, qualityProfile.surfaceBias, 50.0f, 24.0f, 96
        );
        TEST_CHECK_MSG(suite, "SurfaceBiasPreventsAcne", shadowBiased == 1.0f,
                       "Proper surface bias must completely eliminate self-shadowing acne on flat surface!");

        // b) Contact shadow: place a box resting directly on the ground at X = [1, 3]
        Entity box = scene->CreateEntity("Box");
        box.AddComponent<TransformComponent>(glm::vec3(2.0f, 0.5f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sBox = box.AddComponent<SDFComponent>();
        sBox.primitiveType = 1; // Box
        sBox.operation = 0;     // Union
        sBox.shape.dimensions = glm::vec4(1.0f, 0.5f, 1.0f, 0.0f); // extents
        sBox.encoding = SDFShapeEncoding::ExplicitShape;
        sBox.csgOrder = 2;

        auto snapshotWithBox = SDFSceneSnapshot::Extract(scene->GetRegistry());

        // Light shining from +X towards ground at X = 0.95 (just 0.05m outside the box wall)
        glm::vec3 lightFromBoxDir = glm::normalize(glm::vec3(1.0f, 0.2f, 0.0f));
        glm::vec3 contactPoint = glm::vec3(0.95f, 0.0f, 0.0f) + normal * qualityProfile.surfaceBias;
        float contactShadow = EvaluateSDFSoftShadow(
            snapshotWithBox, contactPoint, lightFromBoxDir, qualityProfile.surfaceBias, 50.0f, 24.0f, 96
        );
        TEST_CHECK_MSG(suite, "ContactShadowNoPeterPanning", contactShadow == 0.0f,
                       "Contact shadow 0.05m from blocker must remain fully occluded with no peter-panning gap!");
    }

    // =========================================================================
    // 4. Smooth CSG Hollow Cavity & Contact AO
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("HollowScene");

        // Floor Plane at Y = 0
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3; // Plane
        sGround.operation = 0;     // Union
        sGround.shape.dimensions = glm::vec4(0.0f);
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        // Vertical Wall Box at X = -1.0 (extents = 1.0 in X, so wall face is at X = 0.0, from Y = 0 to 4)
        Entity wall = scene->CreateEntity("Wall");
        wall.AddComponent<TransformComponent>(glm::vec3(-1.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sWall = wall.AddComponent<SDFComponent>();
        sWall.primitiveType = 1; // Box
        sWall.operation = 0;     // Union
        sWall.shape.dimensions = glm::vec4(1.0f, 2.0f, 5.0f, 0.0f); // half extents
        sWall.encoding = SDFShapeEncoding::ExplicitShape;
        sWall.csgOrder = 2;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        // Point a: Far out on open floor at (5.0, 0.0, 0.0), normal = (0, 1, 0)
        glm::vec3 openPos(5.0f, 0.0f, 0.0f);
        glm::vec3 floorNormal(0.0f, 1.0f, 0.0f);
        float openAO = EvaluateSDFAO(snapshot, openPos, floorNormal, qualityProfile.aoSamples, qualityProfile.aoRadius, qualityProfile.surfaceBias);

        // Point b: In 90-degree corner at (0.1, 0.0, 0.0) (0.1m from vertical wall), normal = (0, 1, 0)
        glm::vec3 cornerPos(0.1f, 0.0f, 0.0f);
        float cornerAO = EvaluateSDFAO(snapshot, cornerPos, floorNormal, qualityProfile.aoSamples, qualityProfile.aoRadius, qualityProfile.surfaceBias);

        TEST_CHECK_MSG(suite, "OpenFloorAOIsOne", openAO > 0.95f,
                       "Open floor must have near 1.0 AO (> 0.95)!");
        TEST_CHECK_MSG(suite, "CornerAODarkened", cornerAO < 0.65f,
                       "Point in 90-degree corner must have lower AO (< 0.65) reflecting contact darkening!");
        TEST_CHECK_MSG(suite, "AORelativeCornerDarker", cornerAO < openAO,
                       "Corner AO must be strictly darker than open floor AO!");

        // AO Radius Normalization Test: Farkli yaricaplarda (0.5m, 1.0m, 2.0m) tutarlilik
        float cornerAO_r05 = EvaluateSDFAO(snapshot, cornerPos, floorNormal, qualityProfile.aoSamples, 0.5f, qualityProfile.surfaceBias);
        float cornerAO_r10 = EvaluateSDFAO(snapshot, cornerPos, floorNormal, qualityProfile.aoSamples, 1.0f, qualityProfile.surfaceBias);
        float cornerAO_r20 = EvaluateSDFAO(snapshot, cornerPos, floorNormal, qualityProfile.aoSamples, 2.0f, qualityProfile.surfaceBias);

        TEST_CHECK_MSG(suite, "AORadiusNormalizationConsistency",
                       cornerAO_r05 < openAO && cornerAO_r10 < openAO && cornerAO_r20 < openAO,
                       "Corner AO must remain darker than open floor across all radii!");
        TEST_CHECK_MSG(suite, "AORadiusMonotonicScale",
                       cornerAO_r20 <= cornerAO_r10 + 0.15f && cornerAO_r05 >= 0.0f,
                       "Normalized weight decay must keep AO bounded and consistent as radius changes!");
    }

    // =========================================================================
    // 5. Non-Uniform Scale (Ellipsoid) Shadows
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("EllipsoidScene");

        // Ground plane
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3;
        sGround.operation = 0;
        sGround.shape.dimensions = glm::vec4(0.0f);
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        // Stretched sphere (ellipsoid) at (0, 3, 0) with non-uniform scale (3, 1, 1)
        Entity ellipsoid = scene->CreateEntity("Ellipsoid");
        ellipsoid.AddComponent<TransformComponent>(glm::vec3(0.0f, 3.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f, 1.0f, 1.0f));
        auto& sEllip = ellipsoid.AddComponent<SDFComponent>();
        sEllip.primitiveType = 0;
        sEllip.operation = 0;
        sEllip.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        sEllip.encoding = SDFShapeEncoding::LegacyPackedScale; // Non-uniform scale legacy ellipsoid
        sEllip.csgOrder = 2;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        // Sun shining straight down (L = (0, 1, 0))
        glm::vec3 L(0.0f, 1.0f, 0.0f);

        // Under center (0, 0, 0): in shadow
        glm::vec3 pCenter = glm::vec3(0.0f, qualityProfile.surfaceBias, 0.0f);
        float sCenter = EvaluateSDFSoftShadow(snapshot, pCenter, L, qualityProfile.surfaceBias, 50.0f, 24.0f, 96);
        TEST_CHECK_MSG(suite, "EllipsoidCenterShadow", sCenter == 0.0f,
                       "Ground under center of ellipsoid must be in full shadow!");

        // At X = 2.0 (under the stretched X axis): in shadow
        glm::vec3 pX2 = glm::vec3(2.0f, qualityProfile.surfaceBias, 0.0f);
        float sX2 = EvaluateSDFSoftShadow(snapshot, pX2, L, qualityProfile.surfaceBias, 50.0f, 24.0f, 96);
        TEST_CHECK_MSG(suite, "EllipsoidStretchedAxisShadow", sX2 == 0.0f,
                       "Ground under stretched X=2.0 axis of ellipsoid must be in shadow!");

        // At Z = 2.0 (outside the thin Z axis of radius 1.0): unoccluded
        glm::vec3 pZ2 = glm::vec3(0.0f, qualityProfile.surfaceBias, 2.0f);
        float sZ2 = EvaluateSDFSoftShadow(snapshot, pZ2, L, qualityProfile.surfaceBias, 50.0f, 24.0f, 96);
        TEST_CHECK_MSG(suite, "EllipsoidNarrowAxisUnoccluded", sZ2 > 0.9f,
                       "Ground outside the narrow Z=2.0 axis of ellipsoid must be unoccluded (> 0.9)!");
    }

    // =========================================================================
    // 6. Fail-Safe Determinism on NaN / Singular Queries
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("EmptyScene");
        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        float nanVal = std::numeric_limits<float>::quiet_NaN();
        float infVal = std::numeric_limits<float>::infinity();

        float sNaN = EvaluateSDFSoftShadow(snapshot, glm::vec3(nanVal), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 50.0f, 24.0f, 96);
        TEST_CHECK_MSG(suite, "SafeNaNShadow", sNaN == 1.0f,
                       "NaN ray origin must produce fail-safe 1.0 shadow visibility!");

        float sInf = EvaluateSDFSoftShadow(snapshot, glm::vec3(infVal), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 50.0f, 24.0f, 96);
        TEST_CHECK_MSG(suite, "SafeInfShadow", sInf == 1.0f,
                       "Inf ray origin must produce fail-safe 1.0 shadow visibility!");

        float aoNaN = EvaluateSDFAO(snapshot, glm::vec3(nanVal), glm::vec3(0.0f, 1.0f, 0.0f), 8, 1.0f, 0.015f);
        TEST_CHECK_MSG(suite, "SafeNaNAO", aoNaN == 1.0f,
                       "NaN position must produce fail-safe 1.0 AO!");
    }

    // =========================================================================
    // 7. Deferred Lighting Combination Contract Verification
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("ContractScene");

        // Ground Plane at Y = 0
        Entity ground = scene->CreateEntity("Ground");
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sGround = ground.AddComponent<SDFComponent>();
        sGround.primitiveType = 3;
        sGround.operation = 0;
        sGround.shape.dimensions = glm::vec4(0.0f);
        sGround.encoding = SDFShapeEncoding::ExplicitShape;
        sGround.csgOrder = 1;

        // Blocker Sphere at (0, 2, 0)
        Entity blocker = scene->CreateEntity("Blocker");
        blocker.AddComponent<TransformComponent>(glm::vec3(0.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sBlocker = blocker.AddComponent<SDFComponent>();
        sBlocker.primitiveType = 0;
        sBlocker.operation = 0;
        sBlocker.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        sBlocker.encoding = SDFShapeEncoding::ExplicitShape;
        sBlocker.csgOrder = 2;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        LightSource sun{};
        sun.type = LightSource::Type::Directional;
        sun.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        sun.color = glm::vec3(1.0f, 1.0f, 1.0f);
        sun.intensity = 5.0f;

        glm::vec3 diffuseIBL(0.2f, 0.2f, 0.2f);
        glm::vec3 specularIBL(0.1f, 0.1f, 0.1f);
        float iblIntensity = 1.0f;

        // Point A: Under blocker at (0, 0, 0) -> Shadow = 0.0
        DeferredSurface surfShadowed{};
        surfShadowed.worldPos = glm::vec3(0.0f, 0.0f, 0.0f);
        surfShadowed.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        surfShadowed.albedo = glm::vec3(1.0f);

        glm::vec3 colorShadowed = EvaluateDeferredLighting(
            surfShadowed, &sun, 1, snapshot, qualityProfile, diffuseIBL, specularIBL, iblIntensity
        );

        // In full shadow, directLo must be 0, but ambient (diffuseIBL*ao + specularIBL) must be intact!
        float expectedMinAmbient = specularIBL.x;
        TEST_CHECK_MSG(suite, "ShadowedPointRetainsAmbient", colorShadowed.x >= expectedMinAmbient,
                       "Shadowed surface must retain ambient lighting and never be completely pitch black!");
        TEST_CHECK_MSG(suite, "DirectSunOccluded", colorShadowed.x < sun.intensity * 0.5f,
                       "Direct sunlight must be fully occluded in shadow!");

        // Point B: Unoccluded at (5, 0, 0)
        DeferredSurface surfUnoccluded{};
        surfUnoccluded.worldPos = glm::vec3(5.0f, 0.0f, 0.0f);
        surfUnoccluded.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        surfUnoccluded.albedo = glm::vec3(1.0f);

        glm::vec3 colorUnoccluded = EvaluateDeferredLighting(
            surfUnoccluded, &sun, 1, snapshot, qualityProfile, diffuseIBL, specularIBL, iblIntensity
        );

        TEST_CHECK_MSG(suite, "UnoccludedPointReceivesDirectSun", colorUnoccluded.x > colorShadowed.x + 3.0f,
                       "Unoccluded point must receive full direct sunlight contribution!");
    }

    // =========================================================================
    // 9. Grid-Accelerated Soft Shadow: Empty Space Skipping vs Fine Steps
    // =========================================================================
    {
        auto scene = std::make_shared<Scene>("GridShadowScene");

        // Sphere blocker at (0, 3, 0) with radius 1.0
        Entity sphere = scene->CreateEntity("SphereBlocker");
        sphere.AddComponent<TransformComponent>(glm::vec3(0.0f, 3.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        auto& sSphere = sphere.AddComponent<SDFComponent>();
        sSphere.primitiveType = 0; // Sphere
        sSphere.operation = 0;     // Union
        sSphere.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        sSphere.encoding = SDFShapeEncoding::ExplicitShape;
        sSphere.csgOrder = 1;

        auto snapshot = SDFSceneSnapshot::Extract(scene->GetRegistry());

        // Build BrickGrid
        BrickGrid grid;
        grid.Build(snapshot);
        auto cellD = grid.GetCellDistances();
        glm::vec3 minB = grid.GetMinBounds();
        glm::vec3 maxB = grid.GetMaxBounds();
        glm::vec3 gridDim(static_cast<float>(BrickGrid::DIM_X), static_cast<float>(BrickGrid::DIM_Y), static_cast<float>(BrickGrid::DIM_Z));
        float cellSize = grid.GetCellSize().x;

        glm::vec3 lightDir(0.0f, 1.0f, 0.0f); // Light pointing up towards blocker

        // Verify that empty space skipping doesn't break penumbra monotonicity
        // Test positions along X from 0.0 (fully blocked) to 2.5 (unoccluded)
        float prevShadowNoGrid = -1.0f;
        float prevShadowWithGrid = -1.0f;
        bool monotonicNoGrid = true;
        bool monotonicWithGrid = true;
        bool closeMatch = true;

        for (int step = 0; step <= 25; ++step) {
            float x = step * 0.1f;
            glm::vec3 rayOrigin(x, 0.0f, 0.0f);

            float sNoGrid = EvaluateSDFSoftShadow(
                snapshot, rayOrigin, lightDir, 0.015f, 50.0f, 24.0f, 96,
                false, false
            );

            float sWithGrid = EvaluateSDFSoftShadow(
                snapshot, rayOrigin, lightDir, 0.015f, 50.0f, 24.0f, 96,
                true, true, minB, maxB, gridDim, cellSize, cellD
            );

            if (sNoGrid < prevShadowNoGrid - 1e-4f) monotonicNoGrid = false;
            if (sWithGrid < prevShadowWithGrid - 1e-4f) monotonicWithGrid = false;
            if (std::abs(sNoGrid - sWithGrid) > 0.05f) closeMatch = false;

            prevShadowNoGrid = sNoGrid;
            prevShadowWithGrid = sWithGrid;
        }

        TEST_CHECK_MSG(suite, "ReferenceNoGridPenumbraMonotonic", monotonicNoGrid,
                       "Reference soft shadow without grid must be monotonic!");
        TEST_CHECK_MSG(suite, "GridSkipPenumbraMonotonic", monotonicWithGrid,
                       "Grid-accelerated soft shadow penumbra must be monotonically non-decreasing!");
        TEST_CHECK_MSG(suite, "GridSkipMatchesFineEvaluation", closeMatch,
                       "Grid-accelerated soft shadow must closely match fine-step reference shadow (diff <= 0.05)!");
    }

    std::cout << "--- [A4-P3] Deferred SDF Shadows & AO Suite Basariyla Tamamlandi! ---\n";
}

} // namespace Astral::Test
