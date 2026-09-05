#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/Swapchain.hpp"
#include "Astral/Core/Window.hpp"

#include <iostream>
#include <sstream>
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
    CreateAllocator();

    CreateCommandPoolAndBuffers();
    CreateTimestampQueryPool();
    CreateSwapchain();

    std::cout << "[Astral::VulkanContext] Vulkan 1.4 ve GPU Timestamp altyapisi basariyla hazirlandi.\n";
}

VulkanContext::~VulkanContext() {
    WaitIdle();

    m_Swapchain.reset();
    m_RenderFinishedSemaphores.clear();
    m_ImageAvailableSemaphores.clear();

    m_TimestampQueryPool.reset();
    m_FrameFence.reset();
    m_CommandBuffer.reset();
    m_CommandPool.reset();

    DestroyAllocator();

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
    m_TimestampPeriod = props.limits.timestampPeriod;

    std::cout << "[Astral::VulkanContext] Secilen GPU: " << props.deviceName << "\n";
    std::cout << "[Astral::VulkanContext] Vulkan API Versiyonu: "
              << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << "\n";
    std::cout << "[Astral::VulkanContext] Timestamp Period: " << m_TimestampPeriod << " ns/tick\n";
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

    auto availableDeviceExts = m_PhysicalDevice.enumerateDeviceExtensionProperties();
    for (const auto& ext : availableDeviceExts) {
        if (std::strcmp(ext.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
            deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
            break;
        }
    }

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

void VulkanContext::CreateCommandPoolAndBuffers() {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = m_QueueIndices.graphicsFamily;

    m_CommandPool = m_Device->createCommandPoolUnique(poolInfo);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_CommandPool.get();
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmdBuffers = m_Device->allocateCommandBuffersUnique(allocInfo);
    m_CommandBuffer = std::move(cmdBuffers[0]);

    // Baslangicta signaled fence olustur
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);
    m_FrameFence = m_Device->createFenceUnique(fenceInfo);
}

void VulkanContext::CreateTimestampQueryPool() {
    vk::QueryPoolCreateInfo poolInfo{};
    poolInfo.queryType = vk::QueryType::eTimestamp;
    poolInfo.queryCount = QUERY_COUNT;

    m_TimestampQueryPool = m_Device->createQueryPoolUnique(poolInfo);
}

vk::CommandBuffer VulkanContext::BeginFrameCommand() {
    // Onceki submit'in tamamlanmasini bekle
    auto resWait = m_Device->waitForFences(1, &m_FrameFence.get(), VK_TRUE, UINT64_MAX);
    (void)resWait;
    auto resReset = m_Device->resetFences(1, &m_FrameFence.get());
    (void)resReset;

    m_CommandBuffer->reset();
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_CommandBuffer->begin(beginInfo);

    // Query pool'u her frame resetle ve ilk zaman damgasini yaz
    m_CommandBuffer->resetQueryPool(m_TimestampQueryPool.get(), 0, QUERY_COUNT);
    m_CommandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, m_TimestampQueryPool.get(), QUERY_FRAME_START);

    return m_CommandBuffer.get();
}

void VulkanContext::WriteTimestamp(vk::CommandBuffer cmd, vk::PipelineStageFlagBits stage, uint32_t queryIndex) {
    cmd.writeTimestamp(stage, m_TimestampQueryPool.get(), queryIndex);
}

void VulkanContext::EndAndSubmitFrameCommand() {
    m_CommandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_TimestampQueryPool.get(), QUERY_FRAME_END);
    m_CommandBuffer->end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    vk::CommandBuffer rawCmd = m_CommandBuffer.get();
    submitInfo.pCommandBuffers = &rawCmd;

    auto resSubmit = m_GraphicsQueue.submit(1, &submitInfo, m_FrameFence.get());
    (void)resSubmit;

    // Fence bekle ve GPU surelerini oku
    auto res = m_Device->waitForFences(1, &m_FrameFence.get(), VK_TRUE, UINT64_MAX);
    (void)res;

    uint64_t timestamps[2] = {0, 0};
    auto qr = m_Device->getQueryPoolResults(
        m_TimestampQueryPool.get(),
        0,
        2,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
    );

    if (qr == vk::Result::eSuccess && timestamps[1] >= timestamps[0]) {
        uint64_t deltaTicks = timestamps[1] - timestamps[0];
        m_LastGpuTimeMs = (static_cast<double>(deltaTicks) * static_cast<double>(m_TimestampPeriod)) / 1e6;
        m_HasValidGpuTime = true;
    }
}

double VulkanContext::GetLastGpuTimeMs() {
    return m_LastGpuTimeMs;
}

