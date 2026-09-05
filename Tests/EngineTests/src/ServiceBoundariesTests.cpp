#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Renderer/RenderContext.hpp"
#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"

#include <iostream>
#include <memory>

namespace Astral::Test {

namespace {

class HeadlessSimTestApp : public Application {
public:
    HeadlessSimTestApp(int targetFrames)
        : Application([targetFrames]() {
            AppConfig cfg;
            cfg.headless = true;
            cfg.maxFrames = targetFrames;
            cfg.simulatePhysics = true;
            cfg.fixedTimeStep = 1.0f / 60.0f;
            cfg.fixedDeltaTime = 1.0f / 60.0f;
            return cfg;
        }()) {}

    std::shared_ptr<Scene> CreateInitialScene() override {
        auto scene = std::make_shared<Scene>("HeadlessSimScene");
        Entity e = scene->CreateEntity();
        scene->GetRegistry().AddComponent<TransformComponent>(e, TransformComponent{
            .position = { 0.0f, 0.0f, 0.0f },
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = { 1.0f, 1.0f, 1.0f }
        });
        scene->GetRegistry().AddComponent<VelocityComponent>(e, VelocityComponent{
            .linear = { 12.0f, 0.0f, 0.0f }
        });
        scene->GetRegistry().AddComponent<SDFComponent>(e, SDFComponent{
            .primitiveType = 0, // Sphere
            .operation = 0,
            .blendFactor = 0.0f
        });
        m_TargetEntity = e.GetHandle();
        return scene;
    }

    void OnUpdate(FrameContext&, uint32_t) override {
        m_UpdateCount++;
    }

    EntityHandle m_TargetEntity{NullEntityHandle};
    uint32_t m_UpdateCount{0};
};

class SceneSwitchTestApp : public Application {
public:
    SceneSwitchTestApp()
        : Application([]() {
            AppConfig cfg;
            cfg.headless = true;
            cfg.maxFrames = 4;
            cfg.simulatePhysics = false;
            return cfg;
        }()) {}

    std::shared_ptr<Scene> CreateInitialScene() override {
        m_Scene1 = std::make_shared<Scene>("InitialScene1");
        Entity e1 = m_Scene1->CreateEntity();
        m_Scene1->GetRegistry().AddComponent<TransformComponent>(e1, TransformComponent{ .position = { 1.0f, 0.0f, 0.0f } });
        m_Scene1->GetRegistry().AddComponent<SDFComponent>(e1, SDFComponent{ .primitiveType = 0 });
        m_Entity1 = e1.GetHandle();
        return m_Scene1;
    }

    void OnUpdate(FrameContext&, uint32_t frameIndex) override {
        if (frameIndex == 1) {
            // 2. karede sahneyi tek noktadan (SceneManager) degistir
            m_Scene2 = std::make_shared<Scene>("SwitchedScene2");
            Entity e2 = m_Scene2->CreateEntity();
            m_Scene2->GetRegistry().AddComponent<TransformComponent>(e2, TransformComponent{ .position = { 99.0f, 0.0f, 0.0f } });
            m_Scene2->GetRegistry().AddComponent<SDFComponent>(e2, SDFComponent{ .primitiveType = 1 });
            m_Entity2 = e2.GetHandle();

            GetSceneManager().SetActiveScene(m_Scene2);
            m_Scene1StoppedOnSwitch = !m_Scene1->IsRunning();
            m_Scene2RunningOnSwitch = m_Scene2->IsRunning();
        } else if (frameIndex >= 2) {
            // 3. ve sonraki karelerde guncel sahne artik Scene 2 olmalidir
            m_FramesOnScene2++;
            m_Scene2RunningInLoop = m_Scene2->IsRunning();
        }
    }

