#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIResource.h"
#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Subsystems/Renderer/RHI/IRHICommandList.h"
#include "Subsystems/Renderer/RHI/IRHIDescriptor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>

using namespace AstralEngine;

struct Matrix4 {
    float m[4][4];

    static Matrix4 Identity() {
        Matrix4 mat;
        for(int i=0; i<4; i++)
            for(int j=0; j<4; j++)
                mat.m[i][j] = (i==j) ? 1.0f : 0.0f;
        return mat;
    }

    static Matrix4 RotateZ(float angleRadians) {
        Matrix4 mat = Identity();
        float c = cos(angleRadians);
        float s = sin(angleRadians);
        mat.m[0][0] = c; mat.m[0][1] = -s;
        mat.m[1][0] = s; mat.m[1][1] = c;
        return mat;
    }
};

struct UniformBufferObject {
    Matrix4 model;
};

class RenderTestApp : public IApplication {
public:
    void OnStart(Engine* owner) override {
        Logger::Info("RenderTest", "Starting Render Test Application...");
        m_engine = owner;
        
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

        TestResourceCreation(device);
        CreatePipeline(device);

        renderSystem->SetRenderCallback([this](IRHICommandList* cmdList) {
            OnRender(cmdList);
        });
    }

    void OnUpdate(float deltaTime) override {
        m_rotationAngle += deltaTime * 1.0f; // 1 radian per second
        
        if (m_uniformBuffer) {
            UniformBufferObject ubo{};
            ubo.model = Matrix4::RotateZ(m_rotationAngle);
            
            void* data = m_uniformBuffer->Map();
            if (data) {
                std::memcpy(data, &ubo, sizeof(ubo));
                m_uniformBuffer->Unmap();
            }
        }
    }

    void OnShutdown() override {
        Logger::Info("RenderTest", "Shutting down...");
        
        if (m_engine) {
            auto* renderSystem = m_engine->GetSubsystem<RenderSubsystem>();
            if (renderSystem) {
                 renderSystem->SetRenderCallback(nullptr);
                 if (renderSystem->GetDevice()) {
                     renderSystem->GetDevice()->WaitIdle();
                 }
            }
        }

        m_descriptorSet.reset();
        m_descriptorSetLayout.reset();
        m_uniformBuffer.reset();
        m_pipeline.reset();
        m_vertexBuffer.reset();
        m_resources.clear();
    }

private:
    std::shared_ptr<IRHIPipeline> m_pipeline;
    std::shared_ptr<IRHIBuffer> m_vertexBuffer;
    std::shared_ptr<IRHIDescriptorSetLayout> m_descriptorSetLayout;
    std::shared_ptr<IRHIDescriptorSet> m_descriptorSet;
    std::shared_ptr<IRHIBuffer> m_uniformBuffer;
    
    std::vector<std::shared_ptr<IRHIResource>> m_resources;
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

