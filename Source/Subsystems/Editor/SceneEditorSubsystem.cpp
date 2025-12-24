#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/MathUtils.h"
#include "../../Events/EventManager.h"
#include "../../Subsystems/Asset/AssetSubsystem.h"
#include "../../Subsystems/Platform/PlatformSubsystem.h"
#include "../../Subsystems/Renderer/Core/RenderSubsystem.h"
#include "../../Subsystems/Renderer/RHI/IRHICommandList.h"
#include "../../Subsystems/Scene/Entity.h"
#include "../../Subsystems/Scene/SceneSerializer.h"
#include "../Renderer/RHI/Vulkan/VulkanCommandList.h"
#include "../Renderer/RHI/Vulkan/VulkanResources.h"
#include "../UI/UISubsystem.h"
#include <entt/entt.hpp>
#include <filesystem>
#include <iomanip>


#ifdef ASTRAL_USE_IMGUI
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#endif

namespace AstralEngine {

SceneEditorSubsystem::SceneEditorSubsystem() {}

SceneEditorSubsystem::~SceneEditorSubsystem() {}

void SceneEditorSubsystem::OnInitialize(Engine *owner) {
  m_owner = owner;
  m_renderSubsystem = owner->GetSubsystem<RenderSubsystem>();
  m_assetSubsystem = owner->GetSubsystem<AssetSubsystem>();
  m_uiSubsystem = owner->GetSubsystem<UISubsystem>();

  m_activeScene = std::make_shared<Scene>();
  m_activeScene->OnInitialize(owner);

  InitializeDefaultResources();
  SetupViewportResources();

  // Register UI Draw Callback
  if (m_uiSubsystem) {
    m_uiSubsystem->RegisterDrawCallback([this]() { this->DrawUI(); });
  }

  // Subscribe to events
  EventManager::GetInstance().Subscribe<FileDropEvent>([this](Event &e) {
    auto &dropEvent = static_cast<FileDropEvent &>(e);
    this->HandleFileDrop(dropEvent);
  });

  // Initialize Panels
  // 1. Toolbar
  auto toolbar = std::make_unique<MainToolbarPanel>();
  m_panels.push_back(std::move(toolbar));

  // 2. Viewport
  auto viewport = std::make_unique<ViewportPanel>();
  viewport->SetRenderSubsystem(m_renderSubsystem);
  viewport->SetContext(m_activeScene);
  viewport->SetViewportTexture(m_viewportTexture, m_viewportDescriptorSet);

  viewport->SetResizeCallback(
      [this](uint32_t w, uint32_t h) { this->ResizeViewport(w, h); });

  m_viewportPanel = viewport.get();
  m_panels.push_back(std::move(viewport));

  // 3. Scene Hierarchy
  auto hierarchy = std::make_unique<SceneHierarchyPanel>();
  hierarchy->SetContext(m_activeScene);
  m_panels.push_back(std::move(hierarchy));

  // 4. Properties
  auto properties = std::make_unique<PropertiesPanel>();
  properties->SetContext(m_activeScene);
  m_propertiesPanel = properties.get();
  m_panels.push_back(std::move(properties));

  // 5. Content Browser
  auto contentBrowser = std::make_unique<ContentBrowserPanel>();
  m_panels.push_back(std::move(contentBrowser));

  // 6. Output Log
  auto outputLog = std::make_unique<OutputLogPanel>();
  m_outputLogPanel = outputLog.get();
  m_panels.push_back(std::move(outputLog));

  // Route Logger to Output Log
  Logger::SetLogCallback([this](Logger::LogLevel level,
                                const std::string &category,
                                const std::string &msg) {
    if (m_outputLogPanel) {
      std::string formatted = "[" + category + "] " + msg;
      m_outputLogPanel->AddLog(level, formatted);
    }
  });

  // Register Render Callback
  if (m_renderSubsystem) {
    m_renderSubsystem->SetPreRenderCallback(
        [this](IRHICommandList *cmdList) { this->RenderScene(cmdList); });
  }

  Logger::Info("SceneEditorSubsystem",
               "Scene Editor initialized with modular panels");
}

void SceneEditorSubsystem::OnUpdate(float deltaTime) {
  if (m_activeScene) {
    m_activeScene->OnUpdate(deltaTime);
  }
}

void SceneEditorSubsystem::OnShutdown() {
  Logger::Info("SceneEditorSubsystem", "Scene Editor Subsystem shutting down");

  m_activeScene.reset();
  m_panels.clear();

  if (m_viewportDescriptorSet) {
    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportDescriptorSet);
    m_viewportDescriptorSet = nullptr;
  }

