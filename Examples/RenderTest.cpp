#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIResource.h"
#include <iostream>

using namespace AstralEngine;

class RenderTestApp : public IApplication {
public:
    void OnStart(Engine* owner) override {
        Logger::Info("RenderTest", "Starting Render Test Application...");
        m_engine = owner;
        
        // RenderSubsystem'e erişim sağla
        auto* renderSystem = m_engine->GetSubsystem<RenderSubsystem>();
        if (!renderSystem) {
            Logger::Error("RenderTest", "RenderSubsystem not found!");
            return;
        }

        IRHIDevice* device = renderSystem->GetDevice();
        if (!device) {
            Logger::Error("RenderTest", "RHI Device not found!");
            return;
        }

        Logger::Info("RenderTest", "RHI Device acquired successfully.");

        // Resource Creation Test
        TestResourceCreation(device);
    }

    void OnUpdate(float /*deltaTime*/) override {
        // Şu an için sadece ekran temizleniyor (siyah).
        // RenderSubsystem::OnUpdate otomatik olarak BeginFrame/EndFrame/Present yapıyor.
    }

    void OnShutdown() override {
        Logger::Info("RenderTest", "Shutting down...");
        m_resources.clear();
    }

private:
    void TestResourceCreation(IRHIDevice* device) {
        Logger::Info("RenderTest", "Testing Resource Creation...");

        try {
            // 1. Buffer Test (Vertex Buffer)
            // Basit bir üçgen için veriler
            float vertices[] = {
                0.0f, -0.5f, 0.0f,
                0.5f,  0.5f, 0.0f,
                -0.5f, 0.5f, 0.0f
            };
            uint64_t vSize = sizeof(vertices);
            
            auto vertexBuffer = device->CreateBuffer(vSize, RHIBufferUsage::Vertex, RHIMemoryProperty::HostVisible);
            if (vertexBuffer) {
                Logger::Info("RenderTest", "Vertex Buffer created successfully. Size: {} bytes", vSize);
                
                // Map/Unmap Test
                void* data = vertexBuffer->Map();
                if (data) {
                    memcpy(data, vertices, vSize);
                    vertexBuffer->Unmap();
                    Logger::Info("RenderTest", "Vertex Buffer mapped and data uploaded.");
                } else {
                    Logger::Error("RenderTest", "Failed to map Vertex Buffer!");
                }
                m_resources.push_back(vertexBuffer);
            } else {
                Logger::Error("RenderTest", "Failed to create Vertex Buffer!");
            }

            // 2. Texture Test
            auto texture = device->CreateTexture2D(256, 256, RHIFormat::R8G8B8A8_SRGB, RHITextureUsage::Sampled);
            if (texture) {
                Logger::Info("RenderTest", "Texture 2D created successfully. 256x256");
                m_resources.push_back(texture);
            } else {
                Logger::Error("RenderTest", "Failed to create Texture!");
            }

        } catch (const std::exception& e) {
            Logger::Error("RenderTest", "Exception during resource creation: {}", e.what());
        }
    }

    Engine* m_engine = nullptr;
    std::vector<std::shared_ptr<IRHIResource>> m_resources;
};

int main(int argc, char* argv[]) {
    Logger::InitializeFileLogging();
    Logger::SetLogLevel(Logger::LogLevel::Debug);
    
    Logger::Info("RenderTest", "Initializing Engine...");
    
    try {
        Engine engine;
        RenderTestApp app;
        
        // EngineConfig config;
        // config.windowWidth = 1280;
        // config.windowHeight = 720;
        // config.windowTitle = "Astral Engine - Render Test";
        
        // engine.Initialize(config);
        engine.Run(&app);
        // engine.Shutdown();
        
    } catch (const std::exception& e) {
        Logger::Error("Main", "Crash: {}", e.what());
        return -1;
    }
    
    return 0;
}
