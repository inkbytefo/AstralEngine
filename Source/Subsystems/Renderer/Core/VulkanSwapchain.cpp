#include "VulkanSwapchain.h"
#include <algorithm>
#include <limits>
#include <array>

namespace AstralEngine {

VulkanSwapchain::VulkanSwapchain() = default;

VulkanSwapchain::~VulkanSwapchain() {
    Shutdown();
}

bool VulkanSwapchain::Initialize(VulkanDevice* device) {
    if (!device) {
        SetError("VulkanDevice pointer is null");
        return false;
    }

    if (m_isInitialized) {
        Logger::Warning("VulkanSwapchain", "VulkanSwapchain is already initialized");
        return true;
    }

    m_device = device;

    Logger::Info("VulkanSwapchain", "Initializing VulkanSwapchain...");

    // Create swapchain
    if (!CreateSwapchain()) {
        Logger::Error("VulkanSwapchain", "Failed to create swapchain: " + m_lastError);
        return false;
    }

    // Create image views
    if (!CreateImageViews()) {
        Logger::Error("VulkanSwapchain", "Failed to create image views: " + m_lastError);
        Cleanup();
        return false;
    }

    // Create depth resources
    if (!CreateDepthResources()) {
        Logger::Error("VulkanSwapchain", "Failed to create depth resources: " + m_lastError);
        Cleanup();
        return false;
    }

    // Create render pass
    if (!CreateRenderPass()) {
        Logger::Error("VulkanSwapchain", "Failed to create render pass: " + m_lastError);
        Cleanup();
        return false;
    }

    // Create framebuffers
    if (!CreateFramebuffers()) {
        Logger::Error("VulkanSwapchain", "Failed to create framebuffers: " + m_lastError);
        Cleanup();
        return false;
    }

    m_isInitialized = true;
    Logger::Info("VulkanSwapchain", "VulkanSwapchain initialized successfully");
    Logger::Info("VulkanSwapchain", "Swapchain extent: " + std::to_string(m_swapchainExtent.width) + "x" + std::to_string(m_swapchainExtent.height));
    Logger::Info("VulkanSwapchain", "Swapchain image count: " + std::to_string(m_swapchainImages.size()));
    Logger::Info("VulkanSwapchain", "Swapchain format: " + std::to_string(m_swapchainImageFormat));
    Logger::Info("VulkanSwapchain", "Depth format: " + std::to_string(m_depthFormat));

    return true;
}

void VulkanSwapchain::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    Logger::Info("VulkanSwapchain", "Shutting down VulkanSwapchain...");
    Cleanup();
    m_device = nullptr;
    m_isInitialized = false;
    Logger::Info("VulkanSwapchain", "VulkanSwapchain shutdown completed");
}

bool VulkanSwapchain::Recreate() {
    Logger::Info("VulkanSwapchain", "Recreating VulkanSwapchain...");

    // Cleanup old resources
    Cleanup();

    // Recreate all resources
    if (!CreateSwapchain()) {
        Logger::Error("VulkanSwapchain", "Failed to recreate swapchain: " + m_lastError);
        return false;
    }

    if (!CreateImageViews()) {
        Logger::Error("VulkanSwapchain", "Failed to recreate image views: " + m_lastError);
        Cleanup();
        return false;
    }

    if (!CreateDepthResources()) {
        Logger::Error("VulkanSwapchain", "Failed to recreate depth resources: " + m_lastError);
        Cleanup();
        return false;
    }

    if (!CreateRenderPass()) {
        Logger::Error("VulkanSwapchain", "Failed to recreate render pass: " + m_lastError);
        Cleanup();
        return false;
    }

    if (!CreateFramebuffers()) {
        Logger::Error("VulkanSwapchain", "Failed to recreate framebuffers: " + m_lastError);
        Cleanup();
        return false;
    }

    Logger::Info("VulkanSwapchain", "VulkanSwapchain recreated successfully");
    Logger::Info("VulkanSwapchain", "New swapchain extent: " + std::to_string(m_swapchainExtent.width) + "x" + std::to_string(m_swapchainExtent.height));
    return true;
}

VkFramebuffer VulkanSwapchain::GetFramebuffer(uint32_t index) const {
    if (index >= m_framebuffers.size()) {
        Logger::Error("VulkanSwapchain", "Framebuffer index out of range: " + std::to_string(index));
        return VK_NULL_HANDLE;
    }
    return m_framebuffers[index]->GetFramebuffer();
}

