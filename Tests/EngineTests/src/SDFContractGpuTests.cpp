#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

namespace Astral::Test {

class SDFContractGpuApp : public Astral::Application {
public:
    explicit SDFContractGpuApp(int maxFrames = 3)
        : Astral::Application(BuildConfig(maxFrames)) {}

    std::shared_ptr<Scene> capturedScene;

protected:
    void OnUpdate(FrameContext&, uint32_t) override {
        // Request center pixel pick to test thread-safe GPU selection & readback
        RequestPick(640, 360);
    }

private:
    std::shared_ptr<Scene> CreateInitialScene() override {
        auto scene = std::make_shared<Scene>("ContractGpuScene");

        // Sphere at origin, radius = 1.0
        Entity e1 = scene->CreateEntity("Sphere");
        e1.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f));
        auto& s1 = e1.AddComponent<SDFComponent>();
        s1.primitiveType = 0; // Sphere
        s1.operation = 0;     // Union
        s1.shape.dimensions = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        s1.encoding = SDFShapeEncoding::ExplicitShape;
        s1.csgOrder = 1;

        // Box at (1.2, 0, 0), half-extents = (0.5, 0.5, 0.5)
        Entity e2 = scene->CreateEntity("Box");
        e2.AddComponent<TransformComponent>(glm::vec3(1.2f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f));
        auto& s2 = e2.AddComponent<SDFComponent>();
        s2.primitiveType = 1; // Box
        s2.operation = 3;     // Smooth Union
        s2.blendFactor = 0.2f;
        s2.shape.dimensions = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
        s2.encoding = SDFShapeEncoding::ExplicitShape;
        s2.csgOrder = 2;

        Entity cam = scene->CreateEntity("Camera");
        cam.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 4.0f));
        cam.AddComponent<CameraComponent>(glm::radians(60.0f), 0.01f, 50.0f, 1u);

        capturedScene = scene;
        return scene;
    }

    static Astral::AppConfig BuildConfig(int maxFrames) {
        Astral::AppConfig config;
        config.maxFrames = maxFrames;
        config.width = 1280;
        config.height = 720;
        config.enableTAA = false; // Deterministic ray paths without temporal jitter
        config.useGrid = true;
        config.optShadow = true;
        return config;
    }
};

