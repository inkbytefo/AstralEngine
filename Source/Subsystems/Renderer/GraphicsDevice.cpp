#include "GraphicsDevice.h"
#include "../Platform/Window.h"
#include "../../Core/Logger.h"

#include <set>
#include <algorithm>

#ifdef ASTRAL_USE_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

#ifdef ASTRAL_USE_VULKAN
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Logger::Error("VulkanValidation", "{}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Logger::Warning("VulkanValidation", "{}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}
#endif

GraphicsDevice::GraphicsDevice() {
    Logger::Debug("GraphicsDevice", "GraphicsDevice created");
}

GraphicsDevice::~GraphicsDevice() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("GraphicsDevice", "GraphicsDevice destroyed");
}

bool GraphicsDevice::Initialize(Window* window) {
    if (m_initialized) {
        return true;
    }
    if (!window) {
        Logger::Critical("GraphicsDevice", "Cannot initialize without a valid window");
        return false;
    }
    m_window = window;

    Logger::Info("GraphicsDevice", "Initializing Vulkan GraphicsDevice...");

#ifndef ASTRAL_USE_VULKAN
    Logger::Critical("GraphicsDevice", "Vulkan not available, cannot initialize GraphicsDevice.");
    return false;
#endif

    if (!CreateInstance()) return false;
    if (!SetupDebugMessenger()) Logger::Warning("GraphicsDevice", "Failed to setup debug messenger.");
    if (!CreateSurface()) return false;
    if (!PickPhysicalDevice()) return false;
    if (!CreateLogicalDevice()) return false;

    CreateRenderPass();

    m_swapchain = std::make_unique<VulkanSwapchain>(this, m_window);
    m_swapchain->Initialize(m_renderPass);

    m_pipeline = std::make_unique<VulkanPipeline>(this, "Assets/Shaders/triangle.vert.spv", "Assets/Shaders/triangle.frag.spv");
    m_pipeline->Initialize(m_renderPass);

    m_commandManager = std::make_unique<VulkanCommandManager>(this, 2); // 2 frames in flight
    m_commandManager->Initialize();

    Logger::Info("GraphicsDevice", "Vulkan GraphicsDevice initialized successfully");
    m_initialized = true;
    return true;
}

void GraphicsDevice::Shutdown() {
    if (!m_initialized) return;

    Logger::Info("GraphicsDevice", "Shutting down GraphicsDevice...");

#ifdef ASTRAL_USE_VULKAN
    vkDeviceWaitIdle(m_device);

    if(m_commandManager) {
        m_commandManager->Shutdown();
    }
    if(m_pipeline) {
        m_pipeline->Shutdown();
    }
    if(m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }
    if(m_swapchain) {
        m_swapchain->Shutdown();
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }

    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(m_instance, m_debugMessenger, nullptr);
        }
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
#endif

    m_initialized = false;
    Logger::Info("GraphicsDevice", "GraphicsDevice shutdown complete");
}

#ifdef ASTRAL_USE_VULKAN
void GraphicsDevice::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchain->GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}


bool GraphicsDevice::CreateInstance() {
    #ifdef ASTRAL_DEBUG
        m_validationLayersEnabled = CheckValidationLayerSupport();
    #endif

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Astral Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Astral Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto requiredExtensions = m_window->GetRequiredVulkanExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    if (m_validationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        Logger::Critical("GraphicsDevice", "Failed to create Vulkan instance!");
        return false;
    }

    return true;
}

bool GraphicsDevice::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }
    return true;
}

bool GraphicsDevice::SetupDebugMessenger() {
    if (!m_validationLayersEnabled) return true;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!func || func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        return false;
    }
    return true;
}

bool GraphicsDevice::CreateSurface() {
    return m_window->CreateVulkanSurface(m_instance, &m_surface);
}

bool GraphicsDevice::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        Logger::Critical("GraphicsDevice", "Failed to find GPUs with Vulkan support!");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    m_physicalDevice = devices[0]; // Simple selection

    if (m_physicalDevice == VK_NULL_HANDLE) {
        Logger::Critical("GraphicsDevice", "Failed to find a suitable GPU!");
        return false;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    Logger::Info("GraphicsDevice", "Selected GPU: {}", deviceProperties.deviceName);
    return true;
}

bool GraphicsDevice::CreateLogicalDevice() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamily = i;
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
            if (presentSupport) {
                m_presentQueueFamily = i;
                break;
            }
        }
        i++;
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {m_graphicsQueueFamily, m_presentQueueFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamilyIndex : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    if (m_validationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        Logger::Critical("GraphicsDevice", "Failed to create logical device!");
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);

    return true;
}
#endif

}
