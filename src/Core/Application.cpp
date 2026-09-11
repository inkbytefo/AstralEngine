#include "Astral/Core/Application.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Core/BenchmarkLogger.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/Swapchain.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/RenderContext.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Systems/InputSubsystem.hpp"
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"
#include "Astral/Core/Systems/TransformSubsystem.hpp"
#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"
#include "Astral/Core/Events/EngineEvents.hpp"
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <chrono>
#include <filesystem>
#include <array>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Astral {

Application::Application()
    : Application(AppConfig{}) {}

Application::Application(const AppConfig& config)
    : m_Config(config) {
    m_PhysicsSubsystem = &PushSystem<PhysicsSubsystem>();
    m_RenderExtractionSubsystem = &PushSystem<RenderExtractionSubsystem>();
    PushSystem<TransformSubsystem>();
    PushSystem<InputSubsystem>();
}

std::shared_ptr<Scene> Application::CreateInitialScene() {
    return std::make_shared<Scene>();
}

void Application::RequestPick(int screenX, int screenY) {
    if (m_SDFRenderer) {
        auto activeScene = m_SceneManager.GetActiveScene();
        uint64_t sceneInstance = activeScene ? activeScene->GetInstanceId() : 0;
        if (m_SDFRenderer->GetPickingReadback()) {
            m_SDFRenderer->GetPickingReadback()->RequestPick(screenX, screenY, sceneInstance);
        } else {
            m_SDFRenderer->SetPickingRequest(screenX, screenY);
        }
    }
}

void Application::Cleanup() {
    m_Running = false;
    m_SystemManager.ShutdownAll();
    m_JobSystem.Shutdown();
    if (m_VulkanContext) {
        m_VulkanContext->WaitIdle();
    }
    m_SceneManager.UnloadCurrentScene();
    if (m_SDFRenderer) {
        m_SDFRenderer.reset();
    }
    if (m_VulkanContext) {
        m_VulkanContext.reset();
    }
    m_Window.reset();
}

Application::~Application() {
    Cleanup();
    std::cout << "[Astral::Application] " << GetName() << " basariyla kapatildi.\n";
}

