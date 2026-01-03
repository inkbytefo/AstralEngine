#include "Core/Engine.h"
#include "Core/Logger.h"
#include "Core/IApplication.h"
#include "Subsystems/Renderer/Core/RenderSubsystem.h"
#include "Subsystems/Renderer/Core/Mesh.h"
#include "Subsystems/Renderer/Core/Texture.h"
#include "Subsystems/Renderer/Core/Material.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIResource.h"
#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Subsystems/Renderer/RHI/IRHICommandList.h"
#include "Subsystems/Renderer/RHI/IRHIDescriptor.h"
#include "Subsystems/Asset/AssetManager.h"
#include <iostream>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Subsystems/Renderer/Core/IBLProcessor.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <thread>
#include <filesystem>

#include "Subsystems/Renderer/Core/Camera.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Platform/InputManager.h"
#include "ECS/Components.h"
#include "Subsystems/Scene/Scene.h"
#include "Subsystems/Scene/Entity.h"
#include "Subsystems/Scene/SceneSerializer.h"
#include "Core/Math/Ray.h"
#include <limits>

using namespace AstralEngine;

struct LightGPU {
    alignas(16) glm::vec4 position; // w = type (0=Directional, 1=Point, 2=Spot)
    alignas(16) glm::vec4 direction; // w = range
    alignas(16) glm::vec4 color;     // w = intensity
    alignas(16) glm::vec4 params;    // x = innerCone, y = outerCone
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 lightSpaceMatrix;
    alignas(16) glm::vec4 viewPos;
    alignas(4)  int lightCount;
    alignas(4)  int hasShadows;
    alignas(4)  int hasIBL;
    alignas(4)  int padding;
    alignas(16) LightGPU lights[4];
};

struct PushConstants {
    glm::mat4 model;
};

class RenderTestApp : public IApplication {
    std::unique_ptr<IBLProcessor> m_iblProcessor;
    AssetHandle m_hdrHandle;
    std::shared_ptr<Texture> m_envMap;
    std::shared_ptr<Texture> m_irradianceMap;
    std::shared_ptr<Texture> m_prefilterMap;
    std::shared_ptr<Texture> m_brdfLut;
    std::shared_ptr<Texture> m_dummyCubemap;
    std::shared_ptr<Texture> m_dummyTexture;
    bool m_iblGenerated = false;

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

        // Initialize IBL Processor
        m_iblProcessor = std::make_unique<IBLProcessor>(m_device);

        // Create Dummy Textures for initial bindings
        m_dummyTexture = Texture::CreateFlatTexture(m_device, 1, 1, glm::vec4(0.0f));
        m_dummyCubemap = Texture::CreateFlatCubemap(m_device, 1, 1, glm::vec4(0.0f));

        // Initialize AssetManager
        std::filesystem::path assetPath = std::filesystem::current_path() / "Assets";
        if (!m_assetManager.Initialize(assetPath.string())) {
            Logger::Error("RenderTest", "Failed to initialize AssetManager!");
            return;
        }

        // Create Global Descriptor Set Layout (Set 0: Camera/Object Data)
        CreateGlobalLayout();

        // Create UBOs (Independent of layout)
        CreateUBOs(device);

        // Create Global Descriptor Sets
        CreateGlobalDescriptorSets();

        // Load BMW Model Async
        Logger::Info("RenderTest", "Loading 3DObjects/bmw_m5_e34/scene.gltf...");
        m_modelHandle = m_assetManager.Load<ModelData>("3DObjects/bmw_m5_e34/scene.gltf");
        
        // Load Texture Async (Using a car paint texture for the test)
        Logger::Info("RenderTest", "Loading BMW Texture...");
        m_textureHandle = m_assetManager.Load<TextureData>("3DObjects/bmw_m5_e34/textures/E34_CAR_PAINT_clearcoat_roughness.png");

        // Load HDR Map for IBL
        Logger::Info("RenderTest", "Loading HDR Map: Textures/HDR/puresky.hdr");
        m_hdrHandle = m_assetManager.Load<TextureData>("Textures/HDR/puresky.hdr");

        // Load Material Async
        Logger::Info("RenderTest", "Loading Material...");
        m_materialHandle = m_assetManager.Load<MaterialData>("Materials/Default.amat");

        // Initialize Scene
        m_activeScene = std::make_unique<Scene>();

