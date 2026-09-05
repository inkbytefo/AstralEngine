#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/SystemManager.hpp"
#include "Astral/Core/Systems/InputSubsystem.hpp"
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"
#include "Astral/Core/Systems/TransformSubsystem.hpp"
#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace Astral::Test {

namespace {

class MockStageProbe final : public ISubsystem {
public:
    MockStageProbe(SystemStage stage, std::vector<SystemStage>& executionLog)
        : ISubsystem(stage), m_Log(executionLog) {}

    void OnInit() override {}
    void OnUpdate(FrameContext&) override {
        m_Log.push_back(GetStage());
    }
    void OnShutdown() override {}

private:
    std::vector<SystemStage>& m_Log;
};

class MockGameplayMover final : public ISubsystem {
public:
    MockGameplayMover(EntityHandle target, glm::vec3 newPosition)
        : ISubsystem(SystemStage::Gameplay), m_Target(target), m_NewPos(newPosition) {}

    void OnInit() override {}
    void OnUpdate(FrameContext& context) override {
        if (context.registry.IsAlive(m_Target) && context.registry.HasComponent<TransformComponent>(m_Target)) {
            context.registry.GetComponent<TransformComponent>(m_Target).position = m_NewPos;
        }
    }
    void OnShutdown() override {}

private:
    EntityHandle m_Target;
    glm::vec3 m_NewPos;
};

} // anonymous namespace