void Application::Run(int maxFrames) {
    std::cout << "[Astral::Application] Baslatiliyor...\n";

    try {
        if (!m_Config.headless) {
            // 1. Pencereyi baslat
            std::string modeTitle = (m_Config.normalMode == 1) ? "[Tetrahedron Normal (4-tap)]" : "[Central Normal (6-tap)]";
            m_Window = std::make_unique<Window>(
                m_Config.width,
                m_Config.height,
                GetName() + " - Vulkan 1.4 SDF Compute " + modeTitle
            );

            // 2. Vulkan 1.4 Context ve Validation Layer'i baslat
            m_VulkanContext = std::make_unique<VulkanContext>(*m_Window, true);

            // 3. SPIR-V dosyasini bul ve SDFRenderer'i baslat
            std::string spvPath = m_Config.shaderPath;
            if (spvPath.empty()) {
                std::vector<std::string> candidates = {
                    "build/shaders/SDFGBuffer.spv",
                    "shaders/SDFGBuffer.spv",
                    "../shaders/SDFGBuffer.spv"
                };
                if (g_CommandLineArgs.argv && g_CommandLineArgs.argv[0]) {
                    std::filesystem::path exeDir = std::filesystem::absolute(g_CommandLineArgs.argv[0]).parent_path();
                    candidates.push_back((exeDir / "shaders/SDFGBuffer.spv").string());
                    candidates.push_back((exeDir / "../shaders/SDFGBuffer.spv").string());
                    candidates.push_back((exeDir / "SDFGBuffer.spv").string());
                }
#ifdef SHADER_BIN_DIR
                candidates.insert(candidates.begin(), SHADER_BIN_DIR "/SDFGBuffer.spv");
#endif
                for (const auto& c : candidates) {
                    if (std::filesystem::exists(c)) {
                        spvPath = c;
                        break;
                    }
                }
            }

            if (spvPath.empty() || !std::filesystem::exists(spvPath)) {
                throw std::runtime_error("SDFGBuffer.spv shader dosyasi bulunamadi! spvPath: " + spvPath);
            }

            m_SDFRenderer = std::make_unique<SDFRenderer>(
                *m_VulkanContext,
                spvPath,
                m_Config.width,
                m_Config.height,
                !m_Config.legacyMap
            );
            m_SDFRenderer->SetUseGBuffer(true);
            m_SDFRenderer->SetDebugMode(m_Config.debugMode);
            m_SDFRenderer->SetQualitySettings(m_Config.qualitySettings);
        } else {
            std::cout << "[Astral::Application] GPU'suz Headless CPU simulasyon modu aktif (Pencere ve Vulkan olusturulmadi).\n";
        }

        // 4. Benchmark logger kurulumu
        if (m_Config.benchMode) {
            m_BenchmarkLogger = std::make_unique<BenchmarkLogger>();
        }

        int targetFrames = maxFrames > 0 ? maxFrames : (m_Config.maxFrames > 0 ? m_Config.maxFrames : (m_Config.benchMode ? m_Config.benchFrames : -1));

        m_Running = true;
        // 5. Baslatma bilgisi
        if (m_Window) {
            std::cout << "[Astral::Application] Normal Modu: " << (m_Config.normalMode == 1 ? "Tetrahedron (4-tap optimize)" : "Central Differences (6-tap)") << "\n";
            std::cout << "[Astral::Application] Bellek Esleme Modu: " << (m_Config.legacyMap ? "Legacy Map/Unmap (Kare basi vkMapMemory)" : "Persistent Mapping (Kalici Pointer)") << "\n";
            std::cout << "[Astral::Application] Izgara Hizlandirmasi (PR-6): " << (m_Config.useGrid ? "AKTIF (Empty Space Skipping)" : "KAPALI (Brute Force)") << "\n";
            std::cout << "[Astral::Application] Golge Optimizasyonu (PR-7): " << (m_Config.optShadow ? "AKTIF (Erken Cikis & Back-Face Culling)" : "KAPALI (Kaba Kuvvet 24-Adim)") << "\n";
            std::cout << "[Astral::Application] Temporal Anti-Aliasing (PR-8): " << (m_Config.enableTAA ? "AKTIF (Halton Jitter + 3x3 Clamp TAA)" : "KAPALI (Ham No-AA)") << "\n";
            std::cout << "[Astral::Application] Pure Deferred Architecture: AKTIF (G-Buffer + PBR Deferred Lighting + Motion Vectors + TAA)\n";
            if (m_Config.debugMode != 0) {
                std::cout << "[Astral::Application] G-Buffer Debug Modu: " << m_Config.debugMode << "\n";
            }
        }
        if (targetFrames > 0) {
            std::cout << "[Astral::Application] Hedef: " << targetFrames << " kare calisip sonlanacak...\n";
        } else {
            std::cout << "[Astral::Application] Ana olay dongusune giriliyor (Cikmak icin pencereyi kapatin)...\n";
        }

        m_SceneManager.SetRuntimeActive(true);
        auto initialScene = CreateInitialScene();
        if (!initialScene) throw std::runtime_error("CreateInitialScene returned null");
        m_SceneManager.SetActiveScene(initialScene);
        m_EventBus.Publish(SceneLoadedEvent{ initialScene->GetName(), initialScene.get() });

        uint32_t frameIndex = 0;
        double cpuFrameMs = 0.0;
        double gpuTotalMs = 0.0;
        auto previousFrameTime = std::chrono::high_resolution_clock::now();

        m_JobSystem.Initialize();
        OnInitialize();
        m_PhysicsSubsystem->SetEnabled(m_Config.simulatePhysics);
        m_SystemManager.InitAll();

        InputSystem& activeInput = m_Window ? m_Window->GetInputSystem() : m_HeadlessInput;
        Window* activeWindow = m_Window.get();

        // 6. Ana render, UI ve profil dongusu
        while (m_Running && (m_Config.headless || (m_Window && !m_Window->ShouldClose()))) {
            auto cpuStart = std::chrono::high_resolution_clock::now();

            if (m_Window) {
                m_Window->PollEvents();
            }

            // Tek noktadan sahne yonetimi: Her karenin basinda guncel aktif sahneyi al
            auto activeScene = m_SceneManager.GetActiveScene();
            if (!activeScene) {
                if (targetFrames > 0 && static_cast<int>(frameIndex) >= targetFrames) break;
                frameIndex++;
                m_TotalFramesRendered = frameIndex;
                continue;
            }
            Registry& sceneRegistry = activeScene->GetRegistry();

            const float rawDeltaTime = (m_Config.fixedDeltaTime > 0.0f)
                ? m_Config.fixedDeltaTime
                : std::chrono::duration<float>(cpuStart - previousFrameTime).count();
            previousFrameTime = cpuStart;

            // Spiral of death onleyici: Uzun takilmalarda (hitch/breakpoint) delta time sinirlandirilir
            const float clampedDeltaTime = std::min(rawDeltaTime, m_Config.maxFrameDelta);
            const float effectiveDeltaTime = m_Config.isPaused ? 0.0f : clampedDeltaTime;
            [[maybe_unused]] float timeSec = static_cast<float>(frameIndex) * m_Config.fixedTimeStep;

            // 1. Asama: Input
            FrameContext inputContext{
                sceneRegistry,
                activeInput,
                m_ActionMap,
                m_EventBus,
                m_JobSystem,
                activeWindow,
                clampedDeltaTime
            };
            m_SystemManager.UpdateStage(SystemStage::Input, inputContext);

            // 2. Asama: Gameplay (Degisken delta time; istemci OnUpdate ve gameplay sistemleri)
            FrameContext gameplayContext{
                sceneRegistry,
                activeInput,
                m_ActionMap,
                m_EventBus,
                m_JobSystem,
                activeWindow,
                effectiveDeltaTime
            };
            OnUpdate(gameplayContext, frameIndex);
            m_SystemManager.UpdateStage(SystemStage::Gameplay, gameplayContext);

            // 3. Asama: Fixed Simulation (Fizik ve sabit adimli sistemler - F07)
            if (!m_Config.isPaused && m_Config.simulatePhysics) {
                m_Accumulator += clampedDeltaTime;
                uint32_t stepsTaken = 0;
                const float fixedDt = m_Config.fixedTimeStep;

                while (m_Accumulator >= fixedDt) {
                    if (stepsTaken >= m_Config.maxSubSteps) {
                        // Fazla birikmis zamani temizle; sonsuz catch-up dongusunu (spiral of death) engelle
                        m_Accumulator = 0.0f;
                        break;
                    }

                    FrameContext fixedContext{
                        sceneRegistry,
                        activeInput,
                        m_ActionMap,
                        m_EventBus,
                        m_JobSystem,
                        activeWindow,
                        fixedDt
                    };
                    m_SystemManager.UpdateStage(SystemStage::FixedSimulation, fixedContext);

                    m_Accumulator -= fixedDt;
                    stepsTaken++;
                }
            } else {
                // Duraklatma veya fizik kapali durumunda akümülatör sifirlanir
                m_Accumulator = 0.0f;
            }

            m_InterpolationAlpha = (m_Config.fixedTimeStep > 0.0f)
                ? std::clamp(m_Accumulator / m_Config.fixedTimeStep, 0.0f, 1.0f)
                : 1.0f;

            // 4. Asama: Transform (Gameplay ve Physics sonrasi World Matrix guncellemesi)
            FrameContext transformContext{
                sceneRegistry,
                activeInput,
                m_ActionMap,
                m_EventBus,
                m_JobSystem,
                activeWindow,
                effectiveDeltaTime
            };
            m_SystemManager.UpdateStage(SystemStage::Transform, transformContext);

            // 5. Asama: Render Extraction (World Matrix hesaplandiktan sonra ayni karede GPU extraction - F08)
            m_SystemManager.UpdateStage(SystemStage::RenderExtraction, transformContext);

            const auto& snapshot = m_RenderExtractionSubsystem->GetLastExtractedSnapshot();
            const auto& sceneEntities = m_RenderExtractionSubsystem->GetLastExtractedEntities();

            // G08: İstek anındaki sahne kimliğini ve entity snapshot sırasını kaydet
            if (m_SDFRenderer && m_SDFRenderer->GetPickingReadback() &&
                m_SDFRenderer->GetPickingReadback()->GetState() == PickingState::Requested) {
                const auto& activeReq = m_SDFRenderer->GetPickingReadback()->GetActiveRequest();
                if (activeReq.has_value()) {
                    m_PendingPickRequest = PendingPickRequest{
                        .requestId = activeReq->requestId,
                        .sceneInstance = activeReq->sceneInstance,
                        .entities = sceneEntities
                    };
                }
            }

            // Yalnizca GPU ve Renderer aktif ise render islemleri calistirilir
            if (m_SDFRenderer && m_VulkanContext) {
                // Kamera matrislerini besle (Motion Vectors, Deferred G-Buffer & SDFChangeSet senkronizasyonu)
                const float aspect = static_cast<float>(m_SDFRenderer->GetWidth()) / m_SDFRenderer->GetHeight();
                auto camera = ExtractActiveCamera(sceneRegistry, aspect);
                if (camera) camera->sceneInstance = activeScene->GetInstanceId();
                // The table is in [-1, 1]; ray offsets are measured in pixels.
                // Keep the subpixel sequence within [-0.5, 0.5].
                glm::vec2 jitter = m_Config.enableTAA ? HALTON_SEQUENCE_8[frameIndex % 8] * 0.5f : glm::vec2(0.0f);
                m_SDFRenderer->SetCamera(camera, jitter);

                // GPU SSBO'ya yaz ve Two-Level Grid'i guncelle (ChangeSet guncel kamera ile uretilir)
                m_SDFRenderer->UpdateEdits(snapshot, m_Config.legacyMap);

                // Swapchain resmi edin
                bool hasSwapchainImage = false;
                if (!m_Config.benchMode && targetFrames < 0 && m_VulkanContext->GetSwapchain()) {
                    hasSwapchainImage = m_VulkanContext->AcquireNextImage();
                }

                // Vurgulanacak varligin render edit indeksini bul ve shader Fresnel Rim-Light icin ayarla
                int selectedHitIndex = -1;
                if (m_HighlightEntity != NullEntityHandle) {
                    for (size_t i = 0; i < sceneEntities.size(); ++i) {
                        if (sceneEntities[i] == m_HighlightEntity) {
                            selectedHitIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
                m_SDFRenderer->SetSelectedHitIndex(selectedHitIndex);

                // GPU Komut Tamponu & Timestamp Olcumu
                auto cmd = m_VulkanContext->BeginFrameCommand();

                bool submitted = false;
                try {
                    // 1. 3D SDF Compute Raymarching (G12: Acik RenderFrameSettings yapisi ile tek kalite politikasi)
                    RenderFrameSettings frameSettings{};
                    frameSettings.camera = camera;
                    frameSettings.jitter = jitter;
                    frameSettings.qualityOverride = m_Config.qualitySettings;
                    frameSettings.frameId = frameIndex;
                    frameSettings.width = static_cast<uint32_t>(m_SDFRenderer->GetWidth());
                    frameSettings.height = static_cast<uint32_t>(m_SDFRenderer->GetHeight());
                    frameSettings.normalMode = m_Config.normalMode;
                    frameSettings.debugMode = m_SDFRenderer->GetDebugMode();
                    frameSettings.selectedHitIndex = selectedHitIndex;
                    frameSettings.useGrid = m_Config.useGrid;
                    frameSettings.optimizedShadows = m_Config.optShadow;
                    frameSettings.taaEnabled = m_Config.enableTAA;
                    frameSettings.exposure = m_Config.qualitySettings.tonemapExposure;

                    m_SDFRenderer->Render(cmd, frameSettings);

                    if (hasSwapchainImage) {
                        if (m_SystemManager.HasRenderSubsystem()) {
                            m_VulkanContext->PrepareSwapchainImage();
                        } else {
                            m_VulkanContext->EndFrameBlit(
                                m_SDFRenderer->GetStorageImage(),
                                m_SDFRenderer->GetWidth(),
                                m_SDFRenderer->GetHeight()
                            );
                        }

                        RenderContext renderCtx{
                            cmd,
                            m_VulkanContext->GetSwapchain()->GetImageViews()[m_VulkanContext->GetCurrentImageIndex()],
                            m_VulkanContext->GetSwapchain()->GetExtent(),
                            activeScene.get(),
                            static_cast<float>(gpuTotalMs),
                            static_cast<float>(cpuFrameMs),
                            m_Config.qualitySettings
                        };
                        m_SystemManager.RenderAll(renderCtx);

                        submitted = m_VulkanContext->EndFramePresent();
                    } else {
                        submitted = m_VulkanContext->EndAndSubmitFrameCommand();
                    }
                } catch (const vk::DeviceLostError& e) {
                    std::cerr << "[Astral::Application] GPU Device Lost: " << e.what() << "\n";
                    m_SDFRenderer->AbortPreparedFrame();
                    m_Running = false;
                    break;
                } catch (const std::exception& e) {
                    std::cerr << "[Astral::Application] Frame recording/submission error: " << e.what() << "\n";
                    m_SDFRenderer->AbortPreparedFrame();
                }

                // G11: Submit basarili ise committed duruma gecir; basarisiz/iptal ise abort yap
                if (submitted) {
                    m_SDFRenderer->CommitSubmittedFrame();
                } else {
                    m_SDFRenderer->AbortPreparedFrame();
                    if (m_VulkanContext->IsDeviceLost()) {
                        std::cerr << "[Astral::Application] Stopping main loop due to device lost.\n";
                        m_Running = false;
                        break;
                    }
                }

                // G08: Fence sonrasi donanımsal guvenli secim okumasi (tek seferlik tuketim) ve dogru snapshot eslemesi
                if (m_SDFRenderer && m_SDFRenderer->HasPendingSelection()) {
                    auto completedPick = m_SDFRenderer->ConsumeCompletedPick();
                    if (completedPick.has_value()) {
                        const auto& pick = *completedPick;
                        uint64_t currentSceneInstance = activeScene ? activeScene->GetInstanceId() : 0;
                        if (pick.sceneInstance == currentSceneInstance) {
                            RuntimePickResult runtimeResult{};
                            runtimeResult.hasHit = (pick.data.hitIndex >= 0);
                            runtimeResult.hitIndex = pick.data.hitIndex;
                            runtimeResult.hitPoint = glm::vec3(pick.data.hitPoint.x, pick.data.hitPoint.y, pick.data.hitPoint.z);
                            runtimeResult.hitDistance = pick.data.hitPoint.w;

                            // Snapshot sirasiyla gecikmis hitIndex cozümleme
                            const std::vector<EntityHandle>* pEntities = &sceneEntities;
                            if (m_PendingPickRequest.has_value() &&
                                m_PendingPickRequest->requestId == pick.requestId &&
                                m_PendingPickRequest->sceneInstance == currentSceneInstance) {
                                pEntities = &m_PendingPickRequest->entities;
                            }

                            if (runtimeResult.hasHit && runtimeResult.hitIndex >= 0 &&
                                static_cast<size_t>(runtimeResult.hitIndex) < pEntities->size()) {
                                runtimeResult.hitEntity = (*pEntities)[runtimeResult.hitIndex];
                            }

                            m_LastPickResult = runtimeResult;
                            m_EventBus.Publish(RuntimePickEvent{ runtimeResult, activeScene.get() });

                            if (runtimeResult.hasHit) {
                                std::cout << "[Astral::Picking] ISABET: hitIndex = " << runtimeResult.hitIndex 
                                          << " -> Entity (handle: " << GetEntityIndex(runtimeResult.hitEntity) << ")"
                                          << " | Nokta: (" << runtimeResult.hitPoint.x << ", " << runtimeResult.hitPoint.y << ", " << runtimeResult.hitPoint.z << ")"
                                          << " | Mesafe: " << runtimeResult.hitDistance << "m\n";
                            } else {
                                std::cout << "[Astral::Picking] ISABET YOK (Gokyuzu/Bosluk)\n";
                            }
                        } else {
                            std::cout << "[Astral::Picking] Sahne degisimi nedeniyle eski sonuc yoksayildi (Req Scene: "
                                      << pick.sceneInstance << " != Current Scene: " << currentSceneInstance << ")\n";
                        }
                        m_PendingPickRequest = std::nullopt;
                    }
                }

                gpuTotalMs = m_VulkanContext->GetLastGpuTimeMs();
            }

            auto cpuEnd = std::chrono::high_resolution_clock::now();
            cpuFrameMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();

            frameIndex++;
            m_TotalFramesRendered = frameIndex;

            if (m_BenchmarkLogger && m_Window && m_VulkanContext) {
                FrameMetric metric{};
                metric.frameIndex = frameIndex;
                metric.cpuFrameMs = cpuFrameMs;
                metric.gpuTotalMs = gpuTotalMs;
                metric.gpuComputeMs = gpuTotalMs;
                metric.gpuRenderMs = 0.0;
                metric.screenW = m_Window->GetWidth();
                metric.screenH = m_Window->GetHeight();
                metric.gpuName = m_VulkanContext->GetDeviceName();
                metric.driverVersion = m_VulkanContext->GetDriverVersionString();

                m_BenchmarkLogger->LogFrame(metric);
            }

            if (targetFrames > 0 && static_cast<int>(frameIndex) >= targetFrames) {
                break;
            }
        }

        m_SceneManager.SetRuntimeActive(false);
        m_SystemManager.ShutdownAll();
        m_JobSystem.Shutdown();
        m_Running = false;
        std::cout << "[Astral::Application] Ana olay dongusu sonlandi (Toplam " << frameIndex << " kare).\n";

        // 6. Benchmark Sonuclari ve Raporlama
        if (m_BenchmarkLogger && m_BenchmarkLogger->GetFrameCount() > 0) {
            m_BenchmarkLogger->PrintSummary(30);

            if (!m_Config.benchOutputFile.empty()) {
                m_BenchmarkLogger->WriteCSV(m_Config.benchOutputFile);

                std::string jsonPath = m_Config.benchOutputFile;
                size_t extPos = jsonPath.rfind(".csv");
                if (extPos != std::string::npos) {
                    jsonPath.replace(extPos, 4, ".json");
                } else {
                    jsonPath += ".json";
                }
                m_BenchmarkLogger->WriteSummaryJSON(jsonPath, 30);
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[Astral::Application Kritik Hata]: " << e.what() << "\n";
        Cleanup();
        throw;
    } catch (...) {
        std::cerr << "[Astral::Application Kritik Hata]: Bilinmeyen kritik istisna yakalandi!\n";
        Cleanup();
        throw;
    }
}

void Application::SetDebugMode(int mode) noexcept {
    m_Config.debugMode = mode;
    m_Config.qualitySettings.debugMode = mode;
    if (m_SDFRenderer) {
        m_SDFRenderer->SetDebugMode(mode);
    }
}

void Application::SetQualitySettings(const QualitySettings& qs) noexcept {
    m_Config.qualitySettings = qs;
    if (m_SDFRenderer) {
        m_SDFRenderer->SetQualitySettings(qs);
    }
}

std::string Application::GetName() {
    return "AstralEngine";
}

std::string Application::GetVersion() {
    return "1.0.0";
}

} // namespace Astral
