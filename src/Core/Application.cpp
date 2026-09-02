#include "Astral/Core/Application.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Core/BenchmarkLogger.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"

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
    if (m_SDFRenderer) {
        m_SDFRenderer.reset();
    }
    if (m_VulkanContext) {
        m_VulkanContext->WaitIdle();
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

        // 4. Benchmark logger kurulumu
        if (m_Config.benchMode) {
            m_BenchmarkLogger = std::make_unique<BenchmarkLogger>();
        }

        int targetFrames = maxFrames > 0 ? maxFrames : (m_Config.benchMode ? m_Config.benchFrames : -1);

        m_Running = true;
        std::cout << "[Astral::Application] Normal Modu: " << (m_Config.normalMode == 1 ? "Tetrahedron (4-tap optimize)" : "Central Differences (6-tap)") << "\n";
        std::cout << "[Astral::Application] Bellek Esleme Modu: " << (m_Config.legacyMap ? "Legacy Map/Unmap (Kare basi vkMapMemory)" : "Persistent Mapping (Kalici Pointer)") << "\n";
        std::cout << "[Astral::Application] Izgara Hizlandirmasi (PR-6): " << (m_Config.useGrid ? "AKTIF (Empty Space Skipping)" : "KAPALI (Brute Force)") << "\n";
        std::cout << "[Astral::Application] Sahne Modu: " << (m_Config.stressTest ? "Karmasik Stress Sahnesi (32 Nesne)" : "Standart Sahne (4 Nesne)") << "\n";
        if (targetFrames > 0) {
            std::cout << "[Astral::Application] Hedef: " << targetFrames << " kare calisip sonlanacak...\n";
        } else {
            std::cout << "[Astral::Application] Ana olay dongusune giriliyor (Cikmak icin pencereyi kapatin)...\n";
        }

        // Sahne nesnelerini hazirla (PR-5 Dynamic SSBO & PR-6 Stress Test)
        std::vector<SDFEditGPU> sceneEdits;
        // Obje 0: Zemin
        SDFEditGPU ground{};
        ground.primitiveType = static_cast<uint32_t>(PrimitiveType::Plane);
        ground.position = glm::vec3(0.0f, -1.0f, 0.0f);
        ground.scale = glm::vec3(1.0f, 1.0f, 1.0f);
        ground.albedo = glm::vec3(0.3f, 0.32f, 0.35f);
        ground.roughness = 0.8f;
        ground.metallic = 0.05f;
        ground.operation = static_cast<uint32_t>(CSGOperation::Union);
        sceneEdits.push_back(ground);

        if (m_Config.stressTest) {
            // 31 adet dinamik dagilmis nesne olustur (Toplam 32 nesne)
            for (int i = 0; i < 31; ++i) {
                SDFEditGPU obj{};
                float angle = static_cast<float>(i) * (2.0f * 3.14159f / 31.0f);
                float radius = 3.0f + static_cast<float>(i % 3) * 2.5f;
                float heightY = 0.5f + static_cast<float>(i % 4) * 1.0f;
                obj.position = glm::vec3(std::cos(angle) * radius, heightY, std::sin(angle) * radius);

                uint32_t type = i % 3; // 0=Sphere, 1=Box, 2=Torus
                obj.primitiveType = type;
                if (type == 0) {
                    obj.scale = glm::vec3(0.5f + (i % 2) * 0.2f);
                    obj.albedo = glm::vec3(0.85f, 0.2f + (i % 5) * 0.15f, 0.25f);
                } else if (type == 1) {
                    obj.scale = glm::vec3(0.45f + (i % 3) * 0.1f);
                    obj.albedo = glm::vec3(0.2f, 0.5f + (i % 4) * 0.1f, 0.9f);
                } else {
                    obj.scale = glm::vec3(0.6f, 0.2f, 1.0f);
                    obj.albedo = glm::vec3(0.9f, 0.8f, 0.2f);
                }
                obj.roughness = 0.3f;
                obj.metallic = 0.5f;
                obj.operation = static_cast<uint32_t>(CSGOperation::SmoothUnion);
                obj.blendFactor = 0.2f;
                sceneEdits.push_back(obj);
            }
        } else {
            // Standart 4 nesne
            // Obje 1: Kutu (Donen kutu)
            SDFEditGPU box{};
            box.primitiveType = static_cast<uint32_t>(PrimitiveType::Box);
            box.position = glm::vec3(-1.8f, 0.2f, 0.0f);
            box.scale = glm::vec3(0.6f);
            box.albedo = glm::vec3(0.2f, 0.5f, 0.9f);
            box.roughness = 0.4f;
            box.metallic = 0.3f;
            box.operation = static_cast<uint32_t>(CSGOperation::SmoothUnion);
            box.blendFactor = 0.3f;
            sceneEdits.push_back(box);

            // Obje 2: Merkez Kure
            SDFEditGPU sphere{};
            sphere.primitiveType = static_cast<uint32_t>(PrimitiveType::Sphere);
            sphere.position = glm::vec3(0.0f, 0.3f, 0.0f);
            sphere.scale = glm::vec3(0.85f);
            sphere.albedo = glm::vec3(0.9f, 0.25f, 0.2f);
            sphere.roughness = 0.2f;
            sphere.metallic = 0.8f;
            sphere.operation = static_cast<uint32_t>(CSGOperation::SmoothUnion);
            sphere.blendFactor = 0.3f;
            sceneEdits.push_back(sphere);

            // Obje 3: Torus (Donen torus)
            SDFEditGPU torus{};
            torus.primitiveType = static_cast<uint32_t>(PrimitiveType::Torus);
            torus.position = glm::vec3(1.8f, 0.2f, 0.0f);
            torus.scale = glm::vec3(0.7f, 0.25f, 1.0f);
            torus.albedo = glm::vec3(0.9f, 0.75f, 0.15f);
            torus.roughness = 0.3f;
            torus.metallic = 0.9f;
            torus.operation = static_cast<uint32_t>(CSGOperation::SmoothUnion);
            torus.blendFactor = 0.25f;
            sceneEdits.push_back(torus);
        }

        uint32_t frameIndex = 0;

        // 5. Ana render ve profil dongusu
        while (m_Running && !m_Window->ShouldClose()) {
            auto cpuStart = std::chrono::high_resolution_clock::now();

            m_Window->PollEvents();

            float timeSec = static_cast<float>(frameIndex) * 0.016f;

            // Dinamik nesnelerin pozisyon ve rotasyonlarini guncelle
            if (m_Config.stressTest) {
                for (size_t i = 1; i < sceneEdits.size(); ++i) {
                    float phase = timeSec * 1.2f + static_cast<float>(i) * 0.5f;
                    sceneEdits[i].position.y += std::sin(phase) * 0.005f;
                }
            } else {
                sceneEdits[1].position.y = 0.2f + std::sin(timeSec * 1.5f) * 0.2f;
                float angleY = timeSec * 0.5f;
                sceneEdits[1].rotation = glm::vec4(0.0f, std::sin(angleY), 0.0f, std::cos(angleY));

                sceneEdits[3].position.y = 0.2f + std::cos(timeSec * 1.5f) * 0.2f;
                float angleX = timeSec * 0.5f;
                sceneEdits[3].rotation = glm::vec4(std::sin(angleX), 0.0f, 0.0f, std::cos(angleX));
            }

            // GPU SSBO'ya yaz ve Two-Level Grid'i guncelle
            m_SDFRenderer->UpdateEdits(sceneEdits, m_Config.legacyMap);

            // GPU Komut Tamponu & Timestamp Olcumu
            auto cmd = m_VulkanContext->BeginFrameCommand();

            m_SDFRenderer->Render(
                cmd,
                timeSec,
                m_Config.normalMode,
                m_Window->GetWidth(),
                m_Window->GetHeight(),
                m_Config.useGrid
            );

            m_VulkanContext->EndAndSubmitFrameCommand();

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