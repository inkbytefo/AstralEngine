#include "Astral/Core/Application.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Core/BenchmarkLogger.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/Swapchain.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Editor/EditorUI.hpp"
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <chrono>
#include <filesystem>

namespace Astral {

Application::Application()
    : Application(AppConfig{}) {}

Application::Application(const AppConfig& config)
    : m_Config(config) {
    std::cout << "[Astral::Application] " << GetName() << " v" << GetVersion() << " olusturuldu.\n";
}

Application::~Application() {
    if (m_VulkanContext) {
        m_VulkanContext->WaitIdle();
    }
    m_SceneManager.UnloadCurrentScene();
    if (m_EditorUI) {
        m_EditorUI.reset();
    }
    if (m_SDFRenderer) {
        m_SDFRenderer.reset();
    }
    if (m_VulkanContext) {
        m_VulkanContext.reset();
    }
    m_Window.reset();
    std::cout << "[Astral::Application] " << GetName() << " basariyla kapatildi.\n";
}

void Application::Run(int maxFrames) {
    std::cout << "[Astral::Application] Baslatiliyor...\n";

    try {
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
                "build/shaders/SDFCompute.spv",
                "shaders/SDFCompute.spv",
                "../shaders/SDFCompute.spv"
            };
#ifdef SHADER_BIN_DIR
            candidates.insert(candidates.begin(), SHADER_BIN_DIR "/SDFCompute.spv");
#endif
            for (const auto& c : candidates) {
                if (std::filesystem::exists(c)) {
                    spvPath = c;
                    break;
                }
            }
        }

        if (spvPath.empty() || !std::filesystem::exists(spvPath)) {
            throw std::runtime_error("SDFCompute.spv shader dosyasi bulunamadi! spvPath: " + spvPath);
        }

        m_SDFRenderer = std::make_unique<SDFRenderer>(
            *m_VulkanContext,
            spvPath,
            m_Config.width,
            m_Config.height,
            !m_Config.legacyMap
        );

        // 4. Editör UI Arayuzu Kurulumu
        m_EditorUI = std::make_unique<EditorUI>(
            *m_VulkanContext,
            m_Window->GetNativeWindow(),
            m_Window->GetInputSystem()
        );
        m_EditorUI->SetRenderer(m_SDFRenderer.get());

        // 5. Benchmark logger kurulumu
        if (m_Config.benchMode) {
            m_BenchmarkLogger = std::make_unique<BenchmarkLogger>();
        }

        int targetFrames = maxFrames > 0 ? maxFrames : (m_Config.benchMode ? m_Config.benchFrames : -1);

        m_Running = true;
        std::cout << "[Astral::Application] Normal Modu: " << (m_Config.normalMode == 1 ? "Tetrahedron (4-tap optimize)" : "Central Differences (6-tap)") << "\n";
        std::cout << "[Astral::Application] Bellek Esleme Modu: " << (m_Config.legacyMap ? "Legacy Map/Unmap (Kare basi vkMapMemory)" : "Persistent Mapping (Kalici Pointer)") << "\n";
        std::cout << "[Astral::Application] Izgara Hizlandirmasi (PR-6): " << (m_Config.useGrid ? "AKTIF (Empty Space Skipping)" : "KAPALI (Brute Force)") << "\n";
        std::cout << "[Astral::Application] Golge Optimizasyonu (PR-7): " << (m_Config.optShadow ? "AKTIF (Erken Cikis & Back-Face Culling)" : "KAPALI (Kaba Kuvvet 24-Adim)") << "\n";
        std::cout << "[Astral::Application] Temporal Anti-Aliasing (PR-8): " << (m_Config.enableTAA ? "AKTIF (Halton Jitter + 3x3 Clamp TAA)" : "KAPALI (Ham No-AA)") << "\n";
        std::cout << "[Astral::Application] Sahne Modu: " << (m_Config.stressTest ? "Karmasik Stress Sahnesi (32 Nesne)" : "Standart Sahne (4 Nesne)") << "\n";
        if (targetFrames > 0) {
            std::cout << "[Astral::Application] Hedef: " << targetFrames << " kare calisip sonlanacak...\n";
        } else {
            std::cout << "[Astral::Application] Ana olay dongusune giriliyor (Cikmak icin pencereyi kapatin)...\n";
        }