  if (m_renderSubsystem) {
    m_renderSubsystem->SetPreRenderCallback(nullptr);
  }

  m_meshCache.clear();
  m_materialCache.clear();
}

void SceneEditorSubsystem::SetSelectedEntity(uint32_t entity) {
  m_selectedEntity = entity;
  if (m_propertiesPanel) {
    m_propertiesPanel->SetSelectedEntity(entity);
  }
}

void SceneEditorSubsystem::NewScene() {
  m_activeScene = std::make_shared<Scene>();
  m_activeScene->OnInitialize(m_owner);
  SetSelectedEntity((uint32_t)entt::null);

  // Update panel contexts
  for (auto &panel : m_panels) {
    if (panel->GetName() == "Level Viewport")
      static_cast<ViewportPanel *>(panel.get())->SetContext(m_activeScene);
    if (panel->GetName() == "World Outliner")
      static_cast<SceneHierarchyPanel *>(panel.get())
          ->SetContext(m_activeScene);
    if (panel->GetName() == "Details")
      static_cast<PropertiesPanel *>(panel.get())->SetContext(m_activeScene);
  }
}

bool SceneEditorSubsystem::SaveScene(const std::string &filename) {
  SceneSerializer serializer(m_activeScene.get());
  serializer.Serialize(filename);
  m_sceneModified = false;
  return true;
}

bool SceneEditorSubsystem::LoadScene(const std::string &filename) {
  if (!std::filesystem::exists(filename)) {
    Logger::Error("SceneEditorSubsystem", "Scene file does not exist: {}",
                  filename);
    return false;
  }

  auto newScene = std::make_shared<Scene>();
  newScene->OnInitialize(m_owner);

  SceneSerializer serializer(newScene.get());
  if (serializer.Deserialize(filename)) {
    m_activeScene = newScene;
    SetSelectedEntity((uint32_t)entt::null);

    // Update panel contexts
    for (auto &panel : m_panels) {
      if (panel->GetName() == "Level Viewport")
        static_cast<ViewportPanel *>(panel.get())->SetContext(m_activeScene);
      if (panel->GetName() == "World Outliner")
        static_cast<SceneHierarchyPanel *>(panel.get())
            ->SetContext(m_activeScene);
      if (panel->GetName() == "Details")
        static_cast<PropertiesPanel *>(panel.get())->SetContext(m_activeScene);
    }

    m_sceneModified = false;
    Logger::Info("SceneEditorSubsystem", "Successfully loaded scene: {}",
                 filename);
    return true;
  }

  return false;
}

void SceneEditorSubsystem::DrawUI() {
  RenderMainMenuBar();

  // Update viewport interaction states
  if (m_viewportPanel) {
    auto *platform = m_owner->GetSubsystem<PlatformSubsystem>();
    if (platform) {
      m_viewportPanel->SetDraggingFile(platform->GetWindow()->IsDraggingFile());
    }
  }

  // Create a fullscreen DockSpace
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("EditorDockSpace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

  if (!m_layoutInitialized) {
    ResetLayout();
    m_layoutInitialized = true;
  }

  ImGui::End();

  // Render all modular panels
  for (auto &panel : m_panels) {
    panel->OnDraw();
  }
}

void SceneEditorSubsystem::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene"))
        NewScene();
      if (ImGui::MenuItem("Open Scene...")) { /* Load logic */
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save Scene"))
        SaveScene("current.scene");
      ImGui::Separator();
      if (ImGui::MenuItem("Exit"))
        m_owner->RequestShutdown();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      for (auto &panel : m_panels) {
        bool open = panel->IsOpen();
        if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, &open)) {
          panel->SetOpen(open);
        }
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Layout"))
        m_layoutInitialized = false;
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void SceneEditorSubsystem::ResetLayout() {
  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  ImGuiID dock_main_id = dockspace_id;
  ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
  ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
  ImGuiID dock_id_right_top, dock_id_right_bottom;
  ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.4f,
                              &dock_id_right_top, &dock_id_right_bottom);

  ImGui::DockBuilderDockWindow("Level Viewport", dock_main_id);
  ImGui::DockBuilderDockWindow("World Outliner", dock_id_right_top);
  ImGui::DockBuilderDockWindow("Details", dock_id_right_bottom);
  ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
  ImGui::DockBuilderDockWindow("Output Log", dock_id_bottom);
  ImGui::DockBuilderDockWindow(
      "Main Toolbar",
      dock_main_id); // Toolbar usually stays on top but can dock

  ImGui::DockBuilderFinish(dockspace_id);
}