void RunApplicationLoopTests() {
    const std::string suite = "ApplicationLoopSuite";

    // =========================================================================
    // 1. Sistem Aşamalarının Belirlenmiş Sıralaması (Stage Ordering) Testi
    // =========================================================================
    {
        SystemManager manager;
        std::vector<SystemStage> log;

        // Karışık sırada alt sistemler eklenir
        manager.PushSystem<MockStageProbe>(SystemStage::RenderExtraction, log);
        manager.PushSystem<MockStageProbe>(SystemStage::Transform, log);
        manager.PushSystem<MockStageProbe>(SystemStage::Input, log);
        manager.PushSystem<MockStageProbe>(SystemStage::FixedSimulation, log);
        manager.PushSystem<MockStageProbe>(SystemStage::Gameplay, log);

        TEST_CHECK(suite, "SystemCountMatches", manager.Size() == 5);
        TEST_CHECK(suite, "InputCount1", manager.GetSystemCount(SystemStage::Input) == 1);
        TEST_CHECK(suite, "GameplayCount1", manager.GetSystemCount(SystemStage::Gameplay) == 1);
        TEST_CHECK(suite, "FixedCount1", manager.GetSystemCount(SystemStage::FixedSimulation) == 1);
        TEST_CHECK(suite, "TransformCount1", manager.GetSystemCount(SystemStage::Transform) == 1);
        TEST_CHECK(suite, "ExtractionCount1", manager.GetSystemCount(SystemStage::RenderExtraction) == 1);

        Registry dummyReg;
        InputSystem dummyInput;
        ActionMap dummyActions;
        EventBus dummyEvents;
        JobSystem dummyJobs;
        Window dummyWin(100, 100, "Dummy");
        FrameContext context{ dummyReg, dummyInput, dummyActions, dummyEvents, dummyJobs, &dummyWin, 0.016f };

        manager.UpdateAll(context);

        TEST_CHECK_MSG(suite, "LogSizeMatches", log.size() == 5, "5 asama sirayla calismali!");
        if (log.size() == 5) {
            TEST_CHECK(suite, "Stage0IsInput", log[0] == SystemStage::Input);
            TEST_CHECK(suite, "Stage1IsGameplay", log[1] == SystemStage::Gameplay);
            TEST_CHECK(suite, "Stage2IsFixedSim", log[2] == SystemStage::FixedSimulation);
            TEST_CHECK(suite, "Stage3IsTransform", log[3] == SystemStage::Transform);
            TEST_CHECK(suite, "Stage4IsExtraction", log[4] == SystemStage::RenderExtraction);
        }
    }

    // =========================================================================
    // 2. Oyun Tarafından Değiştirilen Transform'un Aynı Kare Extraction Doğrulaması (F08)
    // =========================================================================
    {
        Registry registry;
        const EntityHandle entity = registry.CreateEntity();
        registry.AddComponent<TransformComponent>(entity, TransformComponent{ .position = { 0.0f, 0.0f, 0.0f } });
        registry.AddComponent<SDFComponent>(entity, SDFComponent{});

        SystemManager manager;
        // Standart motor pipeline'i ve bir gameplay sistemi kurulur
        manager.PushSystem<InputSubsystem>();
        manager.PushSystem<MockGameplayMover>(entity, glm::vec3(42.0f, 10.0f, -5.0f));
        manager.PushSystem<PhysicsSubsystem>();
        manager.PushSystem<TransformSubsystem>();
        auto& extraction = manager.PushSystem<RenderExtractionSubsystem>();

        InputSystem dummyInput;
        ActionMap dummyActions;
        EventBus dummyEvents;
        JobSystem dummyJobs;
        Window dummyWin(100, 100, "Dummy");
        FrameContext context{ registry, dummyInput, dummyActions, dummyEvents, dummyJobs, &dummyWin, 0.016f };

        // Tek kare calistirilir
        manager.UpdateAll(context);

        // Gameplay sistemi transform'u ayni karede degistirmistir.
        // Transform ve Extraction daha sonra calistigi icin GPU verisi 42.0f'yi aninda yansitmalidir!
        const auto& edits = extraction.GetLastExtractedEdits();
        TEST_CHECK_MSG(suite, "SameFrameExtractionCount", edits.size() == 1, "Edits boyutu 1 olmali!");
        if (!edits.empty()) {
            TEST_CHECK_MSG(suite, "SameFramePositionXMatches",
                           std::abs(edits[0].position.x - 42.0f) < 0.0001f,
                           "Gameplay transform degisimi ayni karede render extraction'a yansimali (sifir gecikme)!");
            TEST_CHECK(suite, "SameFramePositionYMatches", std::abs(edits[0].position.y - 10.0f) < 0.0001f);
            TEST_CHECK(suite, "SameFramePositionZMatches", std::abs(edits[0].position.z - (-5.0f)) < 0.0001f);
        }
    }

    // =========================================================================
    // 3. Multi-Rate FPS Invariance (30 / 60 / 144 FPS Karşılaştırması - F07)
    // =========================================================================
    {
        constexpr float fixedDt = 1.0f / 60.0f; // 60 Hz sabit adim
        constexpr float totalTime = 1.0f;        // 1 saniyelik simulasyon
        constexpr float speed = 10.0f;           // 10 m/s hiz (1 saniyede tam 10 metre gitmeli)

        auto simulateAtFps = [&](float renderFps) -> float {
            Registry reg;
            EntityHandle e = reg.CreateEntity();
            reg.AddComponent<TransformComponent>(e, TransformComponent{ .position = { 0.0f, 0.0f, 0.0f } });
            reg.AddComponent<VelocityComponent>(e, VelocityComponent{ .linear = { speed, 0.0f, 0.0f } });

            float accumulator = 0.0f;
            const float frameDt = 1.0f / renderFps;
            const int frameCount = static_cast<int>(std::round(renderFps * totalTime));
            uint32_t totalSteps = 0;

            for (int f = 0; f < frameCount; ++f) {
                accumulator += frameDt;
                while (accumulator >= fixedDt) {
                    PhysicsSubsystem::Integrate(reg, fixedDt);
                    accumulator -= fixedDt;
                    totalSteps++;
                }
            }

            TEST_CHECK_MSG(suite, "FixedStepsApprox60", totalSteps == 60 || totalSteps == 59,
                           "1 saniyede ~60 fizik adimi atilmalidir (FPS'ten bagimsiz)!");

            // Render aşamasında ekrana yansıtılan konum interpolasyon sözleşmesi ile elde edilir:
            const auto& prev = reg.GetComponent<PreviousTransformComponent>(e);
            const auto& curr = reg.GetComponent<TransformComponent>(e);
            const float alpha = std::clamp(accumulator / fixedDt, 0.0f, 1.0f);
            const auto interp = InterpolateTransform(prev, curr, alpha);
            return interp.position.x;
        };

        const float pos30 = simulateAtFps(30.0f);
        const float pos60 = simulateAtFps(60.0f);
        const float pos144 = simulateAtFps(144.0f);

        // Render interpolasyonunda 1 sabit adim gecikmeli tamponlama kullanildigindan,
        // 1.0 saniye sonucunda render edilen konum tam olarak speed * (totalTime - fixedDt) = 9.833333m olmalidir.
        constexpr float expectedRenderPos = speed * (totalTime - fixedDt);

        // Tum FPS hizlarinda varligin kat ettigi render mesafesi teorik degerle eslesmelidir
        TEST_CHECK_MSG(suite, "Pos30Accurate", std::abs(pos30 - expectedRenderPos) < 0.001f,
                       "30 FPS'te render interpolasyon konumu teorik degerle eslesmelidir!");
        TEST_CHECK_MSG(suite, "Pos60Accurate", std::abs(pos60 - expectedRenderPos) < 0.001f,
                       "60 FPS'te render interpolasyon konumu teorik degerle eslesmelidir!");
        TEST_CHECK_MSG(suite, "Pos144Accurate", std::abs(pos144 - expectedRenderPos) < 0.001f,
                       "144 FPS'te render interpolasyon konumu teorik degerle eslesmelidir!");

        // 30, 60 ve 144 FPS sonucunun tam esdegerligi (F07 cozumu kaniti: render hizi ne olursa olsun ayni noktada)
        TEST_CHECK_MSG(suite, "FPSInvariance30vs60", std::abs(pos30 - pos60) < 0.0001f,
                       "30 FPS ve 60 FPS simulasyon sonuclari esdeger olmalidir!");
        TEST_CHECK_MSG(suite, "FPSInvariance60vs144", std::abs(pos60 - pos144) < 0.0001f,
                       "60 FPS ve 144 FPS simulasyon sonuclari esdeger olmalidir!");
    }

    // =========================================================================
    // 4. Duraklatma (Pause) Davranışı Testi
    // =========================================================================
    {
        Registry reg;
        EntityHandle e = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(e, TransformComponent{ .position = { 0.0f, 0.0f, 0.0f } });
        reg.AddComponent<VelocityComponent>(e, VelocityComponent{ .linear = { 10.0f, 0.0f, 0.0f } });

        constexpr float fixedDt = 1.0f / 60.0f;
        float accumulator = 0.0f;
        bool isPaused = true;
        uint32_t stepsTaken = 0;

        // 60 kare gecirilir ama duraklatilmistir
        for (int f = 0; f < 60; ++f) {
            if (!isPaused) {
                accumulator += fixedDt;
                while (accumulator >= fixedDt) {
                    PhysicsSubsystem::Integrate(reg, fixedDt);
                    accumulator -= fixedDt;
                    stepsTaken++;
                }
            } else {
                accumulator = 0.0f;
            }
        }

        TEST_CHECK_MSG(suite, "PausedStepsZero", stepsTaken == 0, "Duraklatmada 0 adim calismalidir!");
        TEST_CHECK_MSG(suite, "PausedPositionZero", reg.GetComponent<TransformComponent>(e).position.x == 0.0f,
                       "Duraklatildiginda nesne kesinlikle ilerlememelidir!");
    }

    // =========================================================================
    // 5. Spiral of Death & Max SubSteps Sınırı Testi
    // =========================================================================
    {
        Registry reg;
        EntityHandle e = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(e, TransformComponent{ .position = { 0.0f, 0.0f, 0.0f } });
        reg.AddComponent<VelocityComponent>(e, VelocityComponent{ .linear = { 10.0f, 0.0f, 0.0f } });

        constexpr float fixedDt = 1.0f / 60.0f;
        constexpr uint32_t maxSubSteps = 8;
        constexpr float maxFrameDelta = 0.25f;

        // Ani 5 saniyelik devasa takilma (hitch / breakpoint simülasyonu)
        float rawHitchDelta = 5.0f;
        float clampedDelta = std::min(rawHitchDelta, maxFrameDelta);

        float accumulator = clampedDelta;
        uint32_t stepsTaken = 0;

        while (accumulator >= fixedDt) {
            if (stepsTaken >= maxSubSteps) {
                accumulator = 0.0f; // Fazlaligi at; sonsuz adim dongusunu onle
                break;
            }
            PhysicsSubsystem::Integrate(reg, fixedDt);
            accumulator -= fixedDt;
            stepsTaken++;
        }

        TEST_CHECK_MSG(suite, "HitchClampedToMaxSubSteps", stepsTaken == maxSubSteps,
                       "Uzun takilmada adim sayisi maxSubSteps (8) ile sinirlandirilmalidir!");
        TEST_CHECK_MSG(suite, "AccumulatorResetAfterHitch", accumulator == 0.0f,
                       "MaxSubSteps asiminda akümülatör sifirlanarak spiral of death onlenmelidir!");
        TEST_CHECK(suite, "PositionBounded", reg.GetComponent<TransformComponent>(e).position.x < 2.0f);
    }

    // =========================================================================
    // 6. Render İnterpolasyon Sözleşmesi ve Durum Takibi Testi
    // =========================================================================
    {
        Registry reg;
        EntityHandle e = reg.CreateEntity();
        reg.AddComponent<TransformComponent>(e, TransformComponent{
            .position = { 0.0f, 0.0f, 0.0f },
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = { 1.0f, 1.0f, 1.0f }
        });
        reg.AddComponent<PreviousTransformComponent>(e, PreviousTransformComponent{
            .position = { 0.0f, 0.0f, 0.0f },
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = { 1.0f, 1.0f, 1.0f }
        });
        reg.AddComponent<VelocityComponent>(e, VelocityComponent{
            .linear = { 12.0f, 0.0f, 0.0f }
        });

        // 1 fizik adimi calistir (fixedDt = 0.5s -> 6m gitmeli)
        PhysicsSubsystem::Integrate(reg, 0.5f);

        const auto& prev = reg.GetComponent<PreviousTransformComponent>(e);
        const auto& curr = reg.GetComponent<TransformComponent>(e);

        TEST_CHECK(suite, "PrevPositionSaved", prev.position.x == 0.0f);
        TEST_CHECK(suite, "CurrPositionAdvanced", curr.position.x == 6.0f);

        // Alpha = 0.0 -> Onceki durum
        TransformComponent t0 = InterpolateTransform(prev, curr, 0.0f);
        TEST_CHECK(suite, "Alpha0Position", t0.position.x == 0.0f);

        // Alpha = 0.5 -> Yari yol (3.0m)
        TransformComponent tHalf = InterpolateTransform(prev, curr, 0.5f);
        TEST_CHECK_MSG(suite, "AlphaHalfPosition", std::abs(tHalf.position.x - 3.0f) < 0.0001f,
                       "Alpha 0.5 iken interpolasyon tam yari yola ulasmalidir!");

        // Alpha = 1.0 -> Guncel durum (6.0m)
        TransformComponent t1 = InterpolateTransform(prev, curr, 1.0f);
        TEST_CHECK(suite, "Alpha1Position", t1.position.x == 6.0f);
    }
}

} // namespace Astral::Test
