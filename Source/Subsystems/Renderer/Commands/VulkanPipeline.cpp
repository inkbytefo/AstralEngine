#include "VulkanPipeline.h"
#include "../../../Core/Logger.h"
#include "../RendererTypes.h"
#include <chrono>

namespace AstralEngine {

VulkanPipeline::VulkanPipeline() 
    : m_device(nullptr)
    , m_pipeline(VK_NULL_HANDLE)
    , m_pipelineLayout(VK_NULL_HANDLE)
    , m_isInitialized(false) {
    
    Logger::Debug("VulkanPipeline", "VulkanPipeline created");
}

VulkanPipeline::~VulkanPipeline() {
    if (m_isInitialized) {
        Shutdown();
    }
    Logger::Debug("VulkanPipeline", "VulkanPipeline destroyed");
}

bool VulkanPipeline::Initialize(VulkanDevice* device, const Config& config) {
    if (m_isInitialized) {
        Logger::Warning("VulkanPipeline", "VulkanPipeline already initialized");
        return true;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    if (!config.swapchain) {
        SetError("Invalid swapchain pointer");
        return false;
    }
    
    if (config.shaders.empty()) {
        SetError("No shaders provided");
        return false;
    }
    
    m_device = device;
    m_config = config;
    
    Logger::Info("VulkanPipeline", "Initializing Vulkan pipeline with {} shaders", 
                config.shaders.size());
    Logger::Info("VulkanPipeline", "Received descriptorSetLayout in config: {}", (void*)config.descriptorSetLayout);
    
    try {
        // Pipeline layout oluştur
        if (!CreatePipelineLayout()) {
            return false;
        }
        
        Logger::Debug("VulkanPipeline", "Pipeline layout created successfully");
        
        // Graphics pipeline oluştur
        if (!CreateGraphicsPipeline()) {
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanPipeline", "Vulkan pipeline initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during pipeline initialization: ") + e.what());
        Logger::Error("VulkanPipeline", "Pipeline initialization failed: {}", e.what());
        return false;
    }
}

void VulkanPipeline::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    Logger::Info("VulkanPipeline", "Shutting down Vulkan pipeline");
    
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device->GetDevice(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
        Logger::Debug("VulkanPipeline", "Pipeline destroyed");
    }
    
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device->GetDevice(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
        Logger::Debug("VulkanPipeline", "Pipeline layout destroyed");
    }
    
    m_device = nullptr;
    m_config = {};
    m_lastError.clear();
    m_isInitialized = false;
    
    Logger::Info("VulkanPipeline", "Pipeline shutdown completed");
}

bool VulkanPipeline::CreatePipelineLayout() {
    Logger::Debug("VulkanPipeline", "Creating pipeline layout");
    Logger::Info("VulkanPipeline", "Checking m_config.descriptorSetLayout in CreatePipelineLayout: {}", (void*)m_config.descriptorSetLayout);
    
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    // Descriptor set layout kullan
    if (m_config.descriptorSetLayout != VK_NULL_HANDLE) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &m_config.descriptorSetLayout;
        Logger::Debug("VulkanPipeline", "Using descriptor set layout for pipeline layout");
    } else {
        layoutInfo.setLayoutCount = 0;
        layoutInfo.pSetLayouts = nullptr;
        Logger::Debug("VulkanPipeline", "Creating pipeline layout without descriptor sets");
    }
    
    // Define push constant range for the model matrix
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Accessible in vertex shader
    pushConstantRange.offset = 0; // Start at the beginning of the push constant block
    pushConstantRange.size = sizeof(glm::mat4); // Size for the model matrix

    layoutInfo.pushConstantRangeCount = 1;   // Enable push constants
    layoutInfo.pPushConstantRanges = &pushConstantRange; // Point to the defined range
    
    VkResult result = vkCreatePipelineLayout(m_device->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to create pipeline layout, VkResult: " + std::to_string(result));
        Logger::Error("VulkanPipeline", "Failed to create pipeline layout: {}", static_cast<int32_t>(result));
        return false;
    }
    
    Logger::Debug("VulkanPipeline", "Pipeline layout created successfully");
    return true;
}

bool VulkanPipeline::CreateGraphicsPipeline() {
    Logger::Info("VulkanPipeline", "Creating graphics pipeline");
    
    try {
        // Shader aşamalarını oluştur
        Logger::Debug("VulkanPipeline", "Creating shader stages...");
        auto shaderStages = CreateShaderStages();
        Logger::Info("VulkanPipeline", "Shader stages created: {} stages", shaderStages.size());
        
        // Vertex input state
        Logger::Debug("VulkanPipeline", "Creating vertex input state...");
        auto bindingDescription = Vertex::GetBindingDescription();
        auto attributeDescriptions = Vertex::GetAttributeDescriptions();
        auto vertexInputInfo = CreateVertexInputState(bindingDescription, attributeDescriptions);
        Logger::Debug("VulkanPipeline", "Vertex input state created");

        // Input assembly state
        Logger::Debug("VulkanPipeline", "Creating input assembly state...");
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // Üçgen listeleri
        inputAssembly.primitiveRestartEnable = VK_FALSE;  // Primitive restart devre dışı
        Logger::Debug("VulkanPipeline", "Input assembly state created (TRIANGLE_LIST)");
        
        // Viewport ve scissor (dynamic state)
        Logger::Debug("VulkanPipeline", "Setting up dynamic states...");
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        Logger::Debug("VulkanPipeline", "Dynamic states configured: {} states", dynamicStates.size());
        
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        
        // Viewport state
        Logger::Debug("VulkanPipeline", "Creating viewport state...");
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr;  // Dynamic state
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;  // Dynamic state
        Logger::Debug("VulkanPipeline", "Viewport state created (dynamic)");
        
        // Rasterization state
        Logger::Debug("VulkanPipeline", "Creating rasterization state...");
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;           // Depth clamping devre dışı
        rasterizer.rasterizerDiscardEnable = VK_FALSE;    // Rasterization'ı devre dışı bırakma
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;    // Üçgenleri doldur
        rasterizer.lineWidth = 1.0f;                      // Çizgi kalınlığı
        rasterizer.cullMode = VK_CULL_MODE_NONE;          // Culling devre dışı - güvenlik önlemi
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // Vertex sırası (saat yönünün tersi) - vertex verisiyle tutarlı
        rasterizer.depthBiasEnable = VK_FALSE;            // Depth bias devre dışı
        Logger::Debug("VulkanPipeline", "Rasterization state created (FILL, NO_CULL, COUNTER_CLOCKWISE)");
        
        // Multisampling state
        Logger::Debug("VulkanPipeline", "Creating multisample state...");
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;     // Sample shading devre dışı
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // Anti-aliasing yok
        multisampling.minSampleShading = 1.0f;           // Minimum sample shading
        multisampling.pSampleMask = nullptr;              // Sample mask
        multisampling.alphaToCoverageEnable = VK_FALSE;    // Alpha to coverage devre dışı
        multisampling.alphaToOneEnable = VK_FALSE;        // Alpha to one devre dışı
        Logger::Debug("VulkanPipeline", "Multisample state created (1 sample)");
        
        // Depth stencil state
        Logger::Debug("VulkanPipeline", "Creating depth stencil state...");
        VkPipelineDepthStencilStateCreateInfo depthStencil = CreateDepthStencilState();
        Logger::Debug("VulkanPipeline", "Depth stencil state created using CreateDepthStencilState()");
        
        // Color blending state
        Logger::Debug("VulkanPipeline", "Creating color blend state...");
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;     // Blending devre dışı
        
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;          // Logic operations devre dışı
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;          // Blend constants
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;
        Logger::Debug("VulkanPipeline", "Color blend state created (no blending)");
        
        // Graphics pipeline create info
        Logger::Info("VulkanPipeline", "Assembling pipeline create info...");
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_config.swapchain->GetRenderPass();
        pipelineInfo.subpass = 0;  // İlk subpass
        
        Logger::Info("VulkanPipeline", "Pipeline layout: {}", reinterpret_cast<uintptr_t>(m_pipelineLayout));
        Logger::Info("VulkanPipeline", "Render pass: {}", reinterpret_cast<uintptr_t>(m_config.swapchain->GetRenderPass()));
        
        // Pipeline türevleri (isteğe bağlı)
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;
        
        Logger::Info("VulkanPipeline", "Creating Vulkan graphics pipeline...");
        
        // Pipeline oluşturmadan önce son kontrol
        Logger::Info("VulkanPipeline", "Pipeline create info validation:");
        Logger::Info("VulkanPipeline", "  - Stage count: {}", pipelineInfo.stageCount);
        Logger::Info("VulkanPipeline", "  - Layout: {}", reinterpret_cast<uintptr_t>(pipelineInfo.layout));
        Logger::Info("VulkanPipeline", "  - Render pass: {}", reinterpret_cast<uintptr_t>(pipelineInfo.renderPass));
        Logger::Info("VulkanPipeline", "  - Subpass: {}", pipelineInfo.subpass);
        
        // Shader modüllerini kontrol et
        for (uint32_t i = 0; i < pipelineInfo.stageCount; i++) {
            Logger::Info("VulkanPipeline", "  - Shader stage {}: module = {}", i, reinterpret_cast<uintptr_t>(pipelineInfo.pStages[i].module));
        }
        
        // Device kontrolü
        VkDevice device = m_device->GetDevice();
        Logger::Info("VulkanPipeline", "Device handle: {}", reinterpret_cast<uintptr_t>(device));
        
        try {
            Logger::Info("VulkanPipeline", "About to call vkCreateGraphicsPipelines...");
            Logger::Info("VulkanPipeline", "Pre-validation checks:");
            Logger::Info("VulkanPipeline", "  - Device handle: {}", reinterpret_cast<uintptr_t>(device));
            Logger::Info("VulkanPipeline", "  - Pipeline layout: {}", reinterpret_cast<uintptr_t>(pipelineInfo.layout));
            Logger::Info("VulkanPipeline", "  - Render pass: {}", reinterpret_cast<uintptr_t>(pipelineInfo.renderPass));
            Logger::Info("VulkanPipeline", "  - Shader stage count: {}", pipelineInfo.stageCount);
            
            // Validate shader modules before creation
            for (uint32_t i = 0; i < pipelineInfo.stageCount; i++) {
                Logger::Info("VulkanPipeline", "  - Shader stage {}: module = {} (stage = {})",
                           i, reinterpret_cast<uintptr_t>(pipelineInfo.pStages[i].module),
                           static_cast<uint32_t>(pipelineInfo.pStages[i].stage));
                
                if (pipelineInfo.pStages[i].module == VK_NULL_HANDLE) {
                    Logger::Error("VulkanPipeline", "ERROR: Shader stage {} has NULL module!", i);
                    SetError("Shader stage " + std::to_string(i) + " has NULL module");
                    return false;
                }
            }
            
            // Pipeline oluştur with timeout protection
            Logger::Info("VulkanPipeline", "Calling vkCreateGraphicsPipelines now...");
            
            // Add timeout mechanism to prevent hanging
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Logger::Info("VulkanPipeline", "vkCreateGraphicsPipelines took {} ms", duration);
            
            if (duration > 5000) { // 5 second timeout warning
                Logger::Warning("VulkanPipeline", "Pipeline creation took unusually long ({} ms) - possible driver issue", duration);
            }
            
            Logger::Info("VulkanPipeline", "vkCreateGraphicsPipelines returned: {}", static_cast<int32_t>(result));
            
            if (result != VK_SUCCESS) {
                std::string errorString = GetVulkanResultString(result);
                SetError("Failed to create graphics pipeline: " + errorString);
                Logger::Error("VulkanPipeline", "Failed to create graphics pipeline: {}", errorString);
                Logger::Error("VulkanPipeline", "Error details:");
                Logger::Error("VulkanPipeline", "  - VkResult code: {} ({})", static_cast<int32_t>(result), errorString);
                Logger::Error("VulkanPipeline", "  - Device: {}", reinterpret_cast<uintptr_t>(device));
                Logger::Error("VulkanPipeline", "  - Pipeline layout valid: {}", pipelineInfo.layout != VK_NULL_HANDLE);
                Logger::Error("VulkanPipeline", "  - Render pass valid: {}", pipelineInfo.renderPass != VK_NULL_HANDLE);
                Logger::Error("VulkanPipeline", "  - Shader stages count: {}", pipelineInfo.stageCount);
                return false;
            }
            
            Logger::Info("VulkanPipeline", "Graphics pipeline created successfully: {}", reinterpret_cast<uintptr_t>(m_pipeline));
            
        } catch (const std::exception& e) {
            Logger::Error("VulkanPipeline", "Exception during vkCreateGraphicsPipelines: {}", e.what());
            SetError("Exception during pipeline creation: " + std::string(e.what()));
            return false;
        } catch (...) {
            Logger::Error("VulkanPipeline", "Unknown exception during vkCreateGraphicsPipelines");
            SetError("Unknown exception during pipeline creation");
            return false;
        }
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during graphics pipeline creation: ") + e.what());
        Logger::Error("VulkanPipeline", "Graphics pipeline creation failed with exception: {}", e.what());
        return false;
    }
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanPipeline::CreateShaderStages() {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    
    Logger::Info("VulkanPipeline", "Creating shader stages for {} shaders", m_config.shaders.size());
    
    for (size_t i = 0; i < m_config.shaders.size(); ++i) {
        auto* shader = m_config.shaders[i];
        
        if (!shader) {
            Logger::Error("VulkanPipeline", "Shader at index {} is null!", i);
            continue;
        }
        
        VkShaderModule module = shader->GetModule();
        VkShaderStageFlagBits stage = shader->GetStage();
        
        Logger::Info("VulkanPipeline", "Processing shader {}: stage={}, module={}",
                    i, static_cast<uint32_t>(stage), reinterpret_cast<uintptr_t>(module));
        
        if (module == VK_NULL_HANDLE) {
            Logger::Error("VulkanPipeline", "ERROR: Shader {} has NULL module!", i);
            continue;
        }
        
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stage;
        stageInfo.module = module;
        stageInfo.pName = "main";  // Shader'daki main fonksiyonu
        
        shaderStages.push_back(stageInfo);
        
        Logger::Debug("VulkanPipeline", "Added shader stage: {} (module: {})",
                     static_cast<uint32_t>(stage), reinterpret_cast<uintptr_t>(module));
    }
    
    if (shaderStages.empty()) {
        Logger::Error("VulkanPipeline", "No valid shader stages created!");
    } else {
        Logger::Info("VulkanPipeline", "Successfully created {} shader stages", shaderStages.size());
    }
    
    return shaderStages;
}

VkPipelineVertexInputStateCreateInfo VulkanPipeline::CreateVertexInputState(
    const VkVertexInputBindingDescription& bindingDescription,
    const std::array<VkVertexInputAttributeDescription, 3>& attributeDescriptions) {
    // Check if we should use minimal vertex input (for debugging)
    if (m_config.useMinimalVertexInput) {
        Logger::Warning("VulkanPipeline", "Using minimal vertex input state (no attributes)");
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        
        return vertexInputInfo;
    }
    
    Logger::Info("VulkanPipeline", "Creating vertex input state");
    Logger::Info("VulkanPipeline", "Binding description:");
    Logger::Info("VulkanPipeline", "  - binding: {}", bindingDescription.binding);
    Logger::Info("VulkanPipeline", "  - stride: {}", bindingDescription.stride);
    Logger::Info("VulkanPipeline", "  - inputRate: {}", static_cast<uint32_t>(bindingDescription.inputRate));
    
    Logger::Info("VulkanPipeline", "Attribute descriptions count: {}", attributeDescriptions.size());
    for (size_t i = 0; i < attributeDescriptions.size(); ++i) {
        Logger::Info("VulkanPipeline", "Attribute {}:", i);
        Logger::Info("VulkanPipeline", "  - binding: {}", attributeDescriptions[i].binding);
        Logger::Info("VulkanPipeline", "  - location: {}", attributeDescriptions[i].location);
        Logger::Info("VulkanPipeline", "  - format: {}", static_cast<uint32_t>(attributeDescriptions[i].format));
        Logger::Info("VulkanPipeline", "  - offset: {}", attributeDescriptions[i].offset);
        
        // Validate that attribute locations match shader layout
        if (i == 0 && attributeDescriptions[i].location != 0) {
            Logger::Error("VulkanPipeline", "ERROR: Position attribute should be at location 0, but found at location {}", attributeDescriptions[i].location);
        }
        if (i == 1 && attributeDescriptions[i].location != 1) {
            Logger::Error("VulkanPipeline", "ERROR: Color attribute should be at location 1, but found at location {}", attributeDescriptions[i].location);
        }
        if (i == 2 && attributeDescriptions[i].location != 2) {
            Logger::Error("VulkanPipeline", "ERROR: Texture coordinate attribute should be at location 2, but found at location {}", attributeDescriptions[i].location);
        }
        
        // Validate format matches expected shader input
        if (i == 0 && attributeDescriptions[i].format != VK_FORMAT_R32G32B32_SFLOAT) {
            Logger::Error("VulkanPipeline", "ERROR: Position attribute should be VK_FORMAT_R32G32B32_SFLOAT, but found {}", static_cast<uint32_t>(attributeDescriptions[i].format));
        }
        if (i == 1 && attributeDescriptions[i].format != VK_FORMAT_R32G32B32_SFLOAT) {
            Logger::Error("VulkanPipeline", "ERROR: Color attribute should be VK_FORMAT_R32G32B32_SFLOAT, but found {}", static_cast<uint32_t>(attributeDescriptions[i].format));
        }
        if (i == 2 && attributeDescriptions[i].format != VK_FORMAT_R32G32_SFLOAT) {
            Logger::Error("VulkanPipeline", "ERROR: Texture coordinate attribute should be VK_FORMAT_R32G32_SFLOAT, but found {}", static_cast<uint32_t>(attributeDescriptions[i].format));
        }
    }
    
    // Log shader layout expectations for comparison
    Logger::Info("VulkanPipeline", "Expected shader layout:");
    Logger::Info("VulkanPipeline", "  - layout(location = 0) in vec3 inPosition");
    Logger::Info("VulkanPipeline", "  - layout(location = 1) in vec3 inColor");
    Logger::Info("VulkanPipeline", "  - layout(location = 2) in vec2 inTexCoord");
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    Logger::Debug("VulkanPipeline", "Vertex input state created successfully");
    return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo VulkanPipeline::CreateInputAssemblyState() {
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // Üçgen listeleri
    inputAssembly.primitiveRestartEnable = VK_FALSE;  // Primitive restart devre dışı
    
    Logger::Debug("VulkanPipeline", "Input assembly state created (TRIANGLE_LIST)");
    return inputAssembly;
}

std::vector<VkDynamicState> VulkanPipeline::GetDynamicStates() {
    // Dinamik state'ler - her frame'de değiştirilebilir
    return { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
}

VkPipelineViewportStateCreateInfo VulkanPipeline::CreateViewportState() {
    // Viewport ve scissor dynamic state olduğu için burada boş geçiyoruz
    // Gerçek değerler command buffer'da ayarlanacak
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;  // Dynamic state
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;  // Dynamic state
    
    Logger::Debug("VulkanPipeline", "Viewport state created (dynamic)");
    return viewportState;
}

VkPipelineRasterizationStateCreateInfo VulkanPipeline::CreateRasterizationState() {
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;           // Depth clamping devre dışı
    rasterizer.rasterizerDiscardEnable = VK_FALSE;    // Rasterization'ı devre dışı bırakma
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;    // Üçgenleri doldur
    rasterizer.lineWidth = 1.0f;                      // Çizgi kalınlığı
    rasterizer.cullMode = VK_CULL_MODE_NONE;          // Culling devre dışı - güvenlik önlemi
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // Vertex sırası (saat yönünün tersi) - vertex verisiyle tutarlı
    rasterizer.depthBiasEnable = VK_FALSE;            // Depth bias devre dışı
    
    Logger::Debug("VulkanPipeline", "Rasterization state created (FILL, NO_CULL, COUNTER_CLOCKWISE)");
    return rasterizer;
}

VkPipelineMultisampleStateCreateInfo VulkanPipeline::CreateMultisampleState() {
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;     // Sample shading devre dışı
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // Anti-aliasing yok
    multisampling.minSampleShading = 1.0f;           // Minimum sample shading
    multisampling.pSampleMask = nullptr;              // Sample mask
    multisampling.alphaToCoverageEnable = VK_FALSE;    // Alpha to coverage devre dışı
    multisampling.alphaToOneEnable = VK_FALSE;        // Alpha to one devre dışı
    
    Logger::Debug("VulkanPipeline", "Multisample state created (1 sample)");
    return multisampling;
}

VkPipelineDepthStencilStateCreateInfo VulkanPipeline::CreateDepthStencilState() {
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;           // Derinlik testi aktif
    depthStencil.depthWriteEnable = VK_TRUE;          // Derinlik buffer'ına yazma aktif
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;  // Daha az olan kabul edilir
    depthStencil.depthBoundsTestEnable = VK_FALSE;    // Depth bounds test devre dışı
    depthStencil.stencilTestEnable = VK_FALSE;        // Stencil test devre dışı
    
    Logger::Debug("VulkanPipeline", "Depth stencil state created (LESS, depth test enabled)");
    return depthStencil;
}

VkPipelineColorBlendStateCreateInfo VulkanPipeline::CreateColorBlendState() {
    // Color blend attachment - basit renk kopyalama
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;     // Blending devre dışı
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;          // Logic operations devre dışı
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;          // Blend constants
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;
    
    Logger::Debug("VulkanPipeline", "Color blend state created (no blending)");
    return colorBlending;
}

void VulkanPipeline::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanPipeline", "Error: {}", error);
}

std::string VulkanPipeline::GetVulkanResultString(VkResult result) const {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT";
        default: return "Unknown VkResult (" + std::to_string(result) + ")";
    }
}

} // namespace AstralEngine
