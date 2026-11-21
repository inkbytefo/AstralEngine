#include "Vk/Device.hpp"

#include <cstdlib>
#include <cstring>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Astral::Render::Vk {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        Log::Error("vulkan", "{}", data->pMessage);
    else
        Log::Warn("vulkan", "{}", data->pMessage);
    return VK_FALSE;
}

bool HasLayer(const char* name) {
    for (const auto& layer : vk::enumerateInstanceLayerProperties())
        if (std::strcmp(layer.layerName, name) == 0) return true;
    return false;
}

bool HasDeviceExtension(vk::PhysicalDevice device, const char* name) {
    for (const auto& ext : device.enumerateDeviceExtensionProperties())
        if (std::strcmp(ext.extensionName, name) == 0) return true;
    return false;
}

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

} // namespace

Device::Device(const DeviceDesc& desc) {
    // The Vulkan loader exports vkGetInstanceProcAddr; everything else is fetched through it.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    CreateInstance(desc);
    PickPhysicalDevice();
    CreateDevice();
}

Device::~Device() {
    if (!m_Device) return;
    m_Device.waitIdle();
    FlushGarbage();
    m_Device.destroyFence(m_UploadFence);
    m_Device.destroyCommandPool(m_UploadPool);
    vmaDestroyAllocator(m_Allocator);
    m_Device.destroy();
    if (m_Messenger) m_Instance.destroyDebugUtilsMessengerEXT(m_Messenger);
    m_Instance.destroy();
}

