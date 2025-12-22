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

#include "Subsystems/Renderer/Core/Camera.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Platform/InputManager.h"

using namespace AstralEngine;

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
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

        // Load Cube Model Async
        Logger::Info("RenderTest", "Loading Models/Cube.obj...");
        m_modelHandle = m_assetManager.Load<ModelData>("Models/Cube.obj");
        
        // Load Texture Async
        Logger::Info("RenderTest", "Loading Texture...");
        m_textureHandle = m_assetManager.Load<TextureData>("Models/testobject/VAZ2101_Body_BaseColor.png");

        // Create Resources (UBO, Layouts) - Independent of mesh content
        CreateResources(device);
        
        // Pipeline creation depends on layout, so we can create it now too
        CreatePipeline(device);

        renderSystem->SetRenderCallback([this](IRHICommandList* cmdList) {
            OnRender(cmdList);
        });
    }

    void OnUpdate(float deltaTime) override {
        // Async Asset Loading Checks
        if (!m_mesh && m_modelHandle.IsValid()) {
            if (m_assetManager.IsAssetLoaded(m_modelHandle)) {
                auto modelData = m_assetManager.GetAsset<ModelData>(m_modelHandle);
                if (modelData) {
                    Logger::Info("RenderTest", "Model loaded successfully. Creating Mesh...");
                    m_mesh = std::make_unique<Mesh>(m_device, *modelData);
                }
            } else if (m_assetManager.GetAssetState(m_modelHandle) == AssetLoadState::Failed) {
                Logger::Error("RenderTest", "Failed to load model asset.");
                m_modelHandle = AssetHandle(); // Invalid handle to stop checking
            }
        }

        if (!m_texture && m_textureHandle.IsValid()) {
            if (m_assetManager.IsAssetLoaded(m_textureHandle)) {
                try {
                    auto textureData = m_assetManager.GetAsset<TextureData>(m_textureHandle);
                    if (textureData && !m_textureCreated) {
                        m_texture = std::make_unique<Texture>(m_device, *textureData);
                        m_textureCreated = true;
                        UpdateDescriptorSets(); // Update descriptors with new texture
                        Logger::Info("RenderTest", "Texture created successfully from async asset.");
                    }
                } catch (const std::exception& e) {
                    Logger::Error("RenderTest", std::string("Failed to load texture: ") + e.what());
                    m_textureHandle = AssetHandle(); // Stop checking
                }
            } else if (m_assetManager.GetAssetState(m_textureHandle) == AssetLoadState::Failed) {
                Logger::Error("RenderTest", "Failed to load texture asset.");
                m_textureHandle = AssetHandle(); // Stop checking
            }
        }

        m_rotationAngle += deltaTime * 1.0f; 
        m_assetManager.Update(); // Update asset manager (for async tasks cleanup)

        if (m_engine) {
            auto* platform = m_engine->GetSubsystem<PlatformSubsystem>();
            if (platform) {
                auto* input = platform->GetInputManager();
                if (input) {
                    float speedMultiplier = input->IsKeyPressed(KeyCode::LeftShift) ? 2.5f : 1.0f;
                    m_camera.SetMovementSpeed(2.5f * speedMultiplier);

                    if (input->IsKeyPressed(KeyCode::W)) m_camera.ProcessKeyboard(CameraMovement::FORWARD, deltaTime);
                    if (input->IsKeyPressed(KeyCode::S)) m_camera.ProcessKeyboard(CameraMovement::BACKWARD, deltaTime);
                    if (input->IsKeyPressed(KeyCode::A)) m_camera.ProcessKeyboard(CameraMovement::LEFT, deltaTime);
                    if (input->IsKeyPressed(KeyCode::D)) m_camera.ProcessKeyboard(CameraMovement::RIGHT, deltaTime);
                    if (input->IsKeyPressed(KeyCode::Q)) m_camera.ProcessKeyboard(CameraMovement::DOWN, deltaTime);
                    if (input->IsKeyPressed(KeyCode::E)) m_camera.ProcessKeyboard(CameraMovement::UP, deltaTime);

                    if (input->IsMouseButtonPressed(MouseButton::Right)) {
                        int dx, dy;
                        input->GetMouseDelta(dx, dy);
                        m_camera.ProcessMouseMovement((float)dx, -(float)dy);
                    }
                }
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
    Camera m_camera{glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -35.0f};
    
    AssetHandle m_modelHandle;
    AssetHandle m_textureHandle;
    bool m_textureCreated = false;

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

    void UpdateDescriptorSets() {
        if (!m_texture) return;
        for (auto& set : m_descriptorSets) {
            if (set) {
                set->UpdateCombinedImageSampler(1, m_texture->GetRHITexture(), m_texture->GetRHISampler());
            }
        }
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
        if (!m_pipeline || !m_mesh || !m_texture || m_descriptorSets.empty()) return;

        uint32_t currentFrame = m_device->GetCurrentFrameIndex();
        if (currentFrame >= m_uniformBuffers.size()) currentFrame = 0;

        // Update UBO
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), m_rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.view = m_camera.GetViewMatrix();

        float aspect = 800.0f / 600.0f;
        if (m_engine) {
            auto* platform = m_engine->GetSubsystem<PlatformSubsystem>();
            if (platform && platform->GetWindow()) {
                aspect = (float)platform->GetWindow()->GetWidth() / (float)platform->GetWindow()->GetHeight();
            }
        }
        ubo.proj = m_camera.GetProjectionMatrix(aspect);
        
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
        if (m_engine) {
             auto* platform = m_engine->GetSubsystem<PlatformSubsystem>();
             if (platform && platform->GetWindow()) {
                 renderArea.extent.width = platform->GetWindow()->GetWidth();
                 renderArea.extent.height = platform->GetWindow()->GetHeight();
             }
        }

        cmdList->BindPipeline(m_pipeline.get());
        
        RHIViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)renderArea.extent.width;
        viewport.height = (float)renderArea.extent.height;
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