        // Create BMW Entity
        m_cubeEntity = m_activeScene->CreateEntity("BMW_M5");
        auto& bmwTransform = m_cubeEntity.GetComponent<TransformComponent>();
        bmwTransform.scale = glm::vec3(0.01f); // glTF might be large
        
        // Add RenderComponent to BMW
        if (m_materialHandle.IsValid() && m_modelHandle.IsValid()) {
            m_cubeEntity.AddComponent<RenderComponent>(m_materialHandle, m_modelHandle);
        } else {
            Logger::Warning("RenderTest", "Material or Model handle invalid, cannot add RenderComponent to BMW.");
        }
        
        // Initialize Lights (Using Scene ECS)
        Entity dirLightEntity = m_activeScene->CreateEntity("DirectionalLight");
        auto& dirTransform = dirLightEntity.GetComponent<TransformComponent>();
        dirTransform.rotation = glm::vec3(glm::radians(-45.0f), glm::radians(-30.0f), 0.0f);
        
        auto& dirLight = dirLightEntity.AddComponent<LightComponent>();
        dirLight.type = LightComponent::LightType::Directional;
        dirLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
        dirLight.intensity = 0.5f;

        Entity pointLightEntity = m_activeScene->CreateEntity("PointLight");
        auto& pointTransform = pointLightEntity.GetComponent<TransformComponent>();
        pointTransform.position = glm::vec3(2.0f, 2.0f, 2.0f);

        auto& pointLight = pointLightEntity.AddComponent<LightComponent>();
        pointLight.type = LightComponent::LightType::Point;
        pointLight.color = glm::vec3(1.0f, 0.0f, 0.0f);
        pointLight.intensity = 2.0f;
        pointLight.range = 10.0f;

        // Hierarchy Test: Parent PointLight to Cube
        // The point light will rotate with the cube
        m_activeScene->ParentEntity(pointLightEntity, m_cubeEntity);
        // Reset local position to be relative to parent
        pointTransform.position = glm::vec3(2.5f, 0.0f, 0.0f); 

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
                    if (textureData) {
                        m_texture = std::make_shared<Texture>(m_device, *textureData);
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

        if (!m_material && m_materialHandle.IsValid()) {
             if (m_assetManager.IsAssetLoaded(m_materialHandle)) {
                try {
                    auto matData = m_assetManager.GetAsset<MaterialData>(m_materialHandle);
                    if (matData) {
                        // Pass global layout to Material
                        m_material = std::make_unique<Material>(m_device, *matData, m_globalDescriptorSetLayout.get());
                        Logger::Info("RenderTest", "Material created successfully.");
                    }
                } catch (const std::exception& e) {
                    Logger::Error("RenderTest", std::string("Failed to load material: ") + e.what());
                    m_materialHandle = AssetHandle();
                }
             }
        }

        if (m_material && m_texture && !m_textureCreated) {
            m_material->SetAlbedoMap(m_texture);
            m_material->UpdateDescriptorSet();

            m_textureCreated = true;
        }

        // IBL Generation Logic
        if (!m_iblGenerated && m_hdrHandle.IsValid()) {
            if (m_assetManager.IsAssetLoaded(m_hdrHandle)) {
                auto hdrData = m_assetManager.GetAsset<TextureData>(m_hdrHandle);
                if (hdrData) {
                    Logger::Info("RenderTest", "HDR Loaded. Starting IBL Pre-processing...");
                    auto hdrTexture = std::make_shared<Texture>(m_device, *hdrData);
                    
                    m_envMap = m_iblProcessor->ConvertEquirectangularToCubemap(hdrTexture);
                    m_irradianceMap = m_iblProcessor->CreateIrradianceMap(m_envMap);
                    m_prefilterMap = m_iblProcessor->CreatePrefilteredMap(m_envMap);
                    m_brdfLut = m_iblProcessor->CreateBRDFLookUpTable();

                    // Update Global Descriptor Sets with IBL maps
                    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                        m_globalDescriptorSets[i]->UpdateCombinedImageSampler(2, m_irradianceMap->GetRHITexture(), m_irradianceMap->GetRHISampler());
                        m_globalDescriptorSets[i]->UpdateCombinedImageSampler(3, m_prefilterMap->GetRHITexture(), m_prefilterMap->GetRHISampler());
                        m_globalDescriptorSets[i]->UpdateCombinedImageSampler(4, m_brdfLut->GetRHITexture(), m_brdfLut->GetRHISampler());
                    }

                    m_iblGenerated = true;
                    Logger::Info("RenderTest", "IBL Pre-processing completed and descriptors updated.");
                }
            }
        }

