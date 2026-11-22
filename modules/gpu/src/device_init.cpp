#include "vk_device.hpp"

#include <astral/core/log.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace astral::gpu {

namespace {

constexpr const char* validation_layer = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data,
                                              void* user) {
    auto* logger = static_cast<Logger*>(user);
    const bool error = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (logger) {
        logger->log(error ? LogLevel::Error : LogLevel::Warn, data->pMessage);
    } else if (error) {
        std::fprintf(stderr, "[vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

bool has_layer(const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::any_of(layers.begin(), layers.end(),
                       [&](const VkLayerProperties& l) { return std::strcmp(l.layerName, name) == 0; });
}

bool has_instance_extension(const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
    return std::any_of(exts.begin(), exts.end(), [&](const VkExtensionProperties& e) {
        return std::strcmp(e.extensionName, name) == 0;
    });
}

/// Every feature the engine relies on. A device without all of them is skipped.
bool meets_requirements(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    if (props.apiVersion < VK_API_VERSION_1_3) return false;

    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13};
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12};
    vkGetPhysicalDeviceFeatures2(device, &f2);
    return f13.dynamicRendering && f13.synchronization2 && f12.timelineSemaphore &&
           f12.bufferDeviceAddress && f12.descriptorIndexing && f12.runtimeDescriptorArray &&
           f12.descriptorBindingPartiallyBound && f12.shaderSampledImageArrayNonUniformIndexing &&
           f12.descriptorBindingSampledImageUpdateAfterBind &&
           f12.descriptorBindingStorageImageUpdateAfterBind && f12.scalarBlockLayout && f12.hostQueryReset &&
           f2.features.shaderInt64 && f2.features.shaderStorageImageWriteWithoutFormat;
}

int device_score(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    switch (props.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
    default: return 0; // CPU (lavapipe) — fine for tests, last choice otherwise
    }
}

AdapterType to_adapter_type(VkPhysicalDeviceType type) noexcept {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return AdapterType::IntegratedGpu;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return AdapterType::DiscreteGpu;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return AdapterType::VirtualGpu;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return AdapterType::Cpu;
    default: return AdapterType::Other;
    }
}

std::string bounded_string(const char* text, std::size_t max_len) {
    std::size_t len = 0;
    while (len < max_len && text[len] != '\0') ++len;
    return std::string(text, len);
}