void Device::CreateInstance(const DeviceDesc& desc) {
    const char* env = std::getenv("ASTRAL_VALIDATION");
    m_Validation = env ? std::strcmp(env, "0") != 0 : desc.validation;
    if (m_Validation && !HasLayer(kValidationLayer)) {
        Log::Warn("render", "validation requested but {} is not installed", kValidationLayer);
        m_Validation = false;
    }

    std::vector<const char*> extensions;
    for (const auto& e : desc.instanceExtensions) {
        extensions.push_back(e.c_str());
        if (e == VK_KHR_SURFACE_EXTENSION_NAME) m_Presentation = true;
    }
    if (m_Validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::ApplicationInfo app;
    app.pApplicationName = desc.applicationName.c_str();
    app.pEngineName = "Astral";
    app.engineVersion = VK_MAKE_API_VERSION(0, 0, 2, 0);
    app.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo info;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (m_Validation) {
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &kValidationLayer;
    }
    m_Instance = vk::createInstance(info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance);

    if (m_Validation) {
        VkDebugUtilsMessengerCreateInfoEXT messenger{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        messenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        messenger.pfnUserCallback = DebugCallback;
        Check(VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDebugUtilsMessengerEXT(m_Instance, &messenger, nullptr, &m_Messenger),
              "vkCreateDebugUtilsMessengerEXT");
    }
}

void Device::PickPhysicalDevice() {
    vk::PhysicalDevice fallback;
    for (vk::PhysicalDevice candidate : m_Instance.enumeratePhysicalDevices()) {
        auto props = candidate.getProperties();
        if (props.apiVersion < VK_API_VERSION_1_3) continue;
        if (!HasDeviceExtension(candidate, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) continue;

        auto chain = candidate.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features,
                                            vk::PhysicalDeviceVulkan13Features>();
        const auto& f12 = chain.get<vk::PhysicalDeviceVulkan12Features>();
        const auto& f13 = chain.get<vk::PhysicalDeviceVulkan13Features>();
        if (!f13.dynamicRendering || !f13.synchronization2) continue;
        if (!chain.get<vk::PhysicalDeviceFeatures2>().features.shaderStorageImageExtendedFormats) continue; // r16f, rg32f storage
        if (!f12.runtimeDescriptorArray || !f12.descriptorBindingPartiallyBound ||
            !f12.shaderSampledImageArrayNonUniformIndexing || !f12.descriptorBindingSampledImageUpdateAfterBind)
            continue;

        bool hasQueue = false;
        auto families = candidate.getQueueFamilyProperties();
        for (uint32_t i = 0; i < families.size(); ++i) {
            auto flags = families[i].queueFlags;
            if ((flags & vk::QueueFlagBits::eGraphics) && (flags & vk::QueueFlagBits::eCompute)) {
                hasQueue = true;
                break;
            }
        }
        if (!hasQueue) continue;

        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            m_Physical = candidate;
            break;
        }
        if (!fallback) fallback = candidate;
    }
    if (!m_Physical) m_Physical = fallback;
    if (!m_Physical)
        throw std::runtime_error("No GPU with Vulkan 1.3, dynamic rendering, synchronization2, descriptor indexing and push descriptors");

    m_Properties = m_Physical.getProperties();
    Log::Info("render", "GPU: {}", m_Properties.deviceName.data());

    auto families = m_Physical.getQueueFamilyProperties();
    for (uint32_t i = 0; i < families.size(); ++i) {
        auto flags = families[i].queueFlags;
        if ((flags & vk::QueueFlagBits::eGraphics) && (flags & vk::QueueFlagBits::eCompute)) {
            m_QueueFamily = i;
            break;
        }
    }
}

void Device::CreateDevice() {
    auto supported = m_Physical.getFeatures();
    m_DepthClamp = supported.depthClamp;
    m_MaxAnisotropy = supported.samplerAnisotropy ? std::min(8.0f, m_Properties.limits.maxSamplerAnisotropy) : 1.0f;

    vk::PhysicalDeviceVulkan13Features f13;
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;

    vk::PhysicalDeviceVulkan12Features f12;
    f12.pNext = &f13;
    f12.runtimeDescriptorArray = VK_TRUE;
    f12.descriptorBindingPartiallyBound = VK_TRUE;
    f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

    vk::PhysicalDeviceFeatures2 features;
    features.pNext = &f12;
    features.features.samplerAnisotropy = supported.samplerAnisotropy;
    features.features.depthClamp = supported.depthClamp;
    features.features.shaderStorageImageExtendedFormats = supported.shaderStorageImageExtendedFormats;

    std::vector<const char*> extensions = {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    if (m_Presentation && HasDeviceExtension(m_Physical, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, m_QueueFamily, 1, &priority);

    vk::DeviceCreateInfo info;
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    m_Device = m_Physical.createDevice(info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);
    m_Queue = m_Device.getQueue(m_QueueFamily, 0);

    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = m_Instance;
    allocatorInfo.physicalDevice = m_Physical;
    allocatorInfo.device = m_Device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.pVulkanFunctions = &functions;
    Check(vmaCreateAllocator(&allocatorInfo, &m_Allocator), "vmaCreateAllocator");

    m_UploadPool = m_Device.createCommandPool({vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_QueueFamily});
    m_UploadFence = m_Device.createFence({});
}

void Device::ImmediateSubmit(const std::function<void(vk::CommandBuffer)>& record) {
    vk::CommandBuffer cmd = m_Device.allocateCommandBuffers({m_UploadPool, vk::CommandBufferLevel::ePrimary, 1})[0];
    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    record(cmd);
    cmd.end();
    vk::SubmitInfo submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    m_Queue.submit(submit, m_UploadFence);
    (void)m_Device.waitForFences(m_UploadFence, VK_TRUE, UINT64_MAX);
    m_Device.resetFences(m_UploadFence);
    m_Device.freeCommandBuffers(m_UploadPool, cmd);
}

void Device::Retire(std::function<void()> destroy) { m_Garbage.push_back({m_FrameNumber, std::move(destroy)}); }

void Device::CollectGarbage(uint64_t completedFrame) {
    while (!m_Garbage.empty() && m_Garbage.front().frame <= completedFrame) {
        m_Garbage.front().destroy();
        m_Garbage.pop_front();
    }
}

void Device::FlushGarbage() {
    for (auto& g : m_Garbage) g.destroy();
    m_Garbage.clear();
}

void Device::SetDebugName(vk::ObjectType type, uint64_t handle, const std::string& name) {
    if (!m_Validation || name.empty()) return;
    vk::DebugUtilsObjectNameInfoEXT info(type, handle, name.c_str());
    m_Device.setDebugUtilsObjectNameEXT(info);
}

} // namespace Astral::Render::Vk