void SceneEditorSubsystem::InitializeDefaultResources() {
  IRHIDevice *device = m_renderSubsystem->GetDevice();
  if (!device)
    return;

  // 1. Create Global Descriptor Set Layout
  std::vector<RHIDescriptorSetLayoutBinding> bindings;
  RHIDescriptorSetLayoutBinding uboBinding{};
  uboBinding.binding = 0;
  uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
  uboBinding.descriptorCount = 1;
  uboBinding.stageFlags = RHIShaderStage::Vertex | RHIShaderStage::Fragment;
  bindings.push_back(uboBinding);
  m_globalDescriptorSetLayout = device->CreateDescriptorSetLayout(bindings);

  // 2. Create Global Uniform Buffers and Descriptor Sets
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_uniformBuffers.push_back(device->CreateBuffer(
        sizeof(GlobalUBO), RHIBufferUsage::Uniform,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent));

    auto set = device->AllocateDescriptorSet(m_globalDescriptorSetLayout.get());
    set->UpdateUniformBuffer(0, m_uniformBuffers[i].get(), 0,
                             sizeof(GlobalUBO));
    m_globalDescriptorSets.push_back(set);
  }

  // 3. Load Default Assets
  auto *assetManager = m_assetSubsystem->GetAssetManager();

  // Register defaults if they don't exist
  AssetHandle cubeHandle =
      assetManager->RegisterAsset("Models/Default/Cube.obj");
  AssetHandle whiteTexHandle =
      assetManager->RegisterAsset("Textures/Default/White.png");

  // We'll lazy load them in GetOrLoad functions
}

void SceneEditorSubsystem::SetupViewportResources() {
  ResizeViewport(1280, 720);
}

void SceneEditorSubsystem::ResizeViewport(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0 || width > 8192 || height > 8192)
    return;

  IRHIDevice *device = m_renderSubsystem->GetDevice();
  if (!device)
    return;

  device->WaitIdle();

  m_viewportTexture = device->CreateTexture2D(
      width, height, RHIFormat::B8G8R8A8_UNORM,
      (RHITextureUsage)(RHITextureUsage::ColorAttachment |
                        RHITextureUsage::Sampled));
  m_viewportDepth =
      device->CreateTexture2D(width, height, RHIFormat::D32_FLOAT,
                              RHITextureUsage::DepthStencilAttachment);

  auto vkTexture = std::dynamic_pointer_cast<VulkanTexture>(m_viewportTexture);
  if (vkTexture) {
    if (m_viewportDescriptorSet) {
      ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportDescriptorSet);
    }

    RHISamplerDescriptor samplerDesc = {};
    m_viewportSampler = device->CreateSampler(samplerDesc);
    auto vkSampler =
        std::dynamic_pointer_cast<VulkanSampler>(m_viewportSampler);

    if (vkSampler) {
      m_viewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
          vkSampler->GetVkSampler(), vkTexture->GetImageView(),
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Initial transition to SHADER_READ_ONLY_OPTIMAL so ImGui doesn't complain
    auto cmd = device->CreateCommandList();
    cmd->Begin();
    static_cast<VulkanCommandList *>(cmd.get())->TransitionImageLayout(
        vkTexture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    cmd->End();
    device->SubmitCommandList(cmd.get());
    device->WaitIdle();
  }

  if (m_viewportPanel) {
    m_viewportPanel->SetViewportTexture(m_viewportTexture,
                                        m_viewportDescriptorSet);
  }
}