bool create_instance(Device::Impl& s, const DeviceDesc& desc) {
    std::vector<const char*> layers;
    std::vector<const char*> extensions;
    s.caps.validation = desc.validation && has_layer(validation_layer);
    if (s.caps.validation) layers.push_back(validation_layer);
    s.caps.debug_utils = has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (s.caps.debug_utils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (desc.surface) {
        for (const auto& ext : desc.surface->instance_extensions) extensions.push_back(ext.c_str());
    }

    const std::string app_name(desc.app_name);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = app_name.c_str();
    app.pEngineName = "AstralEngine";
    app.apiVersion = VK_API_VERSION_1_3;

    VkDebugUtilsMessengerCreateInfoEXT messenger_info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messenger_info.pfnUserCallback = debug_callback;
    messenger_info.pUserData = desc.logger;

    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    info.ppEnabledLayerNames = layers.data();
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (s.caps.validation && s.caps.debug_utils) info.pNext = &messenger_info;
    if (vkCreateInstance(&info, nullptr, &s.instance) != VK_SUCCESS) return false;

    if (s.caps.validation && s.caps.debug_utils) {
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(s.instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create) create(s.instance, &messenger_info, nullptr, &s.messenger);
    }
    return true;
}

bool create_surface(Device::Impl& s, const DeviceDesc& desc) {
    if (!desc.surface) return true;
    if (!desc.surface->create_surface) return false;
    s.surface = reinterpret_cast<VkSurfaceKHR>(desc.surface->create_surface(s.instance));
    return s.surface != VK_NULL_HANDLE;
}

bool pick_physical_device(Device::Impl& s) {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(s.instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(s.instance, &count, devices.data());

    int best_score = -1;
    for (VkPhysicalDevice candidate : devices) {
        if (!meets_requirements(candidate)) continue;
        if (s.surface && !has_device_extension(candidate, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;

        std::uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
        for (std::uint32_t i = 0; i < family_count; ++i) {
            const auto needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((families[i].queueFlags & needed) != needed) continue;
            if (s.surface) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, s.surface, &present);
                if (!present) continue;
            }
            const int score = device_score(candidate);
            if (score > best_score) {
                best_score = score;
                s.physical = candidate;
                s.queue_family = i;
            }
            break;
        }
    }
    if (!s.physical) return false;
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &p12};
    vkGetPhysicalDeviceProperties2(s.physical, &p2);
    const VkPhysicalDeviceProperties& props = p2.properties;
    s.adapter.name = props.deviceName;
    s.adapter.vendor_id = props.vendorID;
    s.adapter.device_id = props.deviceID;
    s.adapter.driver_id = static_cast<std::uint32_t>(p12.driverID);
    s.adapter.driver_name = bounded_string(p12.driverName, VK_MAX_DRIVER_NAME_SIZE);
    s.adapter.type = to_adapter_type(props.deviceType);
    s.caps.presentation = s.surface != VK_NULL_HANDLE;
    {
        std::uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(s.physical, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(s.physical, &family_count, families.data());
        const std::uint32_t bits = s.queue_family < family_count ? families[s.queue_family].timestampValidBits : 0;
        s.caps.timestamps = props.limits.timestampComputeAndGraphics == VK_TRUE && bits > 0;
        s.caps.timestamp_period = props.limits.timestampPeriod;
        s.caps.timestamp_mask = bits >= 64 ? ~0ull : ((1ull << bits) - 1);
    }
    return true;
}

void load_optional_dispatch(Device::Impl& s) {
    if (s.caps.debug_utils) {
        s.debug.cmd_begin_label = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(s.device, "vkCmdBeginDebugUtilsLabelEXT"));
        s.debug.cmd_end_label = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(s.device, "vkCmdEndDebugUtilsLabelEXT"));
        s.debug.set_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(s.device, "vkSetDebugUtilsObjectNameEXT"));
    }
    if (s.rt.supported) {
        auto load = [&](const char* name) { return vkGetDeviceProcAddr(s.device, name); };
        s.rt.create_as = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(load("vkCreateAccelerationStructureKHR"));
        s.rt.destroy_as = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(load("vkDestroyAccelerationStructureKHR"));
        s.rt.as_build_sizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            load("vkGetAccelerationStructureBuildSizesKHR"));
        s.rt.cmd_build_as = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(load("vkCmdBuildAccelerationStructuresKHR"));
        s.rt.as_address = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            load("vkGetAccelerationStructureDeviceAddressKHR"));
        VkPhysicalDeviceAccelerationStructurePropertiesKHR as_props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &as_props};
        vkGetPhysicalDeviceProperties2(s.physical, &props);
        s.rt.scratch_alignment = std::max(1u, as_props.minAccelerationStructureScratchOffsetAlignment);
        s.rt.supported = s.rt.create_as && s.rt.destroy_as && s.rt.as_build_sizes && s.rt.cmd_build_as && s.rt.as_address;
    }
}