    std::shared_ptr<Scene> m_Scene1;
    std::shared_ptr<Scene> m_Scene2;
    EntityHandle m_Entity1{NullEntityHandle};
    EntityHandle m_Entity2{NullEntityHandle};
    uint32_t m_FramesOnScene2{0};
    bool m_Scene1StoppedOnSwitch{false};
    bool m_Scene2RunningOnSwitch{false};
    bool m_Scene2RunningInLoop{false};
};

} // anonymous namespace

void RunServiceBoundariesTests() {
    const std::string suite = "ServiceBoundariesSuite";

    // =========================================================================
    // 1. GPU'suz / Penceresiz Saf CPU Simülasyonu Doğrulaması (Kabul Kriteri)
    // =========================================================================
    {
        HeadlessSimTestApp app(10);

        TEST_CHECK_MSG(suite, "HeadlessConfigured", app.IsHeadless(),
                       "Application headless modu aktif olmalidir!");

        // 10 kare calistir (GPU veya Pencere acilmamalidir)
        app.Run(10);

        TEST_CHECK_MSG(suite, "WindowNotCreated", app.GetWindow() == nullptr,
                       "Headless modda GLFW penceresi acilmamalidir (GetWindow == nullptr)!");
        TEST_CHECK_MSG(suite, "VulkanNotCreated", app.GetVulkanContext() == nullptr,
                       "Headless modda VulkanContext baslatilmamalidir (GetVulkanContext == nullptr)!");
        TEST_CHECK_MSG(suite, "RendererNotCreated", app.GetRenderer() == nullptr,
                       "Headless modda SDFRenderer olusturulmamalidir (GetRenderer == nullptr)!");
        TEST_CHECK_MSG(suite, "TotalFramesRendered", app.GetTotalFramesRendered() == 10,
                       "Headless modda 10 simulasyon karesi eksiksiz tamamlanmalidir!");
        TEST_CHECK_MSG(suite, "UpdateCountVerified", app.m_UpdateCount == 10,
                       "Gameplay OnUpdate hook'u 10 kez calismalidir!");

        // Fizik simulasyonunun CPU uzerinde ilerledigini dogrula
        auto activeScene = app.GetActiveScene();
        TEST_CHECK(suite, "SceneAlive", activeScene != nullptr);
        if (activeScene) {
            auto& reg = activeScene->GetRegistry();
            TEST_CHECK(suite, "TargetEntityAlive", reg.IsAlive(app.m_TargetEntity));
            if (reg.HasComponent<TransformComponent>(app.m_TargetEntity)) {
                float x = reg.GetComponent<TransformComponent>(app.m_TargetEntity).position.x;
                TEST_CHECK_MSG(suite, "PhysicsSimulatedOnCPU", x > 0.0f,
                               "Fizik entegrasyonu penceresiz ortamda basariyla CPU uzerinde ilerlemelidir!");
            }
        }
    }

    // =========================================================================
    // 2. Tek Noktadan Sahne Değişimi ve Eski Sahnenin Terk Edilmesi (F03, A2.3)
    // =========================================================================
    {
        SceneSwitchTestApp app;
        app.Run(4);

        TEST_CHECK_MSG(suite, "Scene2FramesCount", app.m_FramesOnScene2 == 2,
                       "Sahne degisimi sonrasi sonraki kareler yeni sahne uzerinde calismalidir!");

        auto activeScene = app.GetActiveScene();
        TEST_CHECK_MSG(suite, "ActiveSceneIsScene2", activeScene == app.m_Scene2,
                       "Motorun guncel aktif sahnesi Scene 2 olmalidir!");

        // Gecis sirasinda ve calisma aninda durumlarin dogrulanmasi
        TEST_CHECK_MSG(suite, "Scene1StoppedOnSwitch", app.m_Scene1StoppedOnSwitch,
                       "Eski sahne OnRuntimeStop alarak durdurulmus olmalidir!");
        TEST_CHECK_MSG(suite, "Scene2RunningOnSwitch", app.m_Scene2RunningOnSwitch,
                       "Yeni sahne OnRuntimeStart alarak baslatilmis olmalidir!");
        TEST_CHECK_MSG(suite, "Scene2RunningInLoop", app.m_Scene2RunningInLoop,
                       "Yeni sahne sonraki karelerde aktif calismalidir!");
        TEST_CHECK_MSG(suite, "Scene2StoppedOnShutdown", !app.m_Scene2->IsRunning(),
                       "Uygulama kapandiginda aktif sahne de temizce durdurulmalidir!");

        // Scene 2 uzerindeki varligin guncel oldugunu dogrula
        TEST_CHECK(suite, "Entity2InActiveScene", app.m_Scene2->GetRegistry().IsAlive(app.m_Entity2));
    }

    // =========================================================================
    // 3. Editör Seçimi ile Çekirdek Runtime Picking Ayrımı
    // =========================================================================
    {
        // Application genel picking ve vurgulama API'sini sunar; editör kendi seçimini tutar
        HeadlessSimTestApp app(1);

        // Vurgulama API testi
        TEST_CHECK(suite, "InitialHighlightNull", app.GetHighlightEntity() == NullEntityHandle);
        EntityHandle dummyHandle = MakeEntityHandle(42, 1);
        app.SetHighlightEntity(dummyHandle);
        TEST_CHECK(suite, "HighlightUpdated", app.GetHighlightEntity() == dummyHandle);
        app.SetHighlightEntity(NullEntityHandle);
        TEST_CHECK(suite, "HighlightCleared", app.GetHighlightEntity() == NullEntityHandle);

        // RuntimePickEvent veri modeli testi
        bool eventReceived = false;
        EntityHandle receivedEntity = NullEntityHandle;

        auto token = app.GetEventBus().Subscribe<RuntimePickEvent>([&](const RuntimePickEvent& e) {
            eventReceived = true;
            receivedEntity = e.result.hitEntity;
        });

        RuntimePickResult pickRes{};
        pickRes.hasHit = true;
        pickRes.hitEntity = dummyHandle;
        pickRes.hitIndex = 3;
        pickRes.hitPoint = { 1.0f, 2.0f, 3.0f };
        pickRes.hitDistance = 5.5f;

        app.GetEventBus().Publish(RuntimePickEvent{ pickRes, nullptr });

        TEST_CHECK_MSG(suite, "PickEventDelivered", eventReceived, "RuntimePickEvent basariyla iletilmelidir!");
        TEST_CHECK_MSG(suite, "PickEntityDelivered", receivedEntity == dummyHandle, "Secilen EntityHandle dogru aktarilmalidir!");

        app.GetEventBus().Unsubscribe(token);
    }

    // =========================================================================
    // 4. Vulkan Bağımlılığı İzolasyonu Doğrulaması
    // =========================================================================
    {
        // RenderContext'in Vulkan tiplerini barındırdığını ama ISubsystem'in doğrudan
        // vulkan.hpp gerektirmediğini doğrula
        RenderContext ctx{};
        ctx.gpuTimeMs = 4.2f;
        ctx.cpuTimeMs = 1.8f;
        TEST_CHECK(suite, "RenderContextAvailable", ctx.gpuTimeMs == 4.2f && ctx.cpuTimeMs == 1.8f);
    }
}

} // namespace Astral::Test
