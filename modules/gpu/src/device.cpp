#include "vk_device.hpp"

#include <astral/core/log.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

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

bool has_device_extension(VkPhysicalDevice device, const char* name) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());
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
           f12.descriptorBindingStorageImageUpdateAfterBind && f12.scalarBlockLayout &&
           f2.features.shaderInt64;
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

} // namespace

// --- Impl helpers ----------------------------------------------------------------------

void Device::Impl::log_error(std::string_view message) const {
    if (logger) {
        logger->log(LogLevel::Error, message);
    } else {
        std::fprintf(stderr, "[astral::gpu] %.*s\n", static_cast<int>(message.size()),
                     message.data());
    }
}

std::uint64_t Device::Impl::completed_value() const {
    std::uint64_t value = 0;
    vkGetSemaphoreCounterValue(device, timeline, &value);
    return value;
}

void Device::Impl::retire(std::function<void()> destroy) {
    // A command list being recorded right now may reference the resource and will be
    // submitted as `last_submitted + 1`: wait for that one too.
    garbage.push_back({last_submitted + 1, std::move(destroy)});
}

bool Device::Impl::any_recording() const noexcept {
    for (const auto& c : command_slots) {
        if (c.recording) return true;
    }
    return false;
}

bool Device::Impl::can_run_immediate(const char* operation) const {
    if (!any_recording()) return true;
    log_error(std::string(operation) + ": refused while a CommandList is being recorded "
                                       "(submit it first, or use a host-visible buffer)");
    return false;
}

void Device::Impl::collect_garbage() {
    const std::uint64_t done = completed_value();
    while (!garbage.empty() && garbage.front().value <= done) {
        garbage.front().destroy();
        garbage.pop_front();
    }
}

void Device::Impl::wait_value(std::uint64_t value) {
    if (value == 0) return;
    VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    info.semaphoreCount = 1;
    info.pSemaphores = &timeline;
    info.pValues = &value;
    vkWaitSemaphores(device, &info, UINT64_MAX);
}

void Device::Impl::name_object(VkObjectType type, std::uint64_t handle, std::string_view name) {
    if (!set_object_name || name.empty()) return;
    const std::string copy(name);
    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = copy.c_str();
    set_object_name(device, &info);
}

std::uint32_t Device::Impl::allocate_bindless() {
    if (!free_bindless.empty()) {
        const std::uint32_t index = free_bindless.back();
        free_bindless.pop_back();
        return index;
    }
    return next_bindless < max_textures ? next_bindless++ : UINT32_MAX;
}

void Device::Impl::free_bindless_index(std::uint32_t index) {
    if (index != UINT32_MAX) free_bindless.push_back(index);
}

