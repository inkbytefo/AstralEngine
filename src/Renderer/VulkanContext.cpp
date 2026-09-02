#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Core/Window.hpp"

#include <iostream>
#include <set>
#include <cstring>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Astral {

static const std::vector<const char*> s_ValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {

    const char* severityStr = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severityStr = "VERBOSE";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severityStr = "WARNING";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severityStr = "ERROR";
    }

    const char* typeStr = "GENERAL";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        typeStr = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        typeStr = "PERF";
    }

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan " << typeStr << " " << severityStr << "]: " << pCallbackData->pMessage << "\n";
    } else {
        std::cout << "[Vulkan " << typeStr << " " << severityStr << "]: " << pCallbackData->pMessage << "\n";
    }

    return VK_FALSE;
}

VulkanContext::VulkanContext(Window& window, bool enableValidation)
    : m_Window(window), m_EnableValidation(enableValidation) {

    // 1. Dynamic Dispatcher baslat
    static bool dispatcherInitialized = false;
    if (!dispatcherInitialized) {
        auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        dispatcherInitialized = true;
    }

    CreateInstance();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance.get());

    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device.get());

    std::cout << "[Astral::VulkanContext] Vulkan 1.4 altyapisi basariyla hazirlandi.\n";
}

VulkanContext::~VulkanContext() {
    WaitIdle();

    m_Device.reset();
    m_Surface.reset();

    if (m_DebugMessenger != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(m_Instance.get(), m_DebugMessenger, nullptr);
        m_DebugMessenger = VK_NULL_HANDLE;
    }

    m_Instance.reset();

    std::cout << "[Astral::VulkanContext] Vulkan kaynaklari guvenle temizlendi.\n";
}

void VulkanContext::WaitIdle() {
    if (m_Device) {
        m_Device->waitIdle();
    }
}

void VulkanContext::CreateInstance() {
    if (m_EnableValidation && !CheckValidationLayerSupport()) {
        std::cerr << "[Astral::VulkanContext] Uyari: Validation layer istendi fakat sistemde bulunamadi!\n";
        m_EnableValidation = false;
    }

    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "AstralEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AstralCore";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (m_EnableValidation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    m_Instance = vk::createInstanceUnique(createInfo);
}

void VulkanContext::SetupDebugMessenger() {
    if (!m_EnableValidation) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    if (CreateDebugUtilsMessengerEXT(m_Instance.get(), &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
        std::cerr << "[Astral::VulkanContext] Debug messenger olusturulamadi!\n";
    } else {
        std::cout << "[Astral::VulkanContext] Vulkan Debug Messenger basariyla kuruldu (Validation Layer Aktif).\n";
    }
}

void VulkanContext::CreateSurface() {
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(m_Instance.get(), m_Window.GetNativeWindow(), nullptr, &rawSurface) != VK_SUCCESS) {
        throw std::runtime_error("Pencere yuzeyi (VkSurfaceKHR) olusturulamadi!");
    }
    m_Surface = vk::UniqueSurfaceKHR(rawSurface, m_Instance.get());
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;
    auto queueFamilies = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
            indices.hasGraphicsFamily = true;
        }

        VkBool32 presentSupport = false;
        if (m_Surface) {
            presentSupport = device.getSurfaceSupportKHR(i, m_Surface.get());
        }
        if (presentSupport) {
            indices.presentFamily = i;
            indices.hasPresentFamily = true;
        }

        if (indices.IsComplete()) {
            break;
        }
    }

    return indices;
}

bool VulkanContext::IsDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = FindQueueFamilies(device);
    auto props = device.getProperties();

    // Vulkan 1.3 veya uzeri gerekli (Dynamic rendering ve synchronization2 icin)
    bool supportsVersion = props.apiVersion >= VK_API_VERSION_1_3;

    return indices.IsComplete() && supportsVersion;
}

void VulkanContext::PickPhysicalDevice() {
    auto devices = m_Instance->enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Vulkan destekli GPU bulunamadi!");
    }

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            auto props = device.getProperties();
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                m_PhysicalDevice = device;
                break;
            }
            if (!m_PhysicalDevice) {
                m_PhysicalDevice = device;
            }
        }
    }

    if (!m_PhysicalDevice) {
        throw std::runtime_error("Gereksinimleri karsilayan bir GPU bulunamadi!");
    }

    auto props = m_PhysicalDevice.getProperties();
    std::cout << "[Astral::VulkanContext] Secilen GPU: " << props.deviceName << "\n";
    std::cout << "[Astral::VulkanContext] Vulkan API Versiyonu: "
              << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << "\n";
}

void VulkanContext::CreateLogicalDevice() {
    m_QueueIndices = FindQueueFamilies(m_PhysicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        m_QueueIndices.graphicsFamily,
        m_QueueIndices.presentFamily
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Vulkan 1.3 / 1.4 core ozellikleri (Dynamic rendering + synchronization2)
    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.pNext = &features13;

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pNext = &deviceFeatures2;



    m_Device = m_PhysicalDevice.createDeviceUnique(createInfo);
    m_GraphicsQueue = m_Device->getQueue(m_QueueIndices.graphicsFamily, 0);
    m_PresentQueue = m_Device->getQueue(m_QueueIndices.presentFamily, 0);
}

std::vector<const char*> VulkanContext::GetRequiredExtensions() {
    auto extensions = Window::GetRequiredExtensions();
    if (m_EnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool VulkanContext::CheckValidationLayerSupport() {
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    for (const char* layerName : s_ValidationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }

    return true;
}

} // namespace Astral