bool create_logical_device(Device::Impl& s) {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = s.queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;

    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13};
    f12.timelineSemaphore = VK_TRUE;
    f12.bufferDeviceAddress = VK_TRUE;
    f12.descriptorIndexing = VK_TRUE;
    f12.runtimeDescriptorArray = VK_TRUE;
    f12.descriptorBindingPartiallyBound = VK_TRUE;
    f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    f12.scalarBlockLayout = VK_TRUE;
    f12.hostQueryReset = VK_TRUE; // timestamp pools start "not available"
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12};
    f2.features.shaderInt64 = VK_TRUE;
    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(s.physical, &supported);
    s.caps.anisotropy = supported.samplerAnisotropy == VK_TRUE;
    f2.features.samplerAnisotropy = s.caps.anisotropy ? VK_TRUE : VK_FALSE;
    s.caps.texture_compression_bc = supported.textureCompressionBC == VK_TRUE;
    f2.features.textureCompressionBC = s.caps.texture_compression_bc ? VK_TRUE : VK_FALSE;
    f2.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

    std::vector<const char*> extensions;
    if (s.surface) extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // Ray tracing via ray queries, when the driver has it.
    VkPhysicalDeviceAccelerationStructureFeaturesKHR as_query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR rq_query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, &as_query};
    VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &rq_query};
    const bool rt_extensions = has_device_extension(s.physical, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                               has_device_extension(s.physical, VK_KHR_RAY_QUERY_EXTENSION_NAME) &&
                               has_device_extension(s.physical, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    if (rt_extensions) vkGetPhysicalDeviceFeatures2(s.physical, &query);
    VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR rq_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, &as_features};
    s.rt.supported = rt_extensions && as_query.accelerationStructure && rq_query.rayQuery;
    if (s.rt.supported) {
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        as_features.accelerationStructure = VK_TRUE;
        rq_features.rayQuery = VK_TRUE;
        f13.pNext = &rq_features;
    }

    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (vkCreateDevice(s.physical, &info, nullptr, &s.device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(s.device, s.queue_family, 0, &s.queue);
    load_optional_dispatch(s);

    VmaAllocatorCreateInfo vma{};
    vma.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vma.physicalDevice = s.physical;
    vma.device = s.device;
    vma.instance = s.instance;
    vma.vulkanApiVersion = VK_API_VERSION_1_3;
    return vmaCreateAllocator(&vma, &s.allocator) == VK_SUCCESS;
}

bool create_sync_and_commands(Device::Impl& s) {
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
    if (vkCreateSemaphore(s.device, &sem, nullptr, &s.timeline) != VK_SUCCESS) return false;

    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = s.queue_family;
    return vkCreateCommandPool(s.device, &pool, nullptr, &s.command_pool) == VK_SUCCESS;
}

bool create_bindless(Device::Impl& s) {
    auto& b = s.bindless;
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &p12};
    vkGetPhysicalDeviceProperties2(s.physical, &p2);
    b.max_textures = std::min({16384u, p12.maxDescriptorSetUpdateAfterBindSampledImages,
                               p12.maxDescriptorSetUpdateAfterBindStorageImages});

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, b.max_textures, VK_SHADER_STAGE_ALL, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, b.max_textures, VK_SHADER_STAGE_ALL, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_SAMPLER, 3, VK_SHADER_STAGE_ALL, nullptr};
    const VkDescriptorBindingFlags bind_flags[3] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0};
    VkDescriptorSetLayoutBindingFlagsCreateInfo flags{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags.bindingCount = 3;
    flags.pBindingFlags = bind_flags;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &flags};
    layout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout.bindingCount = 3;
    layout.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(s.device, &layout, nullptr, &b.set_layout) != VK_SUCCESS) return false;

    VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, b.max_textures},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, b.max_textures},
                                     {VK_DESCRIPTOR_TYPE_SAMPLER, 3}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool.maxSets = 1;
    pool.poolSizeCount = 3;
    pool.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(s.device, &pool, nullptr, &b.descriptor_pool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = b.descriptor_pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &b.set_layout;
    if (vkAllocateDescriptorSets(s.device, &alloc, &b.descriptor_set) != VK_SUCCESS) return false;

    // Fixed samplers, in gpu::Sampler order.
    const VkFilter filters[3] = {VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_FILTER_NEAREST};
    const VkSamplerAddressMode modes[3] = {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
    VkDescriptorImageInfo sampler_infos[3]{};
    for (int i = 0; i < 3; ++i) {
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = info.minFilter = filters[i];
        info.mipmapMode = filters[i] == VK_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                                         : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = info.addressModeV = info.addressModeW = modes[i];
        info.maxLod = VK_LOD_CLAMP_NONE;
        if (i == static_cast<int>(Sampler::LinearRepeat) && s.caps.anisotropy) { // material textures
            info.anisotropyEnable = VK_TRUE;
            info.maxAnisotropy = 8.0f;
        }
        if (vkCreateSampler(s.device, &info, nullptr, &b.samplers[i]) != VK_SUCCESS) return false;
        sampler_infos[i].sampler = b.samplers[i];
    }
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = b.descriptor_set;
    write.dstBinding = 2;
    write.descriptorCount = 3;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo = sampler_infos;
    vkUpdateDescriptorSets(s.device, 1, &write, 0, nullptr);

    VkPushConstantRange push{VK_SHADER_STAGE_ALL, 0, max_push_constant_size};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &b.set_layout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    return vkCreatePipelineLayout(s.device, &pl, nullptr, &b.pipeline_layout) == VK_SUCCESS;
}

} // namespace

bool has_device_extension(VkPhysicalDevice device, const char* name) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());
    return std::any_of(exts.begin(), exts.end(), [&](const VkExtensionProperties& e) {
        return std::strcmp(e.extensionName, name) == 0;
    });
}

bool init_vulkan_device(Device::Impl& s, const DeviceDesc& desc) {
    if (!create_instance(s, desc)) {
        s.log_error("vkCreateInstance failed (is a Vulkan driver installed?)");
        return false;
    }
    if (!create_surface(s, desc)) {
        s.log_error("could not create a Vulkan surface for the window");
        return false;
    }
    if (!pick_physical_device(s)) {
        s.log_error("no Vulkan 1.3 device with the required features");
        return false;
    }
    if (!create_logical_device(s) || !create_sync_and_commands(s) || !create_bindless(s)) {
        s.log_error("failed to initialise the Vulkan device");
        return false;
    }
    return true;
}

} // namespace astral::gpu
