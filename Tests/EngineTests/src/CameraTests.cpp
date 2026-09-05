#include "TestFramework.hpp"
#include "Astral/Core/Application.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneSerializer.hpp"
#include <filesystem>
#include <limits>
#include <cmath>

namespace Astral::Test {
void RunCameraTests() {
    const std::string suite = "Camera";
    class EmptyApp final : public Application {
    public:
        using Application::CreateInitialScene;
    };
    EmptyApp app;
    auto empty = app.CreateInitialScene();
    TEST_CHECK(suite, "DefaultSceneIsEmpty", empty && empty->GetRegistry().GetAliveEntityCount() == 0);
    auto& registry = empty->GetRegistry();
    TEST_CHECK(suite, "NoImplicitCamera", !ExtractActiveCamera(registry, 16.0f / 9.0f));
    auto camera = empty->CreateEntity();
    camera.AddComponent<TransformComponent>(glm::vec3(2, 3, 4));
    camera.AddComponent<CameraComponent>();
    TEST_CHECK(suite, "CameraRequiresSelection", !ExtractActiveCamera(registry, 1.0f));
    TEST_CHECK(suite, "SelectCamera", SetActiveCamera(registry, camera.GetHandle()));
    UpdateWorldTransforms(registry);
    auto result = ExtractActiveCamera(registry, 2.0f);
    TEST_CHECK(suite, "CameraExtracted", result.has_value());
    if (result) {
        TEST_CHECK(suite, "Position", glm::length(result->position - glm::vec3(2, 3, 4)) < 1e-5f);
        TEST_CHECK(suite, "Forward", glm::length(result->forward - glm::vec3(0, 0, -1)) < 1e-5f);
        TEST_CHECK(suite, "Aspect", std::abs(result->projection[1][1] / result->projection[0][0] - 2.0f) < 1e-5f);
    }
    auto second = empty->CreateEntity();
    second.AddComponent<TransformComponent>();
    second.AddComponent<CameraComponent>();
    TEST_CHECK(suite, "Switch", SetActiveCamera(registry, second.GetHandle()));
    TEST_CHECK(suite, "PreviousDeactivated", camera.GetComponent<CameraComponent>().primary == 0);
    TEST_CHECK(suite, "InvalidSelectionRejected", !SetActiveCamera(registry, MakeEntityHandle(9999, 1)));
    TEST_CHECK(suite, "InvalidSelectionPreservesCamera", second.GetComponent<CameraComponent>().primary == 1);
    second.GetComponent<CameraComponent>().verticalFovRadians = std::numeric_limits<float>::quiet_NaN();
    TEST_CHECK(suite, "InvalidLensRejected", !ExtractActiveCamera(registry, 1.0f));
    second.GetComponent<CameraComponent>() = CameraComponent{};
    TEST_CHECK(suite, "Reselect", SetActiveCamera(registry, second.GetHandle()));
    TEST_CHECK(suite, "InvalidAspectRejected", !ExtractActiveCamera(registry, 0.0f));
    empty->DestroyEntity(second);
    TEST_CHECK(suite, "DeletedCameraNotReplacedImplicitly", !ExtractActiveCamera(registry, 1.0f));
    TEST_CHECK(suite, "ClearSelection", SetActiveCamera(registry, NullEntityHandle));

    // Camera hierarchy and roll must survive extraction, copy, duplication and persistence.
    auto parent = empty->CreateEntity();
    parent.AddComponent<TransformComponent>(glm::vec3(5, 0, 0),
        glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1)));
    camera.GetComponent<TransformComponent>().position = glm::vec3(0, 2, 0);
    TEST_CHECK(suite, "ParentCamera", empty->SetParent(camera, parent));
    TEST_CHECK(suite, "ActivateParentedCamera", SetActiveCamera(registry, camera.GetHandle()));
    UpdateWorldTransforms(registry);
    result = ExtractActiveCamera(registry, 1.0f);
    TEST_CHECK(suite, "ParentedPosition", result && glm::length(result->position - glm::vec3(3, 0, 0)) < 1e-4f);
    TEST_CHECK(suite, "RollPreserved", result && glm::length(result->up - glm::vec3(-1, 0, 0)) < 1e-4f);
    auto duplicate = empty->DuplicateEntity(camera);
    TEST_CHECK(suite, "DuplicateCopiesLens", duplicate.HasComponent<CameraComponent>());
    TEST_CHECK(suite, "DuplicateDoesNotStealSelection", duplicate.HasComponent<CameraComponent>() && duplicate.GetComponent<CameraComponent>().primary == 0);
    auto copied = Scene::Copy(empty);
    TEST_CHECK(suite, "CopiedCamera", ExtractActiveCamera(copied->GetRegistry(), 1.0f).has_value());
    const auto testDir = std::filesystem::temp_directory_path() / "astral_test_camera";
    std::filesystem::create_directories(testDir);
    const auto path = testDir / "camera-roundtrip.astral";
    TEST_CHECK(suite, "SaveCamera", SceneSerializer::Serialize(empty, path));
    auto loaded = std::make_shared<Scene>();
    TEST_CHECK(suite, "LoadCamera", SceneSerializer::Deserialize(loaded, path));
    auto restored = ExtractActiveCamera(loaded->GetRegistry(), 1.0f);
    TEST_CHECK(suite, "RestoredCameraPose", restored && glm::length(restored->position - glm::vec3(3, 0, 0)) < 1e-4f);
    TEST_CHECK(suite, "RestoredLens", restored && std::abs(restored->projection[1][1] - result->projection[1][1]) < 1e-5f);
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
}
}
