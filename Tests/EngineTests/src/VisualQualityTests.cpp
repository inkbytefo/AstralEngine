#include "TestFramework.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SDFWorldQuery.hpp"
#include "Astral/Renderer/EnvironmentImage.hpp"
#include "Astral/Renderer/BrickGrid.hpp"
#include <fstream>

namespace Astral::Test {
void RunVisualQualityTests() {
    const std::string suite = "VisualQuality";
    Scene scene;
    auto entity = scene.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SDFComponent>();
    auto& registry = scene.GetRegistry();
    std::vector<SDFEditGPU> edits;
    UpdateWorldTransforms(registry);
    ExtractRenderData(registry, edits);
    TEST_CHECK(suite, "FirstFramePreviousEqualsCurrent", edits[0].GetPrevPosition() == edits[0].position);
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {1, 2, 3};
    transform.rotation = glm::angleAxis(1.0f, glm::vec3(0, 1, 0));
    transform.scale = {0.5f, 2, 1};
    UpdateWorldTransforms(registry);
    ExtractRenderData(registry, edits);
    TEST_CHECK(suite, "TranslationHistory", edits[0].GetPrevPosition() == glm::vec3(0));
    TEST_CHECK(suite, "RotationHistory", edits[0].prevRotation == glm::vec4(0, 0, 0, 1));
    TEST_CHECK(suite, "ScaleHistory", glm::vec3(edits[0].prevScale) == glm::vec3(1));
    UpdateWorldTransforms(registry);
    ExtractRenderData(registry, edits);
    TEST_CHECK(suite, "HistoryAdvances", edits[0].prevRotation == edits[0].rotation);
    Scene other;
    auto fresh = other.CreateEntity();
    fresh.AddComponent<TransformComponent>(glm::vec3(7));
    fresh.AddComponent<SDFComponent>();
    UpdateWorldTransforms(other.GetRegistry());
    ExtractRenderData(other.GetRegistry(), edits);
    TEST_CHECK(suite, "SceneHistoryIsolation", edits[0].GetPrevPosition() == glm::vec3(7));
    transform.position = {0, 0, 0}; transform.rotation = {1, 0, 0, 0};
    TEST_CHECK(suite, "EllipsoidYSurface", std::abs(SDFWorldQuery::QueryDistance(registry, {0, 2, 0})) < 1e-5f);
    TEST_CHECK(suite, "EllipsoidXSurface", std::abs(SDFWorldQuery::QueryDistance(registry, {0.5f, 0, 0})) < 1e-5f);
    TEST_CHECK(suite, "EllipsoidExterior", SDFWorldQuery::QueryDistance(registry, {0, 3, 0}) > 0);

    const auto path = std::filesystem::temp_directory_path() / "astral_a4_environment_test.hdr";
    {
        std::ofstream file(path, std::ios::binary);
        file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n";
        for (int y = 0; y < 2; ++y) {
            const unsigned char row[]{2, 2, 0, 8, 136, 128, 136, 64, 136, 32, 136, 130};
            file.write(reinterpret_cast<const char*>(row), sizeof(row));
        }
    }
    const auto hdr = EnvironmentImage::LoadRadiance(path);
    for (const auto dir : {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, -1, 0)})
        TEST_CHECK(suite, "HDRLinearRadianceAndSeams", glm::length(hdr.Sample(dir) - glm::vec3(2, 1, 0.5f)) < 1e-5f);
    { std::ofstream file(path, std::ios::binary | std::ios::trunc); file << "#?RADIANCE\n"; }
    bool rejected = false;
    try { (void)EnvironmentImage::LoadRadiance(path); } catch (const std::runtime_error&) { rejected = true; }
    TEST_CHECK(suite, "TruncatedHDRRejected", rejected);
    std::filesystem::remove(path);

    BrickGrid incremental;
    edits.resize(2);
    edits[0].scale = glm::vec3(0.25f); edits[1].scale = glm::vec3(0.4f);
    incremental.Build(edits);
    edits[0].position.x = 5;
    incremental.Build(edits);
    BrickGrid full; full.Build(edits);
    TEST_CHECK(suite, "GridIncrementalMatchesFull", std::equal(incremental.GetCellDistances().begin(),
               incremental.GetCellDistances().end(), full.GetCellDistances().begin()));
}
}
