#include "TestFramework.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include <cmath>
#include <cstring>

namespace Astral::Test {

static void RunShapeTransformIndependenceTests(const std::string& suite) {
    // 1. Sphere
    {
        SDFShapeParameters sphere{glm::vec4(2.0f, 0.0f, 0.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(1.0f, 1.0f, 1.0f);
        const auto beforeScale = pose.scale;
        sphere.dimensions.x = 3.5f;
        TEST_CHECK_MSG(suite, "SphereRadiusEditPreservesScale", pose.scale == beforeScale,
                       "Sphere radius edit must not modify transform scale!");
        const auto beforeDims = sphere.dimensions;
        pose.scale = glm::vec3(2.5f, 2.5f, 2.5f);
        TEST_CHECK_MSG(suite, "SphereScaleEditPreservesDims", sphere.dimensions == beforeDims,
                       "Transform scale edit must not modify sphere dimensions!");
    }

    // 2. Box
    {
        SDFShapeParameters box{glm::vec4(1.5f, 2.0f, 3.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(1.0f);
        const auto beforeScale = pose.scale;
        box.dimensions.y = 4.0f;
        TEST_CHECK(suite, "BoxHalfExtentsPreservesScale", pose.scale == beforeScale);
        const auto beforeDims = box.dimensions;
        pose.scale = glm::vec3(0.5f, 1.2f, 2.0f);
        TEST_CHECK(suite, "BoxScalePreservesHalfExtents", box.dimensions == beforeDims);
    }

    // 3. Torus
    {
        SDFShapeParameters torus{glm::vec4(2.0f, 0.5f, 0.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(1.0f);
        const auto beforeScale = pose.scale;
        torus.dimensions.x = 3.0f;
        torus.dimensions.y = 0.8f;
        TEST_CHECK(suite, "TorusRadiiPreservesScale", pose.scale == beforeScale);
        const auto beforeDims = torus.dimensions;
        pose.scale = glm::vec3(1.5f, 1.5f, 1.5f);
        TEST_CHECK(suite, "TorusScalePreservesRadii", torus.dimensions == beforeDims);
    }

    // 4. Capsule
    {
        SDFShapeParameters capsule{glm::vec4(0.4f, 2.0f, 0.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(1.0f);
        const auto beforeScale = pose.scale;
        capsule.dimensions.y = 3.5f;
        TEST_CHECK(suite, "CapsuleLengthPreservesScale", pose.scale == beforeScale);
        const auto beforeDims = capsule.dimensions;
        pose.scale = glm::vec3(0.8f);
        TEST_CHECK(suite, "CapsuleScalePreservesDims", capsule.dimensions == beforeDims);
    }

    // 5. Cylinder
    {
        SDFShapeParameters cylinder{glm::vec4(0.6f, 1.8f, 0.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(1.0f);
        const auto beforeScale = pose.scale;
        cylinder.dimensions.x = 1.0f;
        cylinder.dimensions.y = 2.5f;
        TEST_CHECK(suite, "CylinderDimsPreservesScale", pose.scale == beforeScale);
        const auto beforeDims = cylinder.dimensions;
        pose.scale = glm::vec3(2.0f);
        TEST_CHECK(suite, "CylinderScalePreservesDims", cylinder.dimensions == beforeDims);
    }

    // 6. Plane
    {
        SDFShapeParameters plane{glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)};
        TransformComponent pose;
        pose.scale = glm::vec3(10.0f, 1.0f, 10.0f);
        const auto beforeScale = pose.scale;
        plane.dimensions.x = -1.5f;
        TEST_CHECK(suite, "PlaneOffsetPreservesScale", pose.scale == beforeScale);
        const auto beforeDims = plane.dimensions;
        pose.scale = glm::vec3(20.0f, 2.0f, 20.0f);
        TEST_CHECK(suite, "PlaneScalePreservesOffset", plane.dimensions == beforeDims);
    }
}

static void RunV1LegacyCompatibilityTests(const std::string& suite) {
    const std::filesystem::path testDir = std::filesystem::temp_directory_path() / "astral_test_sdf_shape";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir);
    const std::filesystem::path testFile = testDir / "v1_legacy_chunk.astral";

    {
        std::ofstream ss(testFile, std::ios::binary);

        // 1. SceneFileHeader (version = 2 is supported)
        SceneFileHeader header{};
        std::memcpy(header.magic, SceneSerializer::MAGIC, 4);
        header.version = 2;
        header.activeEntityCount = 102;
        ss.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // 2. Transform Chunk
        ComponentChunkHeader trHeader{
            .typeId = ComponentTraits<TransformComponent>::TypeHash,
            .version = 1,
            .flags = 0,
            .elementCount = 2,
            .entityDataSize = 2 * sizeof(EntityHandle),
            .componentDataSize = 2 * sizeof(float) * 10
        };
        ss.write(reinterpret_cast<const char*>(&trHeader), sizeof(trHeader));
        EntityHandle h0 = 100, h1 = 101;
        ss.write(reinterpret_cast<const char*>(&h0), sizeof(h0));
        ss.write(reinterpret_cast<const char*>(&h1), sizeof(h1));
        float trData[20] = {
            0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
            2.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f,  0.5f, 0.7f, 0.3f
        };
        ss.write(reinterpret_cast<const char*>(trData), sizeof(trData));

        // 3. SDF Chunk Version 1 (40 bytes per elem)
        ComponentChunkHeader sdfHeader{
            .typeId = ComponentTraits<SDFComponent>::TypeHash,
            .version = 1, // Legacy format!
            .flags = 0,
            .elementCount = 2,
            .entityDataSize = 2 * sizeof(EntityHandle),
            .componentDataSize = 2 * 40
        };
        ss.write(reinterpret_cast<const char*>(&sdfHeader), sizeof(sdfHeader));
        ss.write(reinterpret_cast<const char*>(&h0), sizeof(h0));
        ss.write(reinterpret_cast<const char*>(&h1), sizeof(h1));

        uint32_t s0_u[4] = { 0, 0, 1, 1 };
        float s0_f[6] = { 0.1f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f };
        ss.write(reinterpret_cast<const char*>(&s0_u[0]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_u[1]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[0]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_u[2]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[1]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[2]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[3]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[4]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_f[5]), 4);
        ss.write(reinterpret_cast<const char*>(&s0_u[3]), 4);

        uint32_t s1_u[4] = { 1, 3, 0, 1 };
        float s1_f[6] = { 0.2f, 0.0f, 1.0f, 0.0f, 0.2f, 0.8f };
        ss.write(reinterpret_cast<const char*>(&s1_u[0]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_u[1]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[0]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_u[2]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[1]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[2]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[3]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[4]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_f[5]), 4);
        ss.write(reinterpret_cast<const char*>(&s1_u[3]), 4);
    }

    auto loadedScene = std::make_shared<Scene>("Test");
    bool ok = SceneSerializer::Deserialize(loadedScene, testFile);
    TEST_CHECK_MSG(suite, "LegacyV1DeserializationSucceeds", ok, "Legacy V1 chunk must deserialize successfully!");

    auto& sdfView = loadedScene->GetRegistry().GetView<SDFComponent>();
    TEST_CHECK(suite, "LegacyV1Count", sdfView.Size() == 2);
    if (sdfView.Size() == 2) {
        const auto& sdf0 = sdfView.Data()[0];
        const auto& sdf1 = sdfView.Data()[1];
        TEST_CHECK_MSG(suite, "LegacyV1MarkedLegacyPackedScale0",
                       sdf0.encoding == SDFShapeEncoding::LegacyPackedScale,
                       "V1 chunk must be tagged as LegacyPackedScale!");
        TEST_CHECK_MSG(suite, "LegacyV1MarkedLegacyPackedScale1",
                       sdf1.encoding == SDFShapeEncoding::LegacyPackedScale,
                       "V1 chunk must be tagged as LegacyPackedScale!");
        TEST_CHECK(suite, "LegacyV1PreservesCSGOrder0", sdf0.csgOrder == 0);
        TEST_CHECK(suite, "LegacyV1PreservesCSGOrder1", sdf1.csgOrder == 1);
        TEST_CHECK(suite, "LegacyV1PreservesPrimitive0", sdf0.primitiveType == 0);
        TEST_CHECK(suite, "LegacyV1PreservesPrimitive1", sdf1.primitiveType == 1);
    }

    std::filesystem::remove_all(testDir, ec);
}

static void RunV2SerializationRoundtripAndValidationTests(const std::string& suite) {
    const std::filesystem::path testDir = std::filesystem::temp_directory_path() / "astral_test_sdf_shape_v2";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir);
    const std::filesystem::path testFile = testDir / "v2_scene.astral";

    // Roundtrip test
    auto scene = std::make_shared<Scene>("V2Scene");
    Entity e1 = scene->CreateEntity("SphereObj");
    e1.AddComponent<TransformComponent>(glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    auto& sdf1 = e1.AddComponent<SDFComponent>();
    sdf1.primitiveType = 0;
    sdf1.operation = 0;
    sdf1.shape.dimensions = glm::vec4(2.75f, 0.0f, 0.0f, 0.0f);
    sdf1.encoding = SDFShapeEncoding::ExplicitShape;
    sdf1.csgOrder = 42;

    Entity e2 = scene->CreateEntity("BoxObj");
    e2.AddComponent<TransformComponent>(glm::vec3(-1.0f, 0.0f, 1.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(2.0f));
    auto& sdf2 = e2.AddComponent<SDFComponent>();
    sdf2.primitiveType = 1;
    sdf2.operation = 3;
    sdf2.shape.dimensions = glm::vec4(0.5f, 1.5f, 2.5f, 0.0f);
    sdf2.encoding = SDFShapeEncoding::ExplicitShape;
    sdf2.csgOrder = 99;

    bool serOk = SceneSerializer::Serialize(scene, testFile);
    TEST_CHECK(suite, "V2SerializeSuccess", serOk);

    auto loaded = std::make_shared<Scene>("LoadedV2");
    bool deserOk = SceneSerializer::Deserialize(loaded, testFile);
    TEST_CHECK(suite, "V2DeserializeSuccess", deserOk);

    auto& view = loaded->GetRegistry().GetView<SDFComponent>();
    TEST_CHECK(suite, "V2LoadedCount", view.Size() == 2);
    if (view.Size() == 2) {
        const auto& s1 = view.Data()[0];
        const auto& s2 = view.Data()[1];
        TEST_CHECK(suite, "V2ShapeDims1", std::abs(s1.shape.dimensions.x - 2.75f) < 1e-5f);
        TEST_CHECK(suite, "V2Encoding1", s1.encoding == SDFShapeEncoding::ExplicitShape);
        TEST_CHECK(suite, "V2CsgOrder1", s1.csgOrder == 42);

        TEST_CHECK(suite, "V2ShapeDims2", std::abs(s2.shape.dimensions.y - 1.5f) < 1e-5f);
        TEST_CHECK(suite, "V2Encoding2", s2.encoding == SDFShapeEncoding::ExplicitShape);
        TEST_CHECK(suite, "V2CsgOrder2", s2.csgOrder == 99);
    }

    // Validation: Negative radius rejection
    {
        e1.GetComponent<SDFComponent>().shape.dimensions.x = -1.0f; // Negative sphere radius!
        const std::filesystem::path badFile = testDir / "bad_neg_radius.astral";
        (void)SceneSerializer::Serialize(scene, badFile);
        auto badLoaded = std::make_shared<Scene>("Bad");
        bool badDeser = SceneSerializer::Deserialize(badLoaded, badFile);
        TEST_CHECK_MSG(suite, "NegativeRadiusRejected", !badDeser, "Negative sphere radius must fail deserialization!");
    }

    // Validation: NaN dimension rejection
    {
        e1.GetComponent<SDFComponent>().shape.dimensions.x = std::numeric_limits<float>::quiet_NaN();
        const std::filesystem::path badFile = testDir / "bad_nan_dims.astral";
        (void)SceneSerializer::Serialize(scene, badFile);
        auto badLoaded = std::make_shared<Scene>("Bad");
        bool badDeser = SceneSerializer::Deserialize(badLoaded, badFile);
        TEST_CHECK_MSG(suite, "NaNDimensionsRejected", !badDeser, "NaN dimensions must fail deserialization!");
    }

    std::filesystem::remove_all(testDir, ec);
}

void RunSDFShapeTests() {
    const std::string suite = "SDFShapeSuite";
    RunShapeTransformIndependenceTests(suite);
    RunV1LegacyCompatibilityTests(suite);
    RunV2SerializationRoundtripAndValidationTests(suite);
}

} // namespace Astral::Test