void SceneEditorSubsystem::RenderScene(IRHICommandList *cmdList) {
  if (!m_viewportTexture || !m_viewportDepth || !m_viewportPanel ||
      !m_activeScene)
    return;
  if (m_globalDescriptorSets.empty())
    return;

  static bool firstRender = true;
  if (firstRender) {
    Logger::Info("SceneEditorSubsystem", "First RenderScene call starting...");
    firstRender = false;
  }

  IRHIDevice *device = m_renderSubsystem->GetDevice();
  if (!device)
    return;

  uint32_t frameIndex = device->GetCurrentFrameIndex();

  Camera *camera = m_viewportPanel->GetCamera();
  if (!camera)
    return;

  // 1. Update Global UBO
  GlobalUBO ubo{};
  ubo.view = camera->GetViewMatrix();
  ubo.proj = camera->GetProjectionMatrix(m_viewportPanel->GetSize().x /
                                         m_viewportPanel->GetSize().y);
  ubo.proj[1][1] *= -1; // Vulkan flip
  ubo.viewPos = glm::vec4(camera->GetPosition(), 1.0f);
  ubo.lightCount = 0; // No lights implementation yet

  // 2. Render Loop
  std::vector<IRHITexture *> colorAttachments = {m_viewportTexture.get()};
  RHIRect2D renderArea;
  renderArea.offset = {0, 0};
  renderArea.extent = {m_viewportTexture->GetWidth(),
                       m_viewportTexture->GetHeight()};

  cmdList->BeginRendering(colorAttachments, m_viewportDepth.get(), renderArea);

  auto view = m_activeScene->Reg().view<TransformComponent, RenderComponent>();
  for (auto entity : view) {
    const auto &transform = view.get<TransformComponent>(entity);
    const auto &render = view.get<RenderComponent>(entity);

    if (!render.visible)
      continue;

    auto mesh = GetOrLoadMesh(render.modelHandle);
    auto material = GetOrLoadMaterial(render.materialHandle);

    if (mesh && material) {
      // Update UBO with model matrix
      ubo.model =
          transform.GetLocalMatrix(); // Use world matrix if hierarchy solved
      if (m_activeScene->Reg().all_of<WorldTransformComponent>(entity)) {
        ubo.model =
            m_activeScene->Reg().get<WorldTransformComponent>(entity).Transform;
      }

      void *data = m_uniformBuffers[frameIndex]->Map();
      memcpy(data, &ubo, sizeof(GlobalUBO));
      m_uniformBuffers[frameIndex]->Unmap();

      cmdList->BindPipeline(material->GetPipeline());

      // Bind descriptor sets individually
      cmdList->BindDescriptorSet(material->GetPipeline(),
                                 m_globalDescriptorSets[frameIndex].get(), 0);
      cmdList->BindDescriptorSet(material->GetPipeline(),
                                 material->GetDescriptorSet(), 1);

      mesh->Draw(cmdList);
    }
  }

  cmdList->EndRendering();
}

std::shared_ptr<Mesh>
SceneEditorSubsystem::GetOrLoadMesh(const AssetHandle &handle) {
  if (!handle.IsValid())
    return nullptr;
  if (m_meshCache.count(handle))
    return m_meshCache[handle];

  auto *assetManager = m_assetSubsystem->GetAssetManager();
  auto modelData = assetManager->GetAsset<ModelData>(handle);

  if (modelData && modelData->IsValid()) {
    auto mesh =
        std::make_shared<Mesh>(m_renderSubsystem->GetDevice(), *modelData);
    m_meshCache[handle] = mesh;
    return mesh;
  }
  return nullptr;
}