    void TestResourceCreation(IRHIDevice* device) {
        // Vertex Data
        struct Vertex {
            float position[2];
            float color[3];
        };

        std::vector<Vertex> vertices = {
            {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
        };

        m_vertexBuffer = device->CreateBuffer(
            vertices.size() * sizeof(Vertex),
            RHIBufferUsage::Vertex,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );

        void* data = m_vertexBuffer->Map();
        std::memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
        m_vertexBuffer->Unmap();

        Logger::Info("RenderTest", "Vertex Buffer created and uploaded.");
        
        // Uniform Buffer
        m_uniformBuffer = device->CreateBuffer(
            sizeof(UniformBufferObject),
            RHIBufferUsage::Uniform,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );
        
        // Descriptor Set Layout
        std::vector<RHIDescriptorSetLayoutBinding> bindings;
        RHIDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = RHIShaderStage::Vertex;
        bindings.push_back(uboBinding);
        
        m_descriptorSetLayout = device->CreateDescriptorSetLayout(bindings);
        
        // Allocate Descriptor Set
        m_descriptorSet = device->AllocateDescriptorSet(m_descriptorSetLayout.get());
        
        // Update Descriptor Set
        m_descriptorSet->UpdateUniformBuffer(0, m_uniformBuffer.get(), 0, sizeof(UniformBufferObject));
        
        Logger::Info("RenderTest", "Uniform Buffer and Descriptor Set created.");
    }

    void CreatePipeline(IRHIDevice* device) {
        try {
            auto vertCode = ReadFile("Assets/Shaders/Bin/SimpleTriangle.vert.spv");
            auto fragCode = ReadFile("Assets/Shaders/Bin/SimpleTriangle.frag.spv");
            
            auto vertexShader = device->CreateShader(RHIShaderStage::Vertex, vertCode);
            auto fragmentShader = device->CreateShader(RHIShaderStage::Fragment, fragCode);
            
            // Keep shaders alive if needed, or rely on shared_ptr in pipeline?
            // Usually pipeline might not keep shader module alive in Vulkan after creation, but RHI abstraction might.
            // Let's store them in m_resources just in case for this test.
            m_resources.push_back(vertexShader);
            m_resources.push_back(fragmentShader);

            RHIPipelineStateDescriptor pipelineDesc{};
            pipelineDesc.vertexShader = vertexShader.get();
            pipelineDesc.fragmentShader = fragmentShader.get();
            
            // Vertex Bindings
            RHIVertexInputBinding binding{};
            binding.binding = 0;
            binding.stride = 5 * sizeof(float); // 2 pos + 3 color
            binding.isInstanced = false;
            pipelineDesc.vertexBindings.push_back(binding);

            // Vertex Attributes
            RHIVertexInputAttribute posAttr{};
            posAttr.location = 0;
            posAttr.binding = 0;
            posAttr.format = RHIFormat::R32G32_FLOAT;
            posAttr.offset = 0;
            pipelineDesc.vertexAttributes.push_back(posAttr);

            RHIVertexInputAttribute colAttr{};
            colAttr.location = 1;
            colAttr.binding = 0;
            colAttr.format = RHIFormat::R32G32B32_FLOAT;
            colAttr.offset = 2 * sizeof(float);
            pipelineDesc.vertexAttributes.push_back(colAttr);
            
            // Descriptor Set Layouts
            pipelineDesc.descriptorSetLayouts.push_back(m_descriptorSetLayout.get());

            pipelineDesc.cullMode = RHICullMode::None; // Disable culling to debug black screen
            pipelineDesc.frontFace = RHIFrontFace::CounterClockwise; 

            m_pipeline = device->CreateGraphicsPipeline(pipelineDesc);
            Logger::Info("RenderTest", "Pipeline created successfully.");

        } catch (const std::exception& e) {
            Logger::Error("RenderTest", std::string("Failed to create pipeline: ") + e.what());
        }
    }

    void OnRender(IRHICommandList* cmdList) {
        if (!m_pipeline || !m_vertexBuffer || !m_descriptorSet) return;

        RHIRect2D renderArea{};
        renderArea.offset.x = 0;
        renderArea.offset.y = 0;
        renderArea.extent.width = 800; // Hardcoded for now, should get from window
        renderArea.extent.height = 600;

        // Note: In real engine, Swapchain RenderPass is handled internally or passed here.
        // But VulkanCommandList::BeginRendering currently handles the legacy RenderPass start.
        
        // cmdList->BeginRendering({}, nullptr, renderArea);

        cmdList->BindPipeline(m_pipeline.get());
        
        RHIViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = 800;
        viewport.height = 600;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdList->SetViewport(viewport);

        cmdList->SetScissor(renderArea);

        cmdList->BindVertexBuffer(0, m_vertexBuffer.get(), 0);
        
        // Bind Descriptor Set
        cmdList->BindDescriptorSet(m_pipeline.get(), m_descriptorSet.get(), 0);

        cmdList->Draw(3, 1, 0, 0);

        // cmdList->EndRendering();
    }
};

int main(int argc, char* argv[]) {
    Logger::InitializeFileLogging();
    
    Engine engine;
    RenderTestApp app;
    engine.Run(&app);
    
    return 0;
}