void Device::Impl::write_texture_descriptors(const TextureRecord& texture) {
    VkDescriptorImageInfo sampled{VK_NULL_HANDLE, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo storage{VK_NULL_HANDLE, texture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet writes[2]{};
    std::uint32_t count = 0;
    if (has_flag(texture.desc.usage, TextureUsage::Sampled)) {
        auto& w = writes[count++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descriptor_set;
        w.dstBinding = 0;
        w.dstArrayElement = texture.bindless;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w.pImageInfo = &sampled;
    }
    if (has_flag(texture.desc.usage, TextureUsage::Storage)) {
        auto& w = writes[count++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descriptor_set;
        w.dstBinding = 1;
        w.dstArrayElement = texture.bindless;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo = &storage;
    }
    if (count) vkUpdateDescriptorSets(device, count, writes, 0, nullptr);
}

void Device::Impl::immediate(const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc, &cmd);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    // Order against everything submitted before and after (uploads are rare: simplicity wins).
    VkMemoryBarrier2 full{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    full.srcStageMask = full.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    full.srcAccessMask = full.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &full;
    vkCmdPipelineBarrier2(cmd, &dep);
    record(cmd);
    full.dstAccessMask |= VK_ACCESS_2_HOST_READ_BIT;
    full.dstStageMask |= VK_PIPELINE_STAGE_2_HOST_BIT;
    vkCmdPipelineBarrier2(cmd, &dep);
    vkEndCommandBuffer(cmd);

    const std::uint64_t value = ++last_submitted;
    VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmd_info.commandBuffer = cmd;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = timeline;
    signal.value = value;
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmd_info;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
    vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE);
    wait_value(value);
    vkFreeCommandBuffers(device, command_pool, 1, &cmd);
}

Device::Impl::~Impl() {
    if (device) {
        vkDeviceWaitIdle(device);
        while (!garbage.empty()) {
            garbage.front().destroy();
            garbage.pop_front();
        }
        for (auto& sampler : samplers) vkDestroySampler(device, sampler, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroySemaphore(device, timeline, nullptr);
        if (allocator) vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
    }
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (messenger) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(instance, messenger, nullptr);
    }
    if (instance) vkDestroyInstance(instance, nullptr);
}

// --- Creation --------------------------------------------------------------------------

namespace {

bool create_instance(Device::Impl& s, const DeviceDesc& desc) {
    std::vector<const char*> layers;
    std::vector<const char*> extensions;
    s.validation = desc.validation && has_layer(validation_layer);
    if (s.validation) layers.push_back(validation_layer);
    s.debug_utils = has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (s.debug_utils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
    if (s.validation && s.debug_utils) info.pNext = &messenger_info;
    if (vkCreateInstance(&info, nullptr, &s.instance) != VK_SUCCESS) return false;

    if (s.validation && s.debug_utils) {
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
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(s.physical, &props);
    s.adapter = props.deviceName;
    s.presentation = s.surface != VK_NULL_HANDLE;
    return true;
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
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12};
    f2.features.shaderInt64 = VK_TRUE;

    std::vector<const char*> extensions;
    if (s.surface) extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (vkCreateDevice(s.physical, &info, nullptr, &s.device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(s.device, s.queue_family, 0, &s.queue);

    if (s.debug_utils) {
        s.cmd_begin_label = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(s.device, "vkCmdBeginDebugUtilsLabelEXT"));
        s.cmd_end_label = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(s.device, "vkCmdEndDebugUtilsLabelEXT"));
        s.set_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(s.device, "vkSetDebugUtilsObjectNameEXT"));
    }

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
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &p12};
    vkGetPhysicalDeviceProperties2(s.physical, &p2);
    s.max_textures = std::min({16384u, p12.maxDescriptorSetUpdateAfterBindSampledImages,
                               p12.maxDescriptorSetUpdateAfterBindStorageImages});

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, s.max_textures, VK_SHADER_STAGE_ALL, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s.max_textures, VK_SHADER_STAGE_ALL, nullptr};
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
    if (vkCreateDescriptorSetLayout(s.device, &layout, nullptr, &s.set_layout) != VK_SUCCESS) return false;

    VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, s.max_textures},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s.max_textures},
                                     {VK_DESCRIPTOR_TYPE_SAMPLER, 3}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool.maxSets = 1;
    pool.poolSizeCount = 3;
    pool.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(s.device, &pool, nullptr, &s.descriptor_pool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = s.descriptor_pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &s.set_layout;
    if (vkAllocateDescriptorSets(s.device, &alloc, &s.descriptor_set) != VK_SUCCESS) return false;

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
        if (vkCreateSampler(s.device, &info, nullptr, &s.samplers[i]) != VK_SUCCESS) return false;
        sampler_infos[i].sampler = s.samplers[i];
    }
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = s.descriptor_set;
    write.dstBinding = 2;
    write.descriptorCount = 3;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo = sampler_infos;
    vkUpdateDescriptorSets(s.device, 1, &write, 0, nullptr);

    VkPushConstantRange push{VK_SHADER_STAGE_ALL, 0, max_push_constant_size};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &s.set_layout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    return vkCreatePipelineLayout(s.device, &pl, nullptr, &s.pipeline_layout) == VK_SUCCESS;
}

} // namespace

std::unique_ptr<Device> Device::create(const DeviceDesc& desc) {
    auto state = std::make_unique<Impl>();
    state->logger = desc.logger;
    if (!create_instance(*state, desc)) {
        state->log_error("vkCreateInstance failed (is a Vulkan driver installed?)");
        return nullptr;
    }
    if (!create_surface(*state, desc)) {
        state->log_error("could not create a Vulkan surface for the window");
        return nullptr;
    }
    if (!pick_physical_device(*state)) {
        state->log_error("no Vulkan 1.3 device with the required features");
        return nullptr;
    }
    if (!create_logical_device(*state) || !create_sync_and_commands(*state) ||
        !create_bindless(*state)) {
        state->log_error("failed to initialise the Vulkan device");
        return nullptr;
    }
    return std::unique_ptr<Device>(new Device(std::move(state)));
}

