#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Renderer/Core/Mesh.h"
#include "Subsystems/Renderer/Core/Texture.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIResource.h"
#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Subsystems/Renderer/RHI/IRHICommandList.h"
#include "Subsystems/Renderer/RHI/IRHIDescriptor.h"
#include "Subsystems/Asset/AssetManager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <thread>
#include <filesystem>

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

    static Matrix4 LookAtRH(float eye[3], float center[3], float up[3]) {
        float f[3] = {center[0]-eye[0], center[1]-eye[1], center[2]-eye[2]};
        float lenF = sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        f[0]/=lenF; f[1]/=lenF; f[2]/=lenF;

        float s[3] = { // f x up
            f[1]*up[2] - f[2]*up[1],
            f[2]*up[0] - f[0]*up[2],
            f[0]*up[1] - f[1]*up[0]
        };
        float lenS = sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
        s[0]/=lenS; s[1]/=lenS; s[2]/=lenS;

        float u[3] = { // s x f
            s[1]*f[2] - s[2]*f[1],
            s[2]*f[0] - s[0]*f[2],
            s[0]*f[1] - s[1]*f[0]
        };

        Matrix4 mat;
        mat.m[0][0] = s[0]; mat.m[1][0] = s[1]; mat.m[2][0] = s[2]; mat.m[3][0] = -(s[0]*eye[0] + s[1]*eye[1] + s[2]*eye[2]);
        mat.m[0][1] = u[0]; mat.m[1][1] = u[1]; mat.m[2][1] = u[2]; mat.m[3][1] = -(u[0]*eye[0] + u[1]*eye[1] + u[2]*eye[2]);
        mat.m[0][2] =-f[0]; mat.m[1][2] =-f[1]; mat.m[2][2] =-f[2]; mat.m[3][2] = (f[0]*eye[0] + f[1]*eye[1] + f[2]*eye[2]);
        mat.m[0][3] = 0.0f; mat.m[1][3] = 0.0f; mat.m[2][3] = 0.0f; mat.m[3][3] = 1.0f;
        
        return mat;
    }

    static Matrix4 Perspective(float fovRadians, float aspect, float zNear, float zFar) {
        float tanHalfFovy = tan(fovRadians / 2.0f);
        Matrix4 mat;
        std::memset(mat.m, 0, sizeof(mat.m));

        mat.m[0][0] = 1.0f / (aspect * tanHalfFovy);
        mat.m[1][1] = 1.0f / (tanHalfFovy); 
        mat.m[1][1] *= -1.0f; // Flip Y for Vulkan

        mat.m[2][2] = zFar / (zNear - zFar);
        mat.m[3][2] = -(zFar * zNear) / (zFar - zNear);
        mat.m[2][3] = -1.0f;
        
        return mat;
    }

    static Matrix4 RotateY(float angleRadians) {
        Matrix4 mat = Identity();
        float c = cos(angleRadians);
        float s = sin(angleRadians);
        mat.m[0][0] = c;  mat.m[0][2] = s;
        mat.m[2][0] = -s; mat.m[2][2] = c;
        return mat;
    }
};

struct UniformBufferObject {
    Matrix4 model;
    Matrix4 view;
    Matrix4 proj;
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
        m_device = device;

        // Initialize AssetManager
        std::filesystem::path assetPath = std::filesystem::current_path() / "Assets";
        if (!m_assetManager.Initialize(assetPath.string())) {
            Logger::Error("RenderTest", "Failed to initialize AssetManager!");
            return;
        }

        // Load Cube Model
        Logger::Info("RenderTest", "Loading Models/Cube.obj...");
        AssetHandle handle = m_assetManager.Load<ModelData>("Models/Cube.obj");
        