        // Also update global descriptor sets with dummy textures if IBL not yet generated
        if (!m_iblGenerated && m_dummyTexture && m_dummyCubemap) {
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(1, m_dummyTexture->GetRHITexture(), m_dummyTexture->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(2, m_dummyCubemap->GetRHITexture(), m_dummyCubemap->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(3, m_dummyCubemap->GetRHITexture(), m_dummyCubemap->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(4, m_dummyTexture->GetRHITexture(), m_dummyTexture->GetRHISampler());
            }
        }

        m_rotationAngle += deltaTime * 1.0f;
        
        if (m_activeScene) {
            // Rotate the cube
            if (m_cubeEntity) {
                auto& tc = m_cubeEntity.GetComponent<TransformComponent>();
                tc.rotation.y = m_rotationAngle;
            }
            m_activeScene->OnUpdate(deltaTime);
        }

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

                    if (input->IsMouseButtonJustPressed(MouseButton::Left)) {
                        int mouseX, mouseY;
                        input->GetMousePosition(mouseX, mouseY);
                        
                        int width = 800; int height = 600;
                        if (platform->GetWindow()) {
                            width = platform->GetWindow()->GetWidth();
                            height = platform->GetWindow()->GetHeight();
                        }

                        // Convert to NDC
                        float x = (2.0f * mouseX) / width - 1.0f;
                        float y = 1.0f - (2.0f * mouseY) / height; // OpenGL Convention (Y up)
                        
                        // Ray Generation
                        glm::mat4 proj = m_camera.GetProjectionMatrix((float)width / height);
                        glm::mat4 view = m_camera.GetViewMatrix();
                        
                        glm::mat4 invProj = glm::inverse(proj);
                        glm::mat4 invView = glm::inverse(view);

                        glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
                        glm::vec4 rayEye = invProj * rayClip;
                        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                        glm::vec3 rayWor = glm::vec3(invView * rayEye);
                        rayWor = glm::normalize(rayWor);
                        
                        Ray ray(m_camera.GetPosition(), rayWor);
                        Logger::Info("RenderTest", "Ray Cast: Origin({}, {}, {}), Dir({}, {}, {})", 
                            ray.Origin.x, ray.Origin.y, ray.Origin.z,
                            ray.Direction.x, ray.Direction.y, ray.Direction.z);

                        // Raycast against all renderable entities
                        float closestT = std::numeric_limits<float>::max();
                        Entity closestEntity;

                        auto renderView = m_activeScene->Reg().view<RenderComponent>();
                        for (auto entityID : renderView) {
                            Entity entity = { entityID, m_activeScene.get() };
                            auto& rc = entity.GetComponent<RenderComponent>();
                            
                            // Get Model Data for AABB
                            if (!rc.modelHandle.IsValid()) continue;
                            
                            auto modelData = m_assetManager.GetAsset<ModelData>(rc.modelHandle);
                            if (!modelData) continue;
                            
                            // Get Transform
                            glm::mat4 modelMatrix = glm::mat4(1.0f);
                            if (entity.HasComponent<WorldTransformComponent>()) {
                                modelMatrix = entity.GetComponent<WorldTransformComponent>().Transform;
                            } else if (entity.HasComponent<TransformComponent>()) {
                                auto& tc = entity.GetComponent<TransformComponent>();
                                modelMatrix = tc.GetLocalMatrix();
                            }
                            
                            // Ray-AABB Test
                            glm::mat4 invModel = glm::inverse(modelMatrix);
                            glm::vec3 rayOriginModel = glm::vec3(invModel * glm::vec4(ray.Origin, 1.0f));
                            glm::vec3 rayDirModel = glm::normalize(glm::vec3(invModel * glm::vec4(ray.Direction, 0.0f)));
                            
                            Ray localRay(rayOriginModel, rayDirModel);
                            float tMin, tMax;
                            if (RayIntersectsAABB(localRay, modelData->boundingBox, tMin, tMax)) {
                                if (tMin < closestT) {
                                    closestT = tMin;
                                    closestEntity = entity;
                                }
                            }
                        }

                        if (closestEntity) {
                            std::string name = "Unknown";
                            if (closestEntity.HasComponent<TagComponent>()) {
                                name = closestEntity.GetComponent<TagComponent>().tag;
                            }
                            Logger::Info("RenderTest", "HIT! Selected Entity: {} (Distance: {})", name, closestT);
                            m_cubeEntity = closestEntity; // Select it
                        } else {
                            Logger::Info("RenderTest", "Missed.");
                        }
                    }

                    if (input->IsKeyJustPressed(KeyCode::K)) {
                        SceneSerializer serializer(m_activeScene.get());
                        serializer.Serialize("scene.json");
                        Logger::Info("RenderTest", "Scene saved to scene.json");
                    }

                    if (input->IsKeyJustPressed(KeyCode::L)) {
                        m_activeScene = std::make_unique<Scene>();
                        SceneSerializer serializer(m_activeScene.get());
                        if (serializer.Deserialize("scene.json")) {
                            Logger::Info("RenderTest", "Scene loaded from scene.json");
                            m_cubeEntity = {};
                            auto view = m_activeScene->Reg().view<TagComponent>();
                            for (auto [entity, tag] : view.each()) {
                                if (tag.tag == "Cube") {
                                    m_cubeEntity = { entity, m_activeScene.get() };
                                    break;
                                }
                            }
                        }
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

        m_globalDescriptorSets.clear();
        m_uniformBuffers.clear();
        m_globalDescriptorSetLayout.reset();
        
        m_mesh.reset();
        m_texture.reset();
        m_material.reset();
        m_resources.clear();
        m_activeScene.reset();
        m_assetManager.Shutdown();
    }

private:
    std::unique_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture> m_texture;
    std::unique_ptr<Material> m_material;
    
    std::shared_ptr<IRHIDescriptorSetLayout> m_globalDescriptorSetLayout;
    std::vector<std::shared_ptr<IRHIBuffer>> m_uniformBuffers;
    std::vector<std::shared_ptr<IRHIDescriptorSet>> m_globalDescriptorSets;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    
    std::vector<std::shared_ptr<IRHIResource>> m_resources;
    Engine* m_engine = nullptr;
    IRHIDevice* m_device = nullptr;
    float m_rotationAngle = 0.0f;
    AssetManager m_assetManager;
    Camera m_camera{glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -35.0f};
    
    AssetHandle m_modelHandle;
    AssetHandle m_textureHandle;
    AssetHandle m_materialHandle;
    bool m_textureCreated = false;
    
    std::unique_ptr<Scene> m_activeScene;
    Entity m_cubeEntity;

    void CreateGlobalLayout() {
        std::vector<RHIDescriptorSetLayoutBinding> bindings;
        
        // Binding 0: UBO
        RHIDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = RHIShaderStage::Vertex | RHIShaderStage::Fragment;
        bindings.push_back(uboBinding);
        
        // Binding 1: Shadow Map
        RHIDescriptorSetLayoutBinding shadowBinding{};
        shadowBinding.binding = 1;
        shadowBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        shadowBinding.descriptorCount = 1;
        shadowBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(shadowBinding);

        // Binding 2: Irradiance Map
        RHIDescriptorSetLayoutBinding irradianceBinding{};
        irradianceBinding.binding = 2;
        irradianceBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        irradianceBinding.descriptorCount = 1;
        irradianceBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(irradianceBinding);

        // Binding 3: Prefilter Map
        RHIDescriptorSetLayoutBinding prefilterBinding{};
        prefilterBinding.binding = 3;
        prefilterBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        prefilterBinding.descriptorCount = 1;
        prefilterBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(prefilterBinding);

        // Binding 4: BRDF LUT
        RHIDescriptorSetLayoutBinding brdfBinding{};
        brdfBinding.binding = 4;
        brdfBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        brdfBinding.descriptorCount = 1;
        brdfBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(brdfBinding);
        
        m_globalDescriptorSetLayout = m_device->CreateDescriptorSetLayout(bindings);
    }

    void CreateUBOs(IRHIDevice* device) {
        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_uniformBuffers[i] = device->CreateBuffer(
                sizeof(UniformBufferObject),
                RHIBufferUsage::Uniform,
                RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
            );
        }
    }

    void CreateGlobalDescriptorSets() {
        m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_globalDescriptorSets[i] = m_device->AllocateDescriptorSet(m_globalDescriptorSetLayout.get());
            m_globalDescriptorSets[i]->UpdateUniformBuffer(0, m_uniformBuffers[i].get(), 0, sizeof(UniformBufferObject));
            
            // Initial dummy bindings to satisfy shader until real textures load or IBL generates
            if (m_dummyTexture && m_dummyCubemap) {
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(1, m_dummyTexture->GetRHITexture(), m_dummyTexture->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(2, m_dummyCubemap->GetRHITexture(), m_dummyCubemap->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(3, m_dummyCubemap->GetRHITexture(), m_dummyCubemap->GetRHISampler());
                m_globalDescriptorSets[i]->UpdateCombinedImageSampler(4, m_dummyTexture->GetRHITexture(), m_dummyTexture->GetRHISampler());
            }
        }
        Logger::Info("RenderTest", "Global Descriptor Sets created with dummy bindings.");
    }

    void OnRender(IRHICommandList* cmdList) {
        if (!m_material || !m_mesh || !m_texture || m_globalDescriptorSets.empty()) return;

        uint32_t currentFrame = m_device->GetCurrentFrameIndex();
        if (currentFrame >= m_uniformBuffers.size()) currentFrame = 0;

        // Update UBO
        UniformBufferObject ubo{};
        ubo.view = m_camera.GetViewMatrix();
        ubo.lightSpaceMatrix = glm::mat4(1.0f); // Identity for now
        ubo.hasShadows = 0;
        ubo.hasIBL = m_iblGenerated ? 1 : 0;

        float aspect = 800.0f / 600.0f;
        if (m_engine) {
            auto* platform = m_engine->GetSubsystem<PlatformSubsystem>();
            if (platform && platform->GetWindow()) {
                aspect = (float)platform->GetWindow()->GetWidth() / (float)platform->GetWindow()->GetHeight();
            }
        }
        ubo.proj = m_camera.GetProjectionMatrix(aspect);
        ubo.proj[1][1] *= -1; // Vulkan Y inversion
        
        // Lighting
        ubo.viewPos = glm::vec4(m_camera.GetPosition(), 1.0f);
        
        ubo.lightCount = 0;
        auto lightView = m_activeScene->Reg().view<const WorldTransformComponent, const LightComponent>();
        for (auto [entity, transform, light] : lightView.each()) {
            if (ubo.lightCount >= 4) break;
            
            LightGPU& gpuLight = ubo.lights[ubo.lightCount];
            // Use World Position
            gpuLight.position = glm::vec4(glm::vec3(transform.Transform[3]), (float)light.type);
            
            // Calculate direction from World Rotation
            glm::mat3 rotMat = glm::mat3(transform.Transform);
            glm::vec3 direction = glm::normalize(rotMat * glm::vec3(0.0f, 0.0f, -1.0f));
            
            gpuLight.direction = glm::vec4(direction, light.range);
            gpuLight.color = glm::vec4(light.color, light.intensity);
            gpuLight.params = glm::vec4(light.innerConeAngle, light.outerConeAngle, 0.0f, 0.0f);
            
            ubo.lightCount++;
        }

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

        cmdList->BindPipeline(m_material->GetPipeline());
        
        RHIViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)renderArea.extent.width;
        viewport.height = (float)renderArea.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdList->SetViewport(viewport);

        cmdList->SetScissor(renderArea);
        
        // Push Constants for Model Matrix
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        if (m_cubeEntity && m_cubeEntity.HasComponent<WorldTransformComponent>()) {
             modelMatrix = m_cubeEntity.GetComponent<WorldTransformComponent>().Transform;
        } else {
             modelMatrix = glm::rotate(glm::mat4(1.0f), m_rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        cmdList->PushConstants(m_material->GetPipeline(), RHIShaderStage::Vertex, 0, sizeof(glm::mat4), &modelMatrix);

        // Bind Descriptor Sets
        // Set 0: Global (Camera)
        cmdList->BindDescriptorSet(m_material->GetPipeline(), m_globalDescriptorSets[currentFrame].get(), 0);
        // Set 1: Material
        if (m_material->GetDescriptorSet()) {
            cmdList->BindDescriptorSet(m_material->GetPipeline(), m_material->GetDescriptorSet(), 1);
        }

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