Device::Device(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Device::~Device() {
    if (!impl_ || !impl_->device) return;
    auto& s = *impl_;
    vkDeviceWaitIdle(s.device);
    s.collect_garbage();
    // Resources the user never destroyed are released here.
    s.buffers.for_each([&](BufferHandle, BufferRecord& r) { destroy_buffer_record(s, r); });
    s.textures.for_each([&](TextureHandle, TextureRecord& r) { destroy_texture_record(s, r); });
    s.pipelines.for_each([&](PipelineHandle, PipelineRecord& r) {
        vkDestroyPipeline(s.device, r.pipeline, nullptr);
    });
}

std::string_view Device::adapter_name() const noexcept { return impl_->adapter; }

bool Device::supports(Feature feature) const noexcept {
    switch (feature) {
    case Feature::BufferDeviceAddress:
    case Feature::Bindless: return true;
    case Feature::Validation: return impl_->validation;
    case Feature::Presentation: return impl_->presentation;
    case Feature::RayTracing:
        return has_device_extension(impl_->physical, "VK_KHR_ray_query");
    case Feature::MeshShaders:
        return has_device_extension(impl_->physical, "VK_EXT_mesh_shader");
    }
    return false;
}

// --- Work submission -------------------------------------------------------------------

CommandList Device::begin_commands() {
    auto& s = *impl_;
    s.collect_garbage();
    const std::uint64_t done = s.completed_value();
    std::uint32_t slot = UINT32_MAX;
    for (std::uint32_t i = 0; i < s.command_slots.size(); ++i) {
        const auto& c = s.command_slots[i];
        if (!c.recording && c.ticket <= done) {
            slot = i;
            break;
        }
    }
    if (slot == UINT32_MAX) {
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool = s.command_pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        Impl::CommandSlot fresh;
        vkAllocateCommandBuffers(s.device, &alloc, &fresh.buffer);
        s.command_slots.push_back(fresh);
        slot = static_cast<std::uint32_t>(s.command_slots.size() - 1);
    }
    auto& c = s.command_slots[slot];
    c.recording = true;
    vkResetCommandBuffer(c.buffer, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(c.buffer, &begin);
    // Bindless set is bound once for both bind points; every pipeline shares the layout.
    vkCmdBindDescriptorSets(c.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline_layout, 0, 1,
                            &s.descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(c.buffer, VK_PIPELINE_BIND_POINT_COMPUTE, s.pipeline_layout, 0, 1,
                            &s.descriptor_set, 0, nullptr);
    return CommandList(this, c.buffer, slot);
}

SubmitTicket Device::submit(CommandList& commands) {
    return submit_internal(commands, nullptr, 0, nullptr, 0);
}

SubmitTicket Device::submit_internal(CommandList& commands, const void* waits, std::uint32_t wait_count,
                                     const void* signals, std::uint32_t signal_count) {
    auto& s = *impl_;
    if (!commands.valid()) return {s.last_submitted};
    if (commands.rendering_) commands.end_rendering();
    auto& slot = s.command_slots[commands.slot_];
    vkEndCommandBuffer(slot.buffer);

    const std::uint64_t value = s.last_submitted + 1;
    VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmd_info.commandBuffer = slot.buffer;
    std::vector<VkSemaphoreSubmitInfo> signal_infos;
    if (signal_count) {
        auto* begin = static_cast<const VkSemaphoreSubmitInfo*>(signals);
        signal_infos.assign(begin, begin + signal_count);
    }
    VkSemaphoreSubmitInfo timeline_signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    timeline_signal.semaphore = s.timeline;
    timeline_signal.value = value;
    timeline_signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signal_infos.push_back(timeline_signal);

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = wait_count;
    submit.pWaitSemaphoreInfos = static_cast<const VkSemaphoreSubmitInfo*>(waits);
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmd_info;
    submit.signalSemaphoreInfoCount = static_cast<std::uint32_t>(signal_infos.size());
    submit.pSignalSemaphoreInfos = signal_infos.data();
    slot.recording = false;
    commands.device_ = nullptr; // consumed
    commands.touched_.clear();
    auto callbacks = std::move(commands.on_submitted_);
    commands.on_submitted_.clear();
    if (vkQueueSubmit2(s.queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
        s.log_error("vkQueueSubmit2 failed (device lost?)");
        slot.ticket = 0;
        return {s.last_submitted}; // nothing will signal `value`: do not advance
    }
    s.last_submitted = value;
    slot.ticket = value;
    for (auto& callback : callbacks) callback({value});
    return {value};
}

void Device::wait(SubmitTicket ticket) {
    impl_->wait_value(ticket.value);
    impl_->collect_garbage();
}

bool Device::is_complete(SubmitTicket ticket) const {
    return impl_->completed_value() >= ticket.value;
}

void Device::wait_idle() {
    auto& s = *impl_;
    vkDeviceWaitIdle(s.device);
    s.collect_garbage();
    if (!s.any_recording()) {
        // Nothing can reference retired resources any more.
        while (!s.garbage.empty()) {
            s.garbage.front().destroy();
            s.garbage.pop_front();
        }
    }
}

} // namespace astral::gpu