void VulkanContext::ExecuteImmediate(std::function<void(vk::CommandBuffer)> func) {
    if (!func || !m_Device || !m_CommandPool) return;

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_CommandPool.get();
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmdBuffers = m_Device->allocateCommandBuffersUnique(allocInfo);
    vk::CommandBuffer cmd = cmdBuffers[0].get();

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(beginInfo);

    func(cmd);

    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    auto submitRes = m_GraphicsQueue.submit(1, &submitInfo, VK_NULL_HANDLE);
    (void)submitRes;
    m_GraphicsQueue.waitIdle();
}

std::string VulkanContext::GetDeviceName() const {
    if (!m_PhysicalDevice) return "Bilinmeyen GPU";
    return std::string(m_PhysicalDevice.getProperties().deviceName.data());
}

std::string VulkanContext::GetDriverVersionString() const {
    if (!m_PhysicalDevice) return "0.0";
    auto props = m_PhysicalDevice.getProperties();
    std::ostringstream ss;
    ss << props.driverVersion;
    return ss.str();
}

std::string VulkanContext::GetVulkanVersionString() const {
    if (!m_PhysicalDevice) return "1.4";
    auto props = m_PhysicalDevice.getProperties();
    std::ostringstream ss;
    ss << VK_VERSION_MAJOR(props.apiVersion) << "."
       << VK_VERSION_MINOR(props.apiVersion) << "."
       << VK_VERSION_PATCH(props.apiVersion);
    return ss.str();
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

void VulkanContext::CreateSwapchain() {
    m_Swapchain = std::make_unique<Swapchain>(
        m_Device.get(),
        m_PhysicalDevice,
        m_Surface.get(),
        static_cast<uint32_t>(m_Window.GetWidth()),
        static_cast<uint32_t>(m_Window.GetHeight())
    );

    m_ImageAvailableSemaphores.clear();
    m_RenderFinishedSemaphores.clear();
    size_t count = m_Swapchain->GetImages().size();
    vk::SemaphoreCreateInfo semInfo{};
    for (size_t i = 0; i < count; ++i) {
        m_ImageAvailableSemaphores.push_back(m_Device->createSemaphoreUnique(semInfo));
        m_RenderFinishedSemaphores.push_back(m_Device->createSemaphoreUnique(semInfo));
    }
    m_CurrentFrame = 0;
}

void VulkanContext::RecreateSwapchain() {
    int width = m_Window.GetWidth();
    int height = m_Window.GetHeight();
    if (width <= 0 || height <= 0) return;

    m_Device->waitIdle();
    m_Swapchain.reset();
    CreateSwapchain();
}

bool VulkanContext::AcquireNextImage() {
    if (!m_Swapchain || m_ImageAvailableSemaphores.empty()) return false;

    auto result = m_Device->acquireNextImageKHR(
        m_Swapchain->GetSwapchain(),
        UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame].get(),
        nullptr,
        &m_CurrentImageIndex
    );

    if (result == vk::Result::eErrorOutOfDateKHR) {
        RecreateSwapchain();
        return false;
    } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        return false;
    }
    return true;
}

void VulkanContext::PrepareSwapchainImage() {
    if (!m_Swapchain) return;

    auto cmd = m_CommandBuffer.get();
    vk::Image swapImage = m_Swapchain->GetImages()[m_CurrentImageIndex];

    vk::ImageMemoryBarrier toColorAttachment{};
    toColorAttachment.oldLayout = vk::ImageLayout::eUndefined;
    toColorAttachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.image = swapImage;
    toColorAttachment.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toColorAttachment.srcAccessMask = {};
    toColorAttachment.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, nullptr, nullptr, toColorAttachment
    );
}

void VulkanContext::EndFrameBlit(vk::Image sourceImage, uint32_t srcWidth, uint32_t srcHeight) {
    if (!m_Swapchain) return;

    auto cmd = m_CommandBuffer.get();
    vk::Image swapImage = m_Swapchain->GetImages()[m_CurrentImageIndex];
    auto extent = m_Swapchain->GetExtent();

    // 1. Swapchain image'i TransferDst Optimal yap
    vk::ImageMemoryBarrier toTransferDst{};
    toTransferDst.oldLayout = vk::ImageLayout::eUndefined;
    toTransferDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = swapImage;
    toTransferDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toTransferDst.srcAccessMask = {};
    toTransferDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    // sourceImage eGeneral duzeninde transfer okumasina hazirlanir
    vk::ImageMemoryBarrier srcBarrier{};
    srcBarrier.oldLayout = vk::ImageLayout::eGeneral;
    srcBarrier.newLayout = vk::ImageLayout::eGeneral;
    srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.image = sourceImage;
    srcBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    srcBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
    srcBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    std::array<vk::ImageMemoryBarrier, 2> barriers = { toTransferDst, srcBarrier };
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {}, nullptr, nullptr, barriers
    );

    // 2. Blit sourceImage (m_StorageImage, eGeneral) -> swapImage (eTransferDstOptimal)
    vk::ImageBlit blitRegion{};
    blitRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    blitRegion.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
    blitRegion.srcOffsets[1] = vk::Offset3D{ static_cast<int32_t>(srcWidth), static_cast<int32_t>(srcHeight), 1 };
    blitRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    blitRegion.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
    blitRegion.dstOffsets[1] = vk::Offset3D{ static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1 };

    cmd.blitImage(
        sourceImage, vk::ImageLayout::eGeneral,
        swapImage, vk::ImageLayout::eTransferDstOptimal,
        1, &blitRegion, vk::Filter::eLinear
    );

    // 3. Swapchain image'i Present oncesi ColorAttachmentOptimal yap
    vk::ImageMemoryBarrier toColorAttachment{};
    toColorAttachment.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    toColorAttachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.image = swapImage;
    toColorAttachment.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toColorAttachment.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    toColorAttachment.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, nullptr, nullptr, toColorAttachment
    );
}