        // 1. Authoring (Editor) Sahnesi olustur
        auto editorScene = std::make_shared<Scene>("Sandbox Editor Level");

        // Obje 0: Zemin Entity
        Entity ground = editorScene->CreateEntity();
        ground.AddComponent<TransformComponent>(glm::vec3(0.0f, -1.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        ground.AddComponent<SDFComponent>(
            static_cast<uint32_t>(PrimitiveType::Plane),
            static_cast<uint32_t>(CSGOperation::Union),
            0.0f, 0u, glm::vec3(0.3f, 0.32f, 0.35f), 0.8f, 0.05f
        );

        Entity boxEntity;
        Entity torusEntity;

        if (m_Config.stressTest) {
            for (int i = 0; i < 31; ++i) {
                Entity obj = editorScene->CreateEntity();
                float angle = static_cast<float>(i) * (2.0f * 3.14159f / 31.0f);
                float radius = 3.0f + static_cast<float>(i % 3) * 2.5f;
                float heightY = 0.5f + static_cast<float>(i % 4) * 1.0f;
                glm::vec3 pos = glm::vec3(std::cos(angle) * radius, heightY, std::sin(angle) * radius);

                uint32_t type = i % 3; // 0=Sphere, 1=Box, 2=Torus
                glm::vec3 scale{1.0f};
                glm::vec3 albedo{1.0f};
                if (type == 0) {
                    scale = glm::vec3(0.5f + (i % 2) * 0.2f);
                    albedo = glm::vec3(0.85f, 0.2f + (i % 5) * 0.15f, 0.25f);
                } else if (type == 1) {
                    scale = glm::vec3(0.45f + (i % 3) * 0.1f);
                    albedo = glm::vec3(0.2f, 0.5f + (i % 4) * 0.1f, 0.9f);
                } else {
                    scale = glm::vec3(0.6f, 0.2f, 1.0f);
                    albedo = glm::vec3(0.9f, 0.8f, 0.2f);
                }

                obj.AddComponent<TransformComponent>(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale);
                obj.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.0f, 0.2f, 0.0f));
                obj.AddComponent<SDFComponent>(
                    type,
                    static_cast<uint32_t>(CSGOperation::SmoothUnion),
                    0.2f, 1u, albedo, 0.3f, 0.5f
                );
            }
        } else {
            // Obje 1: Kutu
            boxEntity = editorScene->CreateEntity();
            boxEntity.AddComponent<TransformComponent>(glm::vec3(-1.8f, 0.2f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.6f));
            boxEntity.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.0f, 0.5f, 0.0f));
            boxEntity.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Box),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.3f, 1u, glm::vec3(0.2f, 0.5f, 0.9f), 0.4f, 0.3f
            );

            // Obje 2: Merkez Kure
            Entity sphereEntity = editorScene->CreateEntity();
            sphereEntity.AddComponent<TransformComponent>(glm::vec3(0.0f, 0.3f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.85f));
            sphereEntity.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Sphere),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.3f, 1u, glm::vec3(0.9f, 0.25f, 0.2f), 0.2f, 0.8f
            );

            // Obje 3: Torus
            torusEntity = editorScene->CreateEntity();
            torusEntity.AddComponent<TransformComponent>(glm::vec3(1.8f, 0.2f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.7f, 0.25f, 1.0f));
            torusEntity.AddComponent<VelocityComponent>(glm::vec3(0.0f), glm::vec3(0.5f, 0.0f, 0.0f));
            torusEntity.AddComponent<SDFComponent>(
                static_cast<uint32_t>(PrimitiveType::Torus),
                static_cast<uint32_t>(CSGOperation::SmoothUnion),
                0.25f, 1u, glm::vec3(0.9f, 0.75f, 0.15f), 0.3f, 0.9f
            );
        }

        // 2. Editor -> Runtime Gecisi: Orijinal editor seviyesini bozmadan tam derin kopyalama (Deep-Copy)
        auto runtimeScene = Scene::Copy(editorScene);
        m_SceneManager.SetActiveScene(runtimeScene);
        runtimeScene->OnRuntimeStart();

        Entity selectedEntity;
        uint32_t frameIndex = 0;
        double cpuFrameMs = 0.0;
        double gpuTotalMs = 0.0;

        // 6. Ana render, UI ve profil dongusu
        while (m_Running && !m_Window->ShouldClose()) {
            auto cpuStart = std::chrono::high_resolution_clock::now();

            m_Window->PollEvents();

            float timeSec = static_cast<float>(frameIndex) * 0.016f;

            // Demo hareketleri yalnizca otomatik test/benchmark kosularinda ilerletilir.
            // Interaktif editor modunda transformlar inspector ve viewport gizmosuna aittir.
            const bool runDemoSimulation = m_Config.benchMode || targetFrames > 0;
            if (runDemoSimulation) {
                runtimeScene->OnUpdate(0.016f);
            }

            if (runDemoSimulation && m_Config.stressTest) {
                auto& transforms = runtimeScene->GetRegistry().GetView<TransformComponent>();
                size_t idx = 0;
                for (auto&& [entity, transform] : transforms) {
                    if (idx > 0) { // Zemin haric
                        float phase = timeSec * 1.2f + static_cast<float>(idx) * 0.5f;
                        transform.position.y += std::sin(phase) * 0.005f;
                    }
                    idx++;
                }
            } else if (runDemoSimulation) {
                Entity rBox(boxEntity.GetHandle(), runtimeScene.get());
                if (rBox.HasComponent<TransformComponent>()) {
                    auto& boxTr = rBox.GetComponent<TransformComponent>();
                    boxTr.position.y = 0.2f + std::sin(timeSec * 1.5f) * 0.2f;
                    float angleY = timeSec * 0.5f;
                    boxTr.rotation = glm::angleAxis(angleY, glm::vec3(0.0f, 1.0f, 0.0f));
                }
                Entity rTorus(torusEntity.GetHandle(), runtimeScene.get());
                if (rTorus.HasComponent<TransformComponent>()) {
                    auto& torusTr = rTorus.GetComponent<TransformComponent>();
                    torusTr.position.y = 0.2f + std::cos(timeSec * 1.5f) * 0.2f;
                    float angleX = timeSec * 0.5f;
                    torusTr.rotation = glm::angleAxis(angleX, glm::vec3(1.0f, 0.0f, 0.0f));
                }
            }

            // Render Extraction: ECS SparseSet -> std430 SDFEditGPU Buffer & Entity Haritasi (Sifir her-kare heap allocation)
            ExtractRenderData(runtimeScene->GetRegistry(), m_SceneEdits, m_SceneEntities);

            // PR-9: Test ve benchmark modunda merkezdeki nesneyi dogrulamak icin 3. karede ekran merkezini sec
            if ((m_Config.benchMode || targetFrames > 0) && frameIndex == 3) {
                int cx = m_Window->GetWidth() / 2;
                int cy = m_Window->GetHeight() / 2;
                m_SDFRenderer->SetPickingRequest(cx, cy);
            }

            // Editör Viewport'u yeniden boyutlandırıldıysa, Vulkan komut tamponu başlamadan önce güvenli yeniden boyutlandırma yap
            if (m_EditorUI) {
                auto& viewportPanel = m_EditorUI->GetViewportPanel();
                if (viewportPanel.HasPendingResize()) {
                    auto newSize = viewportPanel.GetPendingResize();
                    if (newSize.x > 0.0f && newSize.y > 0.0f) {
                        m_SDFRenderer->Resize(static_cast<int>(newSize.x), static_cast<int>(newSize.y));
                    }
                    viewportPanel.ClearPendingResize();
                }
            }

            // GPU SSBO'ya yaz ve Two-Level Grid'i guncelle
            m_SDFRenderer->UpdateEdits(m_SceneEdits, m_Config.legacyMap);

            // Swapchain resmi edin
            bool hasSwapchainImage = false;
            if (!m_Config.benchMode && targetFrames < 0) {
                hasSwapchainImage = m_VulkanContext->AcquireNextImage();
            }

            // Seçili varlığın render edit indeksini bul ve shader Fresnel Rim-Light için ayarla
            int selectedHitIndex = -1;
            if (selectedEntity.IsValid()) {
                EntityHandle selHandle = selectedEntity.GetHandle();
                for (size_t i = 0; i < m_SceneEntities.size(); ++i) {
                    if (m_SceneEntities[i] == selHandle) {
                        selectedHitIndex = static_cast<int>(i);
                        break;
                    }
                }
            }
            m_SDFRenderer->SetSelectedHitIndex(selectedHitIndex);

            // GPU Komut Tamponu & Timestamp Olcumu
            auto cmd = m_VulkanContext->BeginFrameCommand();

            // 1. 3D SDF Compute Raymarching
            m_SDFRenderer->Render(
                cmd,
                timeSec,
                m_Config.normalMode,
                m_SDFRenderer->GetWidth(),
                m_SDFRenderer->GetHeight(),
                m_Config.useGrid,
                m_Config.optShadow,
                m_Config.enableTAA,
                frameIndex
            );

            if (hasSwapchainImage) {
                // 2. Swapchain resmini ImGui Dynamic Rendering icin hazırla
                m_VulkanContext->PrepareSwapchainImage();

                // 3. ImGui Editör Panelleri Render (Dynamic Rendering)
                m_EditorUI->BeginFrame();
                m_EditorUI->RenderPanels(*runtimeScene, selectedEntity, static_cast<float>(gpuTotalMs), static_cast<float>(cpuFrameMs));
                m_EditorUI->EndFrame(
                    cmd,
                    m_VulkanContext->GetSwapchain()->GetImageViews()[m_VulkanContext->GetCurrentImageIndex()],
                    m_VulkanContext->GetSwapchain()->GetExtent()
                );

                // 4. Semaphor'larla submit et ve ekrana sun (Present)
                m_VulkanContext->EndFramePresent();
            } else {
                // Headless veya test modu: Dogrudan komut tamponunu submit et
                m_VulkanContext->EndAndSubmitFrameCommand();
            }

            // PR-9: Fence sonrasi donanımsal guvenli secim okumasi (tek seferlik tuketim)
            if (m_SDFRenderer->HasPendingSelection()) {
                auto pickResult = m_SDFRenderer->ConsumeSelectionResult();
                if (pickResult.hasHit) {
                    if (pickResult.hitIndex >= 0 && static_cast<size_t>(pickResult.hitIndex) < m_SceneEntities.size()) {
                        Entity hitEntity(m_SceneEntities[pickResult.hitIndex], runtimeScene.get());
                        selectedEntity = hitEntity; // Editörde seçili nesneyi güncelle
                        std::cout << "[Astral::Picking] ISABET: hitIndex = " << pickResult.hitIndex 
                                  << " -> " << hitEntity.ToDisplayString()
                                  << " (Valid: " << (hitEntity.IsValid() ? "true" : "false") << ")"
                                  << " | Nokta: (" << pickResult.hitPoint.x << ", " << pickResult.hitPoint.y << ", " << pickResult.hitPoint.z << ")"
                                  << " | Mesafe: " << pickResult.hitDistance << "m\n";
                    } else {
                        std::cout << "[Astral::Picking] ISABET YOK (Gokyuzu/Bosluk)\n";
                    }
                }
            }

            auto cpuEnd = std::chrono::high_resolution_clock::now();
            double cpuFrameMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
            double gpuTotalMs = m_VulkanContext->GetLastGpuTimeMs();

            frameIndex++;

            if (m_BenchmarkLogger) {
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
    }
}

std::string Application::GetName() {
    return "AstralEngine";
}

std::string Application::GetVersion() {
    return "1.0.0";
}

} // namespace Astral