        // Wait for load (simple busy wait for test)
        std::shared_ptr<ModelData> modelData = nullptr;
        int attempts = 0;
        while (!modelData && attempts < 100) {
            modelData = m_assetManager.GetAsset<ModelData>(handle);
            if (!modelData) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                attempts++;
            }
        }

        if (modelData) {
            Logger::Info("RenderTest", "Model loaded successfully. Creating Mesh...");
            m_mesh = std::make_unique<Mesh>(device, *modelData);
        } else {
            Logger::Error("RenderTest", "Failed to load Models/Cube.obj");
        }

        // Load Texture
        try {
            Logger::Info("RenderTest", "Loading Texture...");
            m_texture = std::make_unique<Texture>(device, "Assets/Models/testobject/VAZ2101_Body_BaseColor.png");
        } catch (const std::exception& e) {
            Logger::Error("RenderTest", std::string("Failed to load texture: ") + e.what());
        }

        CreateResources(device);
        CreatePipeline(device);

        renderSystem->SetRenderCallback([this](IRHICommandList* cmdList) {
            OnRender(cmdList);
        });
    }

    void OnUpdate(float deltaTime) override {
        m_rotationAngle += deltaTime * 1.0f; 
        m_assetManager.Update(); // Update asset manager (for async tasks cleanup)
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

        m_descriptorSets.clear();
        m_descriptorSetLayout.reset();
        m_uniformBuffers.clear();
        m_pipeline.reset();
        m_mesh.reset();
        m_texture.reset();
        m_resources.clear();
        m_assetManager.Shutdown();
    }