void VulkanContext::EndFramePresent() {
    if (!m_Swapchain) return;

    auto cmd = m_CommandBuffer.get();
    vk::Image swapImage = m_Swapchain->GetImages()[m_CurrentImageIndex];

    // 1. Swapchain image'i PresentSrcKHR yap
    vk::ImageMemoryBarrier toPresent{};
    toPresent.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toPresent.newLayout = vk::ImageLayout::ePresentSrcKHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = swapImage;
    toPresent.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toPresent.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    toPresent.dstAccessMask = {};

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {}, nullptr, nullptr, toPresent
    );

    m_CommandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_TimestampQueryPool.get(), QUERY_FRAME_END);
    m_CommandBuffer->end();

    // 2. Submit with Semaphores
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentFrame].get();
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    vk::CommandBuffer rawCmd = m_CommandBuffer.get();
    submitInfo.pCommandBuffers = &rawCmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[m_CurrentImageIndex].get();

    auto resSubmit = m_GraphicsQueue.submit(1, &submitInfo, m_FrameFence.get());
    (void)resSubmit;

    // 3. Present
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentImageIndex].get();
    presentInfo.swapchainCount = 1;
    vk::SwapchainKHR rawSwapchain = m_Swapchain->GetSwapchain();
    presentInfo.pSwapchains = &rawSwapchain;
    presentInfo.pImageIndices = &m_CurrentImageIndex;

    m_CurrentFrame = (m_CurrentFrame + 1) % m_ImageAvailableSemaphores.size();

    try {
        auto resPresent = m_PresentQueue.presentKHR(presentInfo);
        if (resPresent == vk::Result::eSuboptimalKHR) {
            RecreateSwapchain();
        }
    } catch (const vk::OutOfDateKHRError&) {
        RecreateSwapchain();
    }

    // 4. Fence bekle ve GPU zamanini oku
    auto res = m_Device->waitForFences(1, &m_FrameFence.get(), VK_TRUE, UINT64_MAX);
    (void)res;

    uint64_t timestamps[2] = {0, 0};
    auto qr = m_Device->getQueryPoolResults(
        m_TimestampQueryPool.get(),
        0,
        2,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
    );

    if (qr == vk::Result::eSuccess && timestamps[1] >= timestamps[0]) {
        uint64_t deltaTicks = timestamps[1] - timestamps[0];
        m_LastGpuTimeMs = (static_cast<double>(deltaTicks) * static_cast<double>(m_TimestampPeriod)) / 1e6;
        m_HasValidGpuTime = true;
    }
}

void VulkanContext::CreateAllocator() {
    VmaVulkanFunctions vmaFunctions{};
    vmaFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    allocatorCreateInfo.physicalDevice = m_PhysicalDevice;
    allocatorCreateInfo.device = m_Device.get();
    allocatorCreateInfo.instance = m_Instance.get();
    allocatorCreateInfo.pVulkanFunctions = &vmaFunctions;

    VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("[Astral::VulkanContext] VmaAllocator olusturulamadi! VkResult: " + std::to_string(result));
    }
    std::cout << "[Astral::VulkanContext] VMA (Vulkan Memory Allocator) basariyla baslatildi.\n";
}

void VulkanContext::DestroyAllocator() {
    if (m_Allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
        std::cout << "[Astral::VulkanContext] VMA allocator serbest birakildi.\n";
    }
}

VmaTotalStatistics VulkanContext::GetMemoryStats() const {
    VmaTotalStatistics stats{};
    if (m_Allocator != VK_NULL_HANDLE) {
        vmaCalculateStatistics(m_Allocator, &stats);
    }
    return stats;
}

} // namespace Astral
