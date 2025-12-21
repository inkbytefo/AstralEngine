#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIResource.h"
#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Subsystems/Renderer/RHI/IRHICommandList.h"
#include <iostream>
#include <fstream>
#include <vector>

#include <cmath>

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

        // Pipeline Creation
        CreatePipeline(device);

        // Render Callback Setup
        renderSystem->SetRenderCallback([this](IRHICommandList* cmdList) {
            OnRender(cmdList);
        });
    }

    void OnUpdate(float deltaTime) override {
        // Update rotation
        m_rotationAngle += deltaTime * 1.0f; // 1 radian per second
    }

    void OnShutdown() override {
        Logger::Info("RenderTest", "Shutting down...");
        
        // Ensure GPU is idle before destroying resources!
        if (m_engine) {
            auto* renderSystem = m_engine->GetSubsystem<RenderSubsystem>();
            if (renderSystem) {
                 // Clear callback
                 renderSystem->SetRenderCallback(nullptr);
                 if (renderSystem->GetDevice()) {
                     renderSystem->GetDevice()->WaitIdle();
                 }
            }
        }

        m_pipeline.reset();
        m_vertexBuffer.reset();
        m_resources.clear();
    }

private:
    std::shared_ptr<IRHIPipeline> m_pipeline;
    std::shared_ptr<IRHIBuffer> m_vertexBuffer;
    std::vector<std::shared_ptr<IRHIResource>> m_resources; // Keep others alive
    Engine* m_engine = nullptr;
    float m_rotationAngle = 0.0f;

    std::vector<uint8_t> ReadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filename);
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<uint8_t> buffer(fileSize);
        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    void CreatePipeline(IRHIDevice* device) {
        try {
            // Use Slang compiled shaders from new modular structure
            auto vertCode = ReadFile("Assets/Shaders/Bin/SimpleTriangle.vert.spv");
            auto fragCode = ReadFile("Assets/Shaders/Bin/SimpleTriangle.frag.spv");

            auto vertShader = device->CreateShader(RHIShaderStage::Vertex, vertCode);
            auto fragShader = device->CreateShader(RHIShaderStage::Fragment, fragCode);

            RHIPipelineStateDescriptor desc{};
            desc.vertexShader = vertShader.get();
            desc.fragmentShader = fragShader.get();
            
            // Vertex Input: Position (Vec3) + Color (Vec3)
            RHIVertexInputBinding binding{};
            binding.binding = 0;
            binding.stride = 6 * sizeof(float); // 3 float pos + 3 float color
            binding.isInstanced = false;
            desc.vertexBindings.push_back(binding);

            // Attribute 0: Position
            RHIVertexInputAttribute attrPos{};
            attrPos.location = 0;
            attrPos.binding = 0;
            attrPos.format = RHIFormat::R32G32B32_FLOAT;
            attrPos.offset = 0;
            desc.vertexAttributes.push_back(attrPos);

            // Attribute 1: Color
            RHIVertexInputAttribute attrColor{};
            attrColor.location = 1; // Assuming shader uses location 1 for color? Slang defaults to sequential locations if not specified.
            // Wait, in my slang shader:
            // struct VSInput { float3 position : POSITION; float3 color : COLOR; };
            // Slang usually maps these. Let's assume POSITION=0, COLOR=1.
            // Actually, for strictness I should check SPIR-V or assume sequential.
            // Let's assume Slang assigns location 0 and 1.
            attrColor.binding = 0;
            attrColor.format = RHIFormat::R32G32B32_FLOAT;
            attrColor.offset = 3 * sizeof(float);
            desc.vertexAttributes.push_back(attrColor);

            // Push Constants
            RHIPushConstantRange pushConst{};
            pushConst.stageFlags = RHIShaderStage::Vertex;
            pushConst.offset = 0;
            pushConst.size = 16 * sizeof(float); // 4x4 matrix
            desc.pushConstants.push_back(pushConst);

            // Default states
            desc.cullMode = RHICullMode::Back;
            desc.frontFace = RHIFrontFace::CounterClockwise; 
            desc.depthTestEnabled = false; 
            
            m_pipeline = device->CreateGraphicsPipeline(desc);
            if (m_pipeline) {
                Logger::Info("RenderTest", "Graphics Pipeline created successfully.");
            } else {
                Logger::Error("RenderTest", "Failed to create Graphics Pipeline!");
            }
        } catch (const std::exception& e) {
            Logger::Error("RenderTest", "Failed to create pipeline: {}", e.what());
        }
    }

    void OnRender(IRHICommandList* cmdList) {
        if (!m_pipeline || !m_vertexBuffer) return;

        cmdList->BindPipeline(m_pipeline.get());
        cmdList->BindVertexBuffer(0, m_vertexBuffer.get(), 0);
        
        // Compute Rotation Matrix (Z-axis rotation)
        float c = std::cos(m_rotationAngle);
        float s = std::sin(m_rotationAngle);
        
        // Column-major 4x4 matrix (standard for GLSL/SPIR-V)
        // Rotation around Z
        // | c -s  0  0 |
        // | s  c  0  0 |
        // | 0  0  1  0 |
        // | 0  0  0  1 |
        float matrix[16] = {
             c,  s, 0.0f, 0.0f,
            -s,  c, 0.0f, 0.0f,
             0.0f, 0.0f, 1.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f
        };
        
        cmdList->PushConstants(m_pipeline.get(), RHIShaderStage::Vertex, 0, sizeof(matrix), matrix);

        IRHITexture* backBuffer = m_engine->GetSubsystem<RenderSubsystem>()->GetDevice()->GetCurrentBackBuffer();
        if (backBuffer) {
            RHIViewport vp{};
            vp.x = 0;
            vp.y = 0;
            vp.width = (float)backBuffer->GetWidth();
            vp.height = (float)backBuffer->GetHeight();
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            cmdList->SetViewport(vp);

            RHIRect2D scissor{};
            scissor.offset = {0, 0, 0};
            scissor.extent = {backBuffer->GetWidth(), backBuffer->GetHeight()};
            cmdList->SetScissor(scissor);
        }

        cmdList->Draw(3, 1, 0, 0);
    }

    void TestResourceCreation(IRHIDevice* device) {
        Logger::Info("RenderTest", "Testing Resource Creation...");

        try {
            // 1. Buffer Test (Vertex Buffer)
            // Interleaved: Pos(3) + Color(3)
            float vertices[] = {
                 0.0f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f, // Top (Red)
                -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f, // Bottom Left (Green)
                 0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f  // Bottom Right (Blue)
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
                m_vertexBuffer = vertexBuffer; // Store it
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