void RunSDFContractGpuTests() {
    const std::string suite = "SDF Contract GPU Suite";
    std::cout << "  [INFO] Vulkan 1.4 SDF Contract GPU Testi baslatiliyor (Snapshot ve GPU distance readback)...\n";

    bool appCompletedSuccessfully = false;
    uint32_t framesRendered = 0;

    try {
        SDFContractGpuApp app(3);
        app.Run();
        framesRendered = app.GetTotalFramesRendered();
        appCompletedSuccessfully = (framesRendered == 3);

        TEST_CHECK_MSG(suite, "SDFContractGpuExecution", appCompletedSuccessfully,
                       "Vulkan 1.4 SDF Compute ve ana render hatti basariyla tamamlanmali!");

        auto* context = app.GetVulkanContext();
        auto* renderer = app.GetRenderer();

        if (app.capturedScene && context && renderer) {
            const auto snapshot = SDFSceneSnapshot::Extract(app.capturedScene->GetRegistry());
            TEST_CHECK(suite, "SnapshotRecordsValid", snapshot.GetRecordCount() == 2);

            // 1. GPU Selection Buffer Readback Test (Center Ray)
            const auto pickRes = app.GetLastPickResult();
            TEST_CHECK_MSG(suite, "GpuCenterPickHit", pickRes.hasHit,
                           "Center ray looking at sphere must hit the surface!");

            if (pickRes.hasHit) {
                // Sphere at (0,0,0) radius 1, camera at (0,0,4) -> distance to front of sphere is ~3.0
                TEST_CHECK_MSG(suite, "GpuDistanceFinite", std::isfinite(pickRes.hitDistance),
                               "GPU distance must be a finite number!");
                TEST_CHECK_MSG(suite, "GpuDistanceSphereFront", std::abs(pickRes.hitDistance - 3.0f) < 0.05f,
                               "Center ray hitDistance must be ~3.0m (4.0 - 1.0)!");

                // CPU evaluation of the exact GPU hit point
                float cpuDistAtHit = snapshot.EvaluateDistance(pickRes.hitPoint);
                TEST_CHECK_MSG(suite, "CpuMatchesGpuHitPoint", std::abs(cpuDistAtHit) < 0.005f,
                               "CPU evaluated distance at GPU hit point must be within raymarch convergence threshold (< 0.005)!");
            }

            // 2. G-Buffer Depth GPU Readback Validation
            const int width = renderer->GetWidth();
            const int height = renderer->GetHeight();
            const size_t depthBytes = static_cast<size_t>(width) * height * sizeof(float);

            Buffer readbackBuffer(context->GetDevice(), context->GetPhysicalDevice(), depthBytes,
                                 vk::BufferUsageFlagBits::eTransferDst,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            context->ExecuteImmediate([&](vk::CommandBuffer cmd) {
                Astral::TransitionImage(cmd, renderer->GetGBufferDepth(),
                    Astral::ImageUse::ComputeRead, Astral::ImageUse::TransferRead);

                vk::BufferImageCopy region{};
                region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
                region.imageExtent = vk::Extent3D(width, height, 1);
                cmd.copyImageToBuffer(renderer->GetGBufferDepth(), vk::ImageLayout::eTransferSrcOptimal,
                                      readbackBuffer.GetBuffer(), region);

                Astral::TransitionImage(cmd, renderer->GetGBufferDepth(),
                    Astral::ImageUse::TransferRead, Astral::ImageUse::ComputeRead);
            });

            const float* gpuDepths = static_cast<const float*>(readbackBuffer.GetMappedData());
            TEST_CHECK_MSG(suite, "GpuDepthMapped", gpuDepths != nullptr,
                           "GPU G-Buffer depth buffer must be readable from host!");

            if (gpuDepths) {
                // Ray direction reconstruction for verification
                glm::vec3 camPos(0.0f, 0.0f, 4.0f);
                glm::vec3 fwd(0.0f, 0.0f, -1.0f);
                glm::vec3 right(1.0f, 0.0f, 0.0f);
                glm::vec3 up(0.0f, 1.0f, 0.0f);
                float fov = glm::radians(60.0f);
                float focalLength = 0.5f / std::tan(fov * 0.5f);

                size_t validHitCount = 0;
                float maxDeviation = 0.0f;

                // Test sample points across screen
                for (int y = 100; y < height - 100; y += 20) {
                    for (int x = 200; x < width - 200; x += 20) {
                        float gpuDist = gpuDepths[y * width + x];
                        if (gpuDist > 0.01f) {
                            validHitCount++;
                            glm::vec2 uv = (glm::vec2(x + 0.5f, y + 0.5f) - 0.5f * glm::vec2(width, height)) / float(height);
                            uv.y = -uv.y;
                            glm::vec3 rd = glm::normalize(fwd * focalLength + right * uv.x + up * uv.y);
                            glm::vec3 hitPoint = camPos + rd * gpuDist;

                            float cpuDist = snapshot.EvaluateDistance(hitPoint);
                            float dev = std::abs(cpuDist);
                            if (dev > maxDeviation) maxDeviation = dev;
                        }
                    }
                }

                TEST_CHECK_MSG(suite, "GpuDepthHitsDetected", validHitCount > 0,
                               "G-Buffer depth readback must detect surface hits!");
                TEST_CHECK_MSG(suite, "GpuDepthToleranceCompliant", maxDeviation < 0.005f,
                               "GPU raymarched depth hitpoints must match CPU distance within 0.005m tolerance!");
            }

            // 3. CPU snapshot 10,000 query evaluation check
            size_t queryCount = 10000;
            float maxVal = 0.0f;
            for (size_t i = 0; i < queryCount; ++i) {
                float angle = static_cast<float>(i) * 0.01f;
                float radius = 1.0f + static_cast<float>(i % 100) * 0.02f;
                glm::vec3 p(std::cos(angle) * radius, std::sin(angle) * 0.5f, std::sin(angle) * radius);

                float d = snapshot.EvaluateDistance(p);
                if (std::abs(d) > maxVal) maxVal = std::abs(d);
            }

            TEST_CHECK_MSG(suite, "Snapshot10kFinite", std::isfinite(maxVal),
                           "10,000 queries against snapshot must be completely finite!");
        }
    } catch (const std::exception& e) {
        std::cerr << "  [ERROR] SDF Contract GPU Test istisnasi: " << e.what() << "\n";
        TEST_CHECK_MSG(suite, "GpuExecutionException", false, e.what());
    }
}

} // namespace Astral::Test
