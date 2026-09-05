#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Editor/EditorUI.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Scene/SceneCommands.hpp"
#include "Astral/Renderer/Swapchain.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <iostream>
#include <array>
#include <cstring>

namespace Astral {

EditorUI::EditorUI(VulkanContext& context, GLFWwindow* window, const InputSystem& input)
    : m_Context(context), m_Input(input), m_ViewportPanel(nullptr, &input) {
    InitImGui(window);
}

EditorUI::~EditorUI() {
    ShutdownImGui();
}

void EditorUI::InitImGui(GLFWwindow* window) {
    // 1. ImGui icin Vulkan Descriptor Pool
    vk::DescriptorPoolSize poolSizes[] = {
        { vk::DescriptorType::eSampler, 100 },
        { vk::DescriptorType::eCombinedImageSampler, 100 },
        { vk::DescriptorType::eSampledImage, 100 },
        { vk::DescriptorType::eStorageImage, 100 },
        { vk::DescriptorType::eUniformTexelBuffer, 100 },
        { vk::DescriptorType::eStorageTexelBuffer, 100 },
        { vk::DescriptorType::eUniformBuffer, 100 },
        { vk::DescriptorType::eStorageBuffer, 100 },
        { vk::DescriptorType::eUniformBufferDynamic, 100 },
        { vk::DescriptorType::eStorageBufferDynamic, 100 },
        { vk::DescriptorType::eInputAttachment, 100 }
    };

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    m_ImguiPool = m_Context.GetDevice().createDescriptorPoolUnique(poolInfo);

    // 2. ImGui Core Init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // ★ Docking Aktif!

    // 3. Astral Engine Photoshop 2026 Theme & Fonts
    ApplyAstralTheme();
    InitEditorFonts(io);

    // 4. GLFW Backend Init
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 5. Vulkan Backend Init (Vulkan 1.4 Dynamic Rendering)
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = static_cast<VkInstance>(m_Context.GetInstance());
    initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(m_Context.GetPhysicalDevice());
    initInfo.Device = static_cast<VkDevice>(m_Context.GetDevice());
    initInfo.QueueFamily = m_Context.GetQueueFamilies().graphicsFamily;
    initInfo.Queue = static_cast<VkQueue>(m_Context.GetGraphicsQueue());
    initInfo.DescriptorPool = static_cast<VkDescriptorPool>(m_ImguiPool.get());
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = m_Context.GetSwapchain() ? static_cast<uint32_t>(m_Context.GetSwapchain()->GetImages().size()) : 2;
    initInfo.UseDynamicRendering = true;

    VkFormat colorFormat = m_Context.GetSwapchain() ? static_cast<VkFormat>(m_Context.GetSwapchain()->GetFormat()) : VK_FORMAT_B8G8R8A8_UNORM;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    initInfo.CheckVkResultFn = [](VkResult err) {
        if (err != 0) {
            std::cerr << "[Astral::EditorUI] ImGui Vulkan Hatasi: " << static_cast<int>(err) << "\n";
        }
    };

    struct ImGuiLoaderData {
        VkInstance instance;
        VkDevice device;
    } loaderData{
        static_cast<VkInstance>(m_Context.GetInstance()),
        static_cast<VkDevice>(m_Context.GetDevice())
    };

    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
        auto* data = static_cast<ImGuiLoaderData*>(user_data);

        // Instance / Physical Device functions must be queried via vkGetInstanceProcAddr
        // to avoid Vulkan validation warnings about querying instance-level functions via vkGetDeviceProcAddr.
        if (std::strncmp(function_name, "vkGetPhysicalDevice", 19) == 0 ||
            std::strcmp(function_name, "vkDestroySurfaceKHR") == 0 ||
            std::strcmp(function_name, "vkEnumeratePhysicalDevices") == 0) {
            return vkGetInstanceProcAddr(data->instance, function_name);
        }

        if (std::strcmp(function_name, "vkCmdBeginRenderingKHR") == 0) {
            auto pfn = vkGetDeviceProcAddr(data->device, "vkCmdBeginRenderingKHR");
            if (!pfn) pfn = vkGetDeviceProcAddr(data->device, "vkCmdBeginRendering");
            if (!pfn) pfn = vkGetInstanceProcAddr(data->instance, "vkCmdBeginRendering");
            return pfn;
        }
        if (std::strcmp(function_name, "vkCmdEndRenderingKHR") == 0) {
            auto pfn = vkGetDeviceProcAddr(data->device, "vkCmdEndRenderingKHR");
            if (!pfn) pfn = vkGetDeviceProcAddr(data->device, "vkCmdEndRendering");
            if (!pfn) pfn = vkGetInstanceProcAddr(data->instance, "vkCmdEndRendering");
            return pfn;
        }

        auto pfn = vkGetDeviceProcAddr(data->device, function_name);
        if (!pfn) {
            pfn = vkGetInstanceProcAddr(data->instance, function_name);
        }
        return pfn;
    }, &loaderData);

    ImGui_ImplVulkan_Init(&initInfo);

    // 6. Dynamic rendering fonksiyon isaretcileri
    m_PfnCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
        m_Context.GetDevice().getProcAddr("vkCmdBeginRendering"));
    m_PfnCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
        m_Context.GetDevice().getProcAddr("vkCmdEndRendering"));

    std::cout << "[Astral::EditorUI] Dear ImGui Vulkan 1.4 Dynamic Rendering + Docking basariyla baslatildi.\n";
}

void EditorUI::ShutdownImGui() {
    m_Context.WaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ImguiPool.reset();
}