std::shared_ptr<Material>
SceneEditorSubsystem::GetOrLoadMaterial(const AssetHandle &handle) {
  if (!handle.IsValid())
    return nullptr;
  if (m_materialCache.count(handle))
    return m_materialCache[handle];

  auto *assetManager = m_assetSubsystem->GetAssetManager();
  auto matData = assetManager->GetAsset<MaterialData>(handle);

  if (matData && matData->IsValid()) {
    std::string vPath = matData->vertexShaderPath;
    std::string fPath = matData->fragmentShaderPath;

    // Strip "Assets/" prefix if it exists because GetFullPath will add it again
    // via m_assetDirectory
    if (vPath.compare(0, 7, "Assets/") == 0)
      vPath = vPath.substr(7);
    if (fPath.compare(0, 7, "Assets/") == 0)
      fPath = fPath.substr(7);

    // Ensure we are using .spv files
    if (vPath.find(".spv") == std::string::npos)
      vPath += ".spv";
    if (fPath.find(".spv") == std::string::npos)
      fPath += ".spv";

    // Final resolution
    std::string vFullPath = assetManager->GetFullPath(vPath);
    std::string fFullPath = assetManager->GetFullPath(fPath);

    // Debug Log
    Logger::Info("SceneEditorSubsystem", "Loading Shaders:\n  {}\n  {}",
                 vFullPath, fFullPath);

    if (!std::filesystem::exists(vFullPath) ||
        !std::filesystem::exists(fFullPath)) {
      Logger::Error("SceneEditorSubsystem",
                    "Shader files not found: \n  {}\n  {}", vFullPath,
                    fFullPath);
      return nullptr;
    }

    try {
      MaterialData fixedData = *matData;
      fixedData.vertexShaderPath = vFullPath;
      fixedData.fragmentShaderPath = fFullPath;

      auto material =
          std::make_shared<Material>(m_renderSubsystem->GetDevice(), fixedData,
                                     m_globalDescriptorSetLayout.get());

      // Load textures if they exist
      if (!matData->texturePaths.empty()) {
        AssetHandle texHandle =
            assetManager->RegisterAsset(matData->texturePaths[0]);
        auto texData = assetManager->GetAsset<TextureData>(texHandle);
        if (texData && texData->IsValid()) {
          auto texture = std::make_shared<Texture>(
              m_renderSubsystem->GetDevice(), *texData);
          material->SetAlbedoMap(texture);
          material->UpdateDescriptorSet();
        }
      }

      m_materialCache[handle] = material;
      return material;
    } catch (const std::exception &e) {
      Logger::Error("SceneEditorSubsystem", "Failed to create material: {}",
                    e.what());
    }
  }
  return nullptr;
}

void SceneEditorSubsystem::ExecuteCommand(uint32_t type) { (void)type; }

void SceneEditorSubsystem::HandleFileDrop(FileDropEvent &e) {
  if (!m_viewportPanel ||
      !m_viewportPanel->IsPointOverViewport(e.GetX(), e.GetY())) {
    return;
  }

  Logger::Info("SceneEditorSubsystem", "Processing dropped file: {}",
               e.GetPath());

  auto *assetManager = m_assetSubsystem->GetAssetManager();
  // RegisterAsset handles absolute paths by normalizing them or using them
  // directly if they match extensions
  AssetHandle handle = assetManager->RegisterAsset(e.GetPath());

  if (!handle.IsValid()) {
    Logger::Warning("SceneEditorSubsystem",
                    "Unsupported file format or failed to register: {}",
                    e.GetPath());
    return;
  }

  if (handle.GetType() == AssetHandle::Type::Model) {
    // Create entity in active scene
    std::string name = std::filesystem::path(e.GetPath()).stem().string();
    Entity entity = m_activeScene->CreateEntity(name);

    // Add components
    auto &renderComp = entity.AddComponent<RenderComponent>();
    renderComp.modelHandle = handle;
    // Use a default material if we have one, otherwise it might be
    // uninitialized
    renderComp.materialHandle =
        assetManager->RegisterAsset("Materials/Default.amat");
    renderComp.visible = true;

    // Place it in front of camera or at origin
    auto *transform = entity.TryGetComponent<TransformComponent>();
    if (transform) {
      Camera *camera = m_viewportPanel->GetCamera();
      if (camera) {
        // Position it 5 units in front of the camera
        transform->position = camera->GetPosition() + camera->GetFront() * 5.0f;
        transform->scale = glm::vec3(1.0f);
        Logger::Info("SceneEditorSubsystem",
                     "Spawned at position: ({}, {}, {})", transform->position.x,
                     transform->position.y, transform->position.z);
      } else {
        transform->position = glm::vec3(0.0f);
        transform->scale = glm::vec3(1.0f);
      }
    }

    Logger::Info("SceneEditorSubsystem",
                 "Imported and spawned model '{}' from drop.", name);
  } else {
    Logger::Info("SceneEditorSubsystem",
                 "Dropped file '{}' registered as type {}, but no default "
                 "spawning logic exists.",
                 e.GetPath(), (int)handle.GetType());
  }
}

} // namespace AstralEngine
