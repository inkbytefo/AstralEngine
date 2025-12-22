#include "UISubsystem.h"
#include "../../Core/Engine.h"
#include "../Platform/PlatformSubsystem.h"
#include "../Renderer/Core/RenderSubsystem.h"
#include "../Renderer/RHI/Vulkan/VulkanDevice.h"
#include "../Editor/SceneEditorSubsystem.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

UISubsystem::UISubsystem() = default;

UISubsystem::~UISubsystem() {
    if (m_initialized) {
        OnShutdown();
    }
}

void UISubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("UISubsystem", "Initializing UI Subsystem...");
    // Defer ImGui initialization to first update to ensure RenderSubsystem is ready
    // InitializeImGui(); 
    m_initialized = false; 
    Logger::Info("UISubsystem", "UI Subsystem Initialized (Deferred ImGui Init).");

    // Integrate with SceneEditorSubsystem
    SceneEditorSubsystem* sceneEditor = m_owner->GetSubsystem<SceneEditorSubsystem>();
    if (sceneEditor) {
        Logger::Info("UISubsystem", "SceneEditorSubsystem integration established.");
        // Scene editor integration points can be added here
        // For example: sceneEditor->SetUIIntegrationPoint(this);
    } else {
        Logger::Warning("UISubsystem", "SceneEditorSubsystem not available for integration.");
    }
}

void UISubsystem::OnUpdate(float /*deltaTime*/) {
    if (!m_initialized) {
        InitializeImGui();
        if (!m_initialized) return;
    }

    BeginFrame();
    
    // Example UI Window (can be removed later)
    ImGui::ShowDemoWindow();
    
    // Custom Engine Stats Window
    if (ImGui::Begin("Astral Engine Stats")) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Separator();
        // Add more stats here later (memory, draw calls, etc.)
    }
    ImGui::End();

    EndFrame();
}

void UISubsystem::OnShutdown() {
    if (!m_initialized) return;
    Logger::Info("UISubsystem", "Shutting Down UI Subsystem...");
    ShutdownImGui();
    m_initialized = false;
    m_owner = nullptr;
    Logger::Info("UISubsystem", "UI Subsystem Shutdown.");
}

void UISubsystem::Render(VkCommandBuffer commandBuffer) {
#ifdef ASTRAL_USE_IMGUI
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->CmdListsCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
#endif
}

void UISubsystem::ProcessSDLEvent(const void* event) {
#ifdef ASTRAL_USE_IMGUI
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(event));
#endif
}

void UISubsystem::InitializeImGui() {
#ifdef ASTRAL_USE_IMGUI
    // 1. Create ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // 2. Setup Style
    SetupStyle();

    // 3. Get dependencies
    auto* platform = m_owner->GetSubsystem<PlatformSubsystem>();
    auto* renderer = m_owner->GetSubsystem<RenderSubsystem>();
    if (!platform || !renderer) {
        Logger::Critical("UISubsystem", "Platform or Render subsystem not available!");
        return;
    }
    auto* window = platform->GetWindow();
    
    // Use VulkanDevice concrete type for backend access
    auto* rhiDevice = renderer->GetDevice();
    auto* vulkanDevice = static_cast<VulkanDevice*>(rhiDevice);
    
    if (!window || !vulkanDevice) {
        Logger::Critical("UISubsystem", "Window or GraphicsDevice not available!");
        return;
    }

    // Establish the critical connection between Window and UISubsystem
    window->SetUISubsystem(this);
    m_window = window;  // Store window pointer for later use

    // 4. Create Descriptor Pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(vulkanDevice->GetVkDevice(), &pool_info, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        Logger::Error("UISubsystem", "Failed to create descriptor pool for ImGui!");
        return;
    }

    // 4.1 Create Dedicated Command Pool for ImGui
    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_info.queueFamilyIndex = vulkanDevice->GetGraphicsQueueFamilyIndex();
    if (vkCreateCommandPool(vulkanDevice->GetVkDevice(), &cmd_pool_info, nullptr, &m_uiCommandPool) != VK_SUCCESS) {
        Logger::Error("UISubsystem", "Failed to create command pool for ImGui!");
        return;
    }

    // 5. Initialize Backends
    ImGui_ImplSDL3_InitForVulkan(window->GetSDLWindow());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanDevice->GetInstance();
    init_info.PhysicalDevice = vulkanDevice->GetPhysicalDevice();
    init_info.Device = vulkanDevice->GetVkDevice();
    init_info.QueueFamily = vulkanDevice->GetGraphicsQueueFamilyIndex();
    init_info.Queue = vulkanDevice->GetGraphicsQueue();
    init_info.DescriptorPool = m_descriptorPool;
    // init_info.CommandPool = m_uiCommandPool; // Removed: Not supported in this ImGui version
    init_info.MinImageCount = 2;
    init_info.ImageCount = vulkanDevice->GetSwapchainImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA for UI pass
    
    // Dynamic Rendering Setup
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = {};
    init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    
    // We need static storage for the format because pColorAttachmentFormats is a pointer
    static VkFormat colorFormat; 
    colorFormat = vulkanDevice->GetSwapchainImageFormat();
    
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = vulkanDevice->GetDepthFormat();
    
    init_info.RenderPass = VK_NULL_HANDLE; 

    ImGui_ImplVulkan_Init(&init_info); // Argument removed in newer ImGui

    // 6. Upload Fonts
    // Ensure we have a clean state before uploading fonts
    vkDeviceWaitIdle(vulkanDevice->GetVkDevice());
    
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        Logger::Error("UISubsystem", "Failed to create fonts texture!");
    }

    // Wait for the upload to finish before proceeding
    vkDeviceWaitIdle(vulkanDevice->GetVkDevice());

    m_initialized = true;
    Logger::Info("UISubsystem", "ImGui Initialized with SDL3 and Vulkan backends.");
#endif
}

void UISubsystem::ShutdownImGui() {
#ifdef ASTRAL_USE_IMGUI
    if (!m_owner) return;
    auto* renderer = m_owner->GetSubsystem<RenderSubsystem>();
    if (!renderer) return;

    auto* rhiDevice = renderer->GetDevice();
    if (rhiDevice) {
         rhiDevice->WaitIdle();
         
         ImGui_ImplVulkan_Shutdown();
         ImGui_ImplSDL3_Shutdown();
         ImGui::DestroyContext();
         
         // Use concrete type for destroying descriptor pool
         auto* vulkanDevice = static_cast<VulkanDevice*>(rhiDevice);
         if (m_descriptorPool != VK_NULL_HANDLE) {
             vkDestroyDescriptorPool(vulkanDevice->GetVkDevice(), m_descriptorPool, nullptr);
             m_descriptorPool = VK_NULL_HANDLE;
         }
         if (m_uiCommandPool != VK_NULL_HANDLE) {
             vkDestroyCommandPool(vulkanDevice->GetVkDevice(), m_uiCommandPool, nullptr);
             m_uiCommandPool = VK_NULL_HANDLE;
         }
    }
#endif
}

void UISubsystem::BeginFrame() {
#ifdef ASTRAL_USE_IMGUI
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif
}

void UISubsystem::EndFrame() {
#ifdef ASTRAL_USE_IMGUI
    ImGui::Render();
#endif
}

void UISubsystem::SetupStyle() {
#ifdef ASTRAL_USE_IMGUI
    ImGui::StyleColorsDark();
    // Optional: Add custom styling here.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
#endif
}

} // namespace AstralEngine