void EditorUI::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void EditorUI::SetupDockSpace(Scene& scene, Entity& selectedEntity) {
    // Sanitize selected entity across scene reload/clear/destruction
    if (selectedEntity.GetHandle() != NullEntityHandle && !selectedEntity.IsValid()) {
        selectedEntity = Entity();
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    float statusBarHeight = 24.0f;
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    workSize.y -= statusBarHeight;

    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(workSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("AstralEditorWorkspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // Create DockSpace
    ImGuiID dockspace_id = ImGui::GetID("AstralEngineDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Setup default layout (only runs on first frame or when reset is requested)
    SetupDefaultEditorLayout(dockspace_id, m_ResetLayout);
    if (m_ResetLayout) {
        m_ResetLayout = false;
    }

    // Draw Menu Bar
    MenuBarActions actions{};
    DrawEditorMenuBar(scene, selectedEntity, actions, m_ShowDemoWindow, m_Input, m_CommandStack);

    // Process menu bar actions
    if (actions.resetLayout) {
        m_ResetLayout = true;
    }
    if (actions.newScene || actions.openScene) {
        selectedEntity = Entity();
        m_CommandStack.Clear();
    }
    if (actions.exitApp) {
        // Signal exit via GLFW (Application polls ShouldClose)
    }
    if (actions.deleteSelected && selectedEntity.IsValid()) {
        m_CommandStack.PushAndExecute(std::make_unique<DeleteEntityCommand>(scene, selectedEntity, &selectedEntity));
    }
    if (actions.addSphere || actions.addBox || actions.addTorus || actions.addCylinder || actions.addPlane) {
        uint32_t primType = 0;
        std::string name = "Sphere";
        glm::vec3 albedo(0.85f, 0.35f, 0.15f);
        if (actions.addSphere)   { primType = 0; name = "Sphere";   albedo = glm::vec3(0.9f, 0.25f, 0.2f); }
        if (actions.addBox)      { primType = 1; name = "Box";      albedo = glm::vec3(0.2f, 0.5f, 0.9f); }
        if (actions.addTorus)    { primType = 2; name = "Torus";    albedo = glm::vec3(0.9f, 0.75f, 0.15f); }
        if (actions.addCylinder) { primType = 3; name = "Cylinder"; albedo = glm::vec3(0.3f, 0.8f, 0.4f); }
        if (actions.addPlane)    { primType = 4; name = "Plane";    albedo = glm::vec3(0.3f, 0.32f, 0.35f); }

        TransformComponent t(
            glm::vec3(0.0f, 0.8f, 0.0f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(0.5f)
        );
        SDFComponent s(
            primType, 3u, 0.25f, 1u, albedo, 0.3f, 0.5f
        );

        m_CommandStack.PushAndExecute(std::make_unique<CreateEntityCommand>(scene, name, t, s, &selectedEntity));
    }
    if (actions.clearScene) {
        auto& transforms = scene.GetRegistry().GetView<TransformComponent>();
        std::vector<EntityID> toDestroy;
        for (auto&& [entityId, transform] : transforms) {
            toDestroy.push_back(entityId);
        }
        for (auto id : toDestroy) {
            scene.DestroyEntity(id);
        }
        selectedEntity = Entity();
        m_CommandStack.Clear();
    }
    if (actions.playToggle && onPlayToggle) {
        onPlayToggle();
    }
    if (actions.pauseToggle && onPauseToggle) {
        onPauseToggle();
    }
    if (actions.stopPlay && onStopPlay) {
        onStopPlay();
    }

    ImGui::End();
}

void EditorUI::RenderPanels(Scene& scene, Entity& selectedEntity, float gpuTimeMs, float cpuTimeMs) {
    // 1. Setup DockSpace + Menu Bar
    SetupDockSpace(scene, selectedEntity);

    // 2. Draw all modular panels
    m_SceneHierarchy.Draw(scene, selectedEntity, &m_CommandStack);
    m_Inspector.Draw(scene, selectedEntity);

    size_t activeCount = scene.GetRegistry().GetView<TransformComponent>().Size();
    m_Statistics.Draw(gpuTimeMs, cpuTimeMs, activeCount);
    m_ViewportPanel.Draw(scene, selectedEntity);
    m_ContentBrowser.Draw();

    // 3. Draw Status Bar (fixed at bottom)
    StatusBarInfo statusInfo;
    statusInfo.gpuTimeMs = gpuTimeMs;
    statusInfo.cpuTimeMs = cpuTimeMs;
    statusInfo.entityCount = activeCount;
    statusInfo.gridEnabled = true;  // Could be connected to AppConfig
    statusInfo.taaEnabled = true;
    DrawEditorStatusBar(statusInfo);

    // 4. ImGui Demo Window
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }
}

void EditorUI::EndFrame(vk::CommandBuffer cmd, vk::ImageView swapchainImageView, vk::Extent2D extent) {
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData) return;

    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = static_cast<VkImageView>(swapchainImageView);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Temiz koyu editor arka plani
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { { 0.118f, 0.118f, 0.118f, 1.0f } };

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = VkRect2D{ {0, 0}, {extent.width, extent.height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    if (m_PfnCmdBeginRendering && m_PfnCmdEndRendering) {
        m_PfnCmdBeginRendering(static_cast<VkCommandBuffer>(cmd), &renderingInfo);
        ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(cmd));
        m_PfnCmdEndRendering(static_cast<VkCommandBuffer>(cmd));
    }
}

bool EditorUI::WantsCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool EditorUI::WantsCaptureKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace Astral