private:
    std::shared_ptr<IRHIPipeline> m_pipeline;
    std::unique_ptr<Mesh> m_mesh;
    std::unique_ptr<Texture> m_texture;
    std::shared_ptr<IRHIDescriptorSetLayout> m_descriptorSetLayout;
    
    std::vector<std::shared_ptr<IRHIBuffer>> m_uniformBuffers;
    std::vector<std::shared_ptr<IRHIDescriptorSet>> m_descriptorSets;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    
    std::vector<std::shared_ptr<IRHIResource>> m_resources;
    Engine* m_engine = nullptr;
    IRHIDevice* m_device = nullptr;
    float m_rotationAngle = 0.0f;
    AssetManager m_assetManager;

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

    void CreateResources(IRHIDevice* device) {
        // Descriptor Set Layout
        std::vector<RHIDescriptorSetLayoutBinding> bindings;
        RHIDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = RHIShaderStage::Vertex;
        bindings.push_back(uboBinding);

        RHIDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 1;
        samplerBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(samplerBinding);
        
        m_descriptorSetLayout = device->CreateDescriptorSetLayout(bindings);
        
        // Create Uniform Buffers and Descriptor Sets per frame
        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_uniformBuffers[i] = device->CreateBuffer(
                sizeof(UniformBufferObject),
                RHIBufferUsage::Uniform,
                RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
            );
            
            m_descriptorSets[i] = device->AllocateDescriptorSet(m_descriptorSetLayout.get());
            m_descriptorSets[i]->UpdateUniformBuffer(0, m_uniformBuffers[i].get(), 0, sizeof(UniformBufferObject));
            
            if (m_texture) {
                m_descriptorSets[i]->UpdateCombinedImageSampler(1, m_texture->GetRHITexture(), m_texture->GetRHISampler());
            }
        }
    }

    void CreatePipeline(IRHIDevice* device) {
        try {
            // Use SimpleMesh shaders (compile them first via CMake)
            auto vertCode = ReadFile("Assets/Shaders/Bin/Test/SimpleMesh.vert.spv");
            auto fragCode = ReadFile("Assets/Shaders/Bin/Test/SimpleMesh.frag.spv");
            
            auto vertexShader = device->CreateShader(RHIShaderStage::Vertex, vertCode);
            auto fragmentShader = device->CreateShader(RHIShaderStage::Fragment, fragCode);
            
            m_resources.push_back(vertexShader);
            m_resources.push_back(fragmentShader);

            RHIPipelineStateDescriptor pipelineDesc{};
            pipelineDesc.vertexShader = vertexShader.get();
            pipelineDesc.fragmentShader = fragmentShader.get();
            
            // Vertex Bindings
            RHIVertexInputBinding binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex); // 56 bytes
            binding.isInstanced = false;
            pipelineDesc.vertexBindings.push_back(binding);

            // Vertex Attributes
            // Location 0: Position (vec3)
            RHIVertexInputAttribute posAttr{};
            posAttr.location = 0;
            posAttr.binding = 0;
            posAttr.format = RHIFormat::R32G32B32_FLOAT;
            posAttr.offset = offsetof(Vertex, position);
            pipelineDesc.vertexAttributes.push_back(posAttr);

            // Location 1: Normal (vec3)
            RHIVertexInputAttribute normAttr{};
            normAttr.location = 1;
            normAttr.binding = 0;
            normAttr.format = RHIFormat::R32G32B32_FLOAT;
            normAttr.offset = offsetof(Vertex, normal);
            pipelineDesc.vertexAttributes.push_back(normAttr);

            // Location 2: TexCoord (vec2)
            RHIVertexInputAttribute uvAttr{};
            uvAttr.location = 2;
            uvAttr.binding = 0;
            uvAttr.format = RHIFormat::R32G32_FLOAT;
            uvAttr.offset = offsetof(Vertex, texCoord);
            pipelineDesc.vertexAttributes.push_back(uvAttr);
            
            // Location 3: Tangent (vec3)
            RHIVertexInputAttribute tanAttr{};
            tanAttr.location = 3;
            tanAttr.binding = 0;
            tanAttr.format = RHIFormat::R32G32B32_FLOAT;
            tanAttr.offset = offsetof(Vertex, tangent);
            pipelineDesc.vertexAttributes.push_back(tanAttr);

            // Location 4: Bitangent (vec3)
            RHIVertexInputAttribute bitanAttr{};
            bitanAttr.location = 4;
            bitanAttr.binding = 0;
            bitanAttr.format = RHIFormat::R32G32B32_FLOAT;
            bitanAttr.offset = offsetof(Vertex, bitangent);
            pipelineDesc.vertexAttributes.push_back(bitanAttr);
            
            // Descriptor Set Layouts
            pipelineDesc.descriptorSetLayouts.push_back(m_descriptorSetLayout.get());

            pipelineDesc.cullMode = RHICullMode::Back; // Cull back faces for cube
            pipelineDesc.frontFace = RHIFrontFace::CounterClockwise; // OBJ uses CCW
            
            pipelineDesc.depthTestEnabled = true;
            pipelineDesc.depthWriteEnabled = true;
            pipelineDesc.depthCompareOp = RHICompareOp::Less; 

            m_pipeline = device->CreateGraphicsPipeline(pipelineDesc);
            Logger::Info("RenderTest", "Pipeline created successfully.");

        } catch (const std::exception& e) {
            Logger::Error("RenderTest", std::string("Failed to create pipeline: ") + e.what());
        }
    }

    void OnRender(IRHICommandList* cmdList) {
        if (!m_pipeline || !m_mesh || m_descriptorSets.empty()) return;

        uint32_t currentFrame = m_device->GetCurrentFrameIndex();
        if (currentFrame >= m_uniformBuffers.size()) currentFrame = 0;

        // Update UBO
        UniformBufferObject ubo{};
        ubo.model = Matrix4::RotateY(m_rotationAngle);
        
        float eye[3] = {1.5f, 1.5f, 1.5f};
        float center[3] = {0.0f, 0.0f, 0.0f};
        float up[3] = {0.0f, 1.0f, 0.0f};
        ubo.view = Matrix4::LookAtRH(eye, center, up);

        float aspect = 800.0f / 600.0f; // TODO: Get actual aspect
        ubo.proj = Matrix4::Perspective(45.0f * 3.14159f / 180.0f, aspect, 0.1f, 10.0f);
        
        void* data = m_uniformBuffers[currentFrame]->Map();
        if (data) {
            std::memcpy(data, &ubo, sizeof(ubo));
            m_uniformBuffers[currentFrame]->Unmap();
        }

        RHIRect2D renderArea{};
        renderArea.offset.x = 0;
        renderArea.offset.y = 0;
        renderArea.extent.width = 800;
        renderArea.extent.height = 600;

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
        
        // Bind Descriptor Set
        cmdList->BindDescriptorSet(m_pipeline.get(), m_descriptorSets[currentFrame].get(), 0);

        // Bind and Draw Mesh
        m_mesh->Draw(cmdList);
    }
};

int main(int argc, char* argv[]) {
    Logger::InitializeFileLogging();
    
    Engine engine;
    RenderTestApp app;
    engine.Run(&app);
    
    return 0;
}
