#include "UISubsystem.h"
#include "../../Core/Engine.h"
#include "../Platform/PlatformSubsystem.h"
#include "../Renderer/RenderSubsystem.h"
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
    InitializeImGui();
    m_initialized = true;
    Logger::Info("UISubsystem", "UI Subsystem Initialized.");

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

void UISubsystem::OnUpdate(float deltaTime) {
    if (!m_initialized) return;

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
    ImGui_ImplSDL3_ProcessEvent(event);
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
    auto* graphicsDevice = renderer->GetGraphicsDevice();
    if (!window || !graphicsDevice) {
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
    if (vkCreateDescriptorPool(graphicsDevice->GetDevice(), &pool_info, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        Logger::Error("UISubsystem", "Failed to create descriptor pool for ImGui!");
        return;
    }

    // 5. Initialize Backends
    ImGui_ImplSDL3_InitForVulkan(window->GetSDLWindow());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = graphicsDevice->GetInstance();
    init_info.PhysicalDevice = graphicsDevice->GetPhysicalDevice();
    init_info.Device = graphicsDevice->GetDevice();
    init_info.QueueFamily = graphicsDevice->GetGraphicsQueueFamily();
    init_info.Queue = graphicsDevice->GetGraphicsQueue();
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = graphicsDevice->GetSwapchain()->GetImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA for UI pass
    // This render pass must be compatible with the one used by RenderSubsystem to render the UI.
    // We will create this in RenderSubsystem later.
    ImGui_ImplVulkan_Init(&init_info, renderer->GetUIRenderPass());

    // 6. Upload Fonts
    VkCommandBuffer cmd = graphicsDevice->GetVulkanDevice()->BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    graphicsDevice->GetVulkanDevice()->EndSingleTimeCommands(cmd);

    Logger::Info("UISubsystem", "ImGui Initialized with SDL3 and Vulkan backends.");
#endif
}

void UISubsystem::ShutdownImGui() {
#ifdef ASTRAL_USE_IMGUI
    auto* graphicsDevice = m_owner->GetSubsystem<RenderSubsystem>()->GetGraphicsDevice();
    vkDeviceWaitIdle(graphicsDevice->GetDevice());
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(graphicsDevice->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
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

void UISubsystem::ShowDebugWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    if (open && !*open) return;

    if (ImGui::Begin("Astral Engine Debug", open)) {
        ImGui::Text("Astral Engine Debug Information");
        ImGui::Separator();

        // Engine State
        ImGui::Text("Engine State:");
        ImGui::Text("  Initialized: %s", m_initialized ? "Yes" : "No");
        ImGui::Text("  Owner: %s", m_owner ? "Valid" : "Null");

        ImGui::Separator();

        // Subsystem Information
        ImGui::Text("UI Subsystem State:");
        ImGui::Text("  Show Demo: %s", m_showDemo ? "Yes" : "No");
        ImGui::Text("  Show Metrics: %s", m_showMetrics ? "Yes" : "No");
        ImGui::Text("  Show Debug: %s", m_showDebugWindow ? "Yes" : "No");

        ImGui::Separator();

        // ImGui State
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("ImGui State:");
        ImGui::Text("  Framerate: %.1f FPS", io.Framerate);
        ImGui::Text("  Frame Time: %.3f ms", 1000.0f / io.Framerate);
        ImGui::Text("  Display Size: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("  Mouse Pos: %.1f, %.1f", io.MousePos.x, io.MousePos.y);

        ImGui::Separator();

        // Controls
        if (ImGui::Button("Show Demo Window")) {
            m_showDemo = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Show Metrics Window")) {
            m_showMetrics = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All Windows")) {
            m_showDemo = false;
            m_showMetrics = false;
            m_showDebugWindow = false;
        }

        ImGui::Separator();

        // Memory Information (if available)
        if (ImGui::CollapsingHeader("Memory Information")) {
            ImGui::Text("Memory stats would be displayed here");
            ImGui::Text("when memory tracking is implemented.");
        }

        // Performance Information (if available)
        if (ImGui::CollapsingHeader("Performance")) {
            ImGui::Text("Performance metrics would be displayed here");
            ImGui::Text("when performance tracking is implemented.");
        }
    }
    ImGui::End();
#endif
}

void UISubsystem::ShowMetricsWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    if (open && !*open) return;

    if (ImGui::Begin("Astral Engine Metrics", open)) {
        ImGui::Text("Astral Engine Performance Metrics");
        ImGui::Separator();

        // Frame Metrics
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Frame Performance:");
        ImGui::Text("  FPS: %.1f", io.Framerate);
        ImGui::Text("  Frame Time: %.3f ms", 1000.0f / io.Framerate);
        ImGui::Text("  Average Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

        ImGui::Separator();

        // ImGui Metrics
        ImGui::Text("ImGui Metrics:");
        ImGui::Text("  Vertices: %d", ImGui::GetDrawData()->TotalVtxCount);
        ImGui::Text("  Indices: %d", ImGui::GetDrawData()->TotalIdxCount);
        ImGui::Text("  Draw Lists: %d", ImGui::GetDrawData()->CmdListsCount);

        ImGui::Separator();

        // Memory Usage (approximate)
        if (ImGui::CollapsingHeader("Memory Usage")) {
            ImGui::Text("Approximate Memory Usage:");
            ImGui::Text("  ImGui Context: ~%d KB", 1024); // Rough estimate
            ImGui::Text("  Font Atlas: ~%d KB", 512); // Rough estimate
            ImGui::Text("  UI Subsystem: ~%d KB", 256); // Rough estimate
        }

        // Render Statistics
        if (ImGui::CollapsingHeader("Render Statistics")) {
            ImGui::Text("Render statistics would be displayed here");
            ImGui::Text("when render tracking is implemented.");
        }
    }
    ImGui::End();
#endif
}

void UISubsystem::ShowDemoWindow(bool* open) {
#ifdef ASTRAL_USE_IMGUI
    if (open && !*open) return;

    // Use ImGui's built-in demo window
    ImGui::ShowDemoWindow(open);
#endif
}

} // namespace AstralEngine