VkImageView VulkanSwapchain::GetImageView(uint32_t index) const {
    if (index >= m_swapchainImageViews.size()) {
        Logger::Error("VulkanSwapchain", "Image view index out of range: " + std::to_string(index));
        return VK_NULL_HANDLE;
    }
    return m_swapchainImageViews[index];
}

VulkanSwapchain::SwapchainSupportDetails VulkanSwapchain::QuerySwapchainSupport(VkPhysicalDevice device) {
    SwapchainSupportDetails details;

    // Get surface capabilities
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_device->GetSurface(), &details.capabilities);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        return details;
    }

    // Get surface formats
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_device->GetSurface(), &formatCount, nullptr);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        return details;
    }

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_device->GetSurface(), &formatCount, details.formats.data());
        if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
            details.formats.clear();
        }
    }

    // Get present modes
    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_device->GetSurface(), &presentModeCount, nullptr);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        return details;
    }

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_device->GetSurface(), &presentModeCount, details.presentModes.data());
        if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
            details.presentModes.clear();
        }
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB format if available
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            Logger::Info("VulkanSwapchain", "Selected surface format: VK_FORMAT_B8G8R8A8_SRGB with SRGB nonlinear color space");
            return availableFormat;
        }
    }

    // If SRGB is not available, choose the first available format
    if (!availableFormats.empty()) {
        Logger::Info("VulkanSwapchain", "Selected first available surface format: " + std::to_string(availableFormats[0].format));
        return availableFormats[0];
    }

    SetError("No available surface formats");
    return VkSurfaceFormatKHR{};
}

VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer MAILBOX mode for low latency (triple buffering)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            Logger::Info("VulkanSwapchain", "Selected present mode: VK_PRESENT_MODE_MAILBOX_KHR (triple buffering)");
            return availablePresentMode;
        }
    }

    // FIFO mode is guaranteed to be available (VSync)
    Logger::Info("VulkanSwapchain", "Selected present mode: VK_PRESENT_MODE_FIFO_KHR (VSync)");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        // Surface defines the extent
        VkExtent2D extent = capabilities.currentExtent;
        Logger::Info("VulkanSwapchain", "Using surface-defined extent: " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
        return extent;
    } else {
        // We need to choose the extent ourselves
        // Get window size from the device (this would need to be passed or queried)
        // For now, use the minimum image extent as a fallback
        VkExtent2D extent = {
            std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, static_cast<uint32_t>(1920))),
            std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, static_cast<uint32_t>(1080)))
        };
        
        Logger::Info("VulkanSwapchain", "Chosen extent: " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
        return extent;
    }
}

bool VulkanSwapchain::CreateSwapchain() {
    // Query swapchain support
    SwapchainSupportDetails swapchainSupport = QuerySwapchainSupport(m_device->GetPhysicalDevice());
    if (swapchainSupport.formats.empty() || swapchainSupport.presentModes.empty()) {
        SetError("No available swapchain formats or present modes");
        return false;
    }

    // Choose swapchain settings
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapchainSupport.capabilities);

    // Choose image count
    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    Logger::Info("VulkanSwapchain", "Creating swapchain with " + std::to_string(imageCount) + " images");

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_device->GetSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Handle queue families
    const auto& indices = m_device->GetQueueFamilyIndices();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device->GetDevice(), &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkCreateSwapchainKHR");
        return false;
    }

    // Get swapchain images
    result = vkGetSwapchainImagesKHR(m_device->GetDevice(), m_swapchain, &imageCount, nullptr);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkGetSwapchainImagesKHR");
        return false;
    }

    m_swapchainImages.resize(imageCount);
    result = vkGetSwapchainImagesKHR(m_device->GetDevice(), m_swapchain, &imageCount, m_swapchainImages.data());
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkGetSwapchainImagesKHR");
        return false;
    }

    // Store swapchain properties
    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    Logger::Info("VulkanSwapchain", "Swapchain created successfully");
    return true;
}

bool VulkanSwapchain::CreateImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        m_swapchainImageViews[i] = CreateImageView(m_swapchainImages[i], m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        if (m_swapchainImageViews[i] == VK_NULL_HANDLE) {
            Logger::Error("VulkanSwapchain", "Failed to create image view for index " + std::to_string(i));
            return false;
        }
    }

    Logger::Info("VulkanSwapchain", "Created " + std::to_string(m_swapchainImageViews.size()) + " image views");
    return true;
}

bool VulkanSwapchain::CreateDepthResources() {
    // Find supported depth format
    m_depthFormat = VulkanUtils::FindSupportedFormat(
        m_device->GetPhysicalDevice(),
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    if (m_depthFormat == VK_FORMAT_UNDEFINED) {
        SetError("No supported depth format found");
        return false;
    }

    Logger::Info("VulkanSwapchain", "Selected depth format: " + std::to_string(m_depthFormat));

    // Create depth image
    if (!CreateImage(
        m_swapchainExtent.width, m_swapchainExtent.height, m_depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory)) {
        return false;
    }

    // Create depth image view
    m_depthImageView = CreateImageView(m_depthImage, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (m_depthImageView == VK_NULL_HANDLE) {
        return false;
    }

    Logger::Info("VulkanSwapchain", "Depth resources created successfully");
    return true;
}


bool VulkanSwapchain::CreateRenderPass() {
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment references
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create render pass
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(m_device->GetDevice(), &renderPassInfo, nullptr, &m_renderPass);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkCreateRenderPass");
        return false;
    }

    Logger::Info("VulkanSwapchain", "Render pass created successfully");
    return true;
}

bool VulkanSwapchain::CreateFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        // Create VulkanFramebuffer instance
        auto framebuffer = std::make_unique<VulkanFramebuffer>();
        
        // Configure framebuffer
        VulkanFramebuffer::Config config;
        config.device = m_device;
        config.renderPass = m_renderPass;
        config.attachments = { m_swapchainImageViews[i], m_depthImageView };
        config.width = m_swapchainExtent.width;
        config.height = m_swapchainExtent.height;
        config.layers = 1;
        
        // Initialize framebuffer
        if (!framebuffer->Initialize(config)) {
            Logger::Error("VulkanSwapchain", "Failed to create framebuffer for index " + std::to_string(i) + ": " + framebuffer->GetLastError());
            return false;
        }
        
        // Store framebuffer
        m_framebuffers[i] = std::move(framebuffer);
    }

    Logger::Info("VulkanSwapchain", "Created " + std::to_string(m_framebuffers.size()) + " framebuffers");
    return true;
}

void VulkanSwapchain::Cleanup() {
    Logger::Info("VulkanSwapchain", "Cleaning up VulkanSwapchain resources...");

    // Cleanup framebuffers
    for (auto& framebuffer : m_framebuffers) {
        if (framebuffer && framebuffer->IsInitialized()) {
            framebuffer->Shutdown();
        }
    }
    m_framebuffers.clear();

    // Cleanup render pass
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    // Cleanup depth resources
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device->GetDevice(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device->GetDevice(), m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }

    if (m_depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_depthImageMemory, nullptr);
        m_depthImageMemory = VK_NULL_HANDLE;
    }

    // Cleanup image views
    for (auto imageView : m_swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device->GetDevice(), imageView, nullptr);
        }
    }
    m_swapchainImageViews.clear();

    // Cleanup swapchain
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device->GetDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_swapchainImages.clear();
    Logger::Info("VulkanSwapchain", "VulkanSwapchain resources cleaned up");
}


bool VulkanSwapchain::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                                 VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkCreateImage");
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->GetDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (allocInfo.memoryTypeIndex == std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    result = vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &imageMemory);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkAllocateMemory");
        vkDestroyImage(m_device->GetDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }

    // Bind memory
    result = vkBindImageMemory(m_device->GetDevice(), image, imageMemory, 0);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkBindImageMemory");
        vkFreeMemory(m_device->GetDevice(), imageMemory, nullptr);
        vkDestroyImage(m_device->GetDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        imageMemory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

VkImageView VulkanSwapchain::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkCreateImageView");
        return VK_NULL_HANDLE;
    }

    return imageView;
}

void VulkanSwapchain::HandleVulkanError(VkResult result, const std::string& operation) {
    std::string errorStr = "Vulkan error in " + operation + ": ";
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            errorStr += "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            errorStr += "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            errorStr += "VK_ERROR_INITIALIZATION_FAILED";
            break;
        case VK_ERROR_DEVICE_LOST:
            errorStr += "VK_ERROR_DEVICE_LOST";
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            errorStr += "VK_ERROR_SURFACE_LOST_KHR";
            break;
        default:
            errorStr += "Error code: " + std::to_string(result);
            break;
    }
    SetError(errorStr);
}

void VulkanSwapchain::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanSwapchain", error);
}

} // namespace AstralEngine
