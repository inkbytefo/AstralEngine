#include "VulkanUtils.h"
#include "../../Core/Logger.h"
#include <algorithm>
#include <iomanip>
#include <unordered_map>
#include <sstream>

namespace AstralEngine {

// VkResult string mapping
static const std::unordered_map<VkResult, const char*> vkResultStrings = {
    {VK_SUCCESS, "VK_SUCCESS"},
    {VK_NOT_READY, "VK_NOT_READY"},
    {VK_TIMEOUT, "VK_TIMEOUT"},
    {VK_EVENT_SET, "VK_EVENT_SET"},
    {VK_EVENT_RESET, "VK_EVENT_RESET"},
    {VK_INCOMPLETE, "VK_INCOMPLETE"},
    {VK_ERROR_OUT_OF_HOST_MEMORY, "VK_ERROR_OUT_OF_HOST_MEMORY"},
    {VK_ERROR_OUT_OF_DEVICE_MEMORY, "VK_ERROR_OUT_OF_DEVICE_MEMORY"},
    {VK_ERROR_INITIALIZATION_FAILED, "VK_ERROR_INITIALIZATION_FAILED"},
    {VK_ERROR_DEVICE_LOST, "VK_ERROR_DEVICE_LOST"},
    {VK_ERROR_MEMORY_MAP_FAILED, "VK_ERROR_MEMORY_MAP_FAILED"},
    {VK_ERROR_LAYER_NOT_PRESENT, "VK_ERROR_LAYER_NOT_PRESENT"},
    {VK_ERROR_EXTENSION_NOT_PRESENT, "VK_ERROR_EXTENSION_NOT_PRESENT"},
    {VK_ERROR_FEATURE_NOT_PRESENT, "VK_ERROR_FEATURE_NOT_PRESENT"},
    {VK_ERROR_INCOMPATIBLE_DRIVER, "VK_ERROR_INCOMPATIBLE_DRIVER"},
    {VK_ERROR_TOO_MANY_OBJECTS, "VK_ERROR_TOO_MANY_OBJECTS"},
    {VK_ERROR_FORMAT_NOT_SUPPORTED, "VK_ERROR_FORMAT_NOT_SUPPORTED"},
    {VK_ERROR_FRAGMENTED_POOL, "VK_ERROR_FRAGMENTED_POOL"},
    {VK_ERROR_UNKNOWN, "VK_ERROR_UNKNOWN"},
    {VK_ERROR_OUT_OF_POOL_MEMORY, "VK_ERROR_OUT_OF_POOL_MEMORY"},
    {VK_ERROR_INVALID_EXTERNAL_HANDLE, "VK_ERROR_INVALID_EXTERNAL_HANDLE"},
    {VK_ERROR_FRAGMENTATION, "VK_ERROR_FRAGMENTATION"},
    {VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"},
    {VK_PIPELINE_COMPILE_REQUIRED, "VK_PIPELINE_COMPILE_REQUIRED"},
    {VK_ERROR_SURFACE_LOST_KHR, "VK_ERROR_SURFACE_LOST_KHR"},
    {VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"},
    {VK_SUBOPTIMAL_KHR, "VK_SUBOPTIMAL_KHR"},
    {VK_ERROR_OUT_OF_DATE_KHR, "VK_ERROR_OUT_OF_DATE_KHR"},
    {VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"},
    {VK_ERROR_VALIDATION_FAILED_EXT, "VK_ERROR_VALIDATION_FAILED_EXT"},
    {VK_ERROR_INVALID_SHADER_NV, "VK_ERROR_INVALID_SHADER_NV"},
    {VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT, "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"},
    {VK_ERROR_NOT_PERMITTED_KHR, "VK_ERROR_NOT_PERMITTED_KHR"},
    {VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"},
    {VK_THREAD_IDLE_KHR, "VK_THREAD_IDLE_KHR"},
    {VK_THREAD_DONE_KHR, "VK_THREAD_DONE_KHR"},
    {VK_OPERATION_DEFERRED_KHR, "VK_OPERATION_DEFERRED_KHR"},
    {VK_OPERATION_NOT_DEFERRED_KHR, "VK_OPERATION_NOT_DEFERRED_KHR"},
    {VK_ERROR_COMPRESSION_EXHAUSTED_EXT, "VK_ERROR_COMPRESSION_EXHAUSTED_EXT"},
    {VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR, "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR"}
};

bool VulkanUtils::CheckVkResult(VkResult result, const std::string& operation, bool throwOnError) {
    if (result == VK_SUCCESS) {
        return true;
    }
    
    std::string errorStr = GetVkResultString(result);
    std::string errorMsg = "Vulkan Error: " + errorStr + " during operation: " + operation;
    
    LogError(errorMsg, __FILE__, __LINE__);
    
    if (throwOnError) {
        throw VulkanResultException(result, operation);
    }
    
    return false;
}

std::string VulkanUtils::GetVkResultString(VkResult result) {
    auto it = vkResultStrings.find(result);
    if (it != vkResultStrings.end()) {
        return it->second;
    }
    
    std::ostringstream oss;
    oss << "UNKNOWN_VK_RESULT_" << static_cast<int>(result);
    return oss.str();
}

bool VulkanUtils::IsFormatSupported(VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    if (physicalDevice == VK_NULL_HANDLE) return false;
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return true;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
        return true;
    }
    return false;
}

VkFormat VulkanUtils::FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        if (IsFormatSupported(physicalDevice, format, tiling, features)) {
            return format;
        }
    }
    LogWarning("No supported format found from candidates", __FILE__, __LINE__);
    return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanUtils::FindDepthFormat(VkPhysicalDevice physicalDevice) {
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    
    return FindSupportedFormat(physicalDevice, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkImageAspectFlags VulkanUtils::GetImageAspectFlags(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

std::string VulkanUtils::GetShaderStageString(VkShaderStageFlags stage) {
    std::vector<std::pair<VkShaderStageFlagBits, const char*>> stageNames = {
        {VK_SHADER_STAGE_VERTEX_BIT, "VERTEX"},
        {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "TESSELLATION_CONTROL"},
        {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "TESSELLATION_EVALUATION"},
        {VK_SHADER_STAGE_GEOMETRY_BIT, "GEOMETRY"},
        {VK_SHADER_STAGE_FRAGMENT_BIT, "FRAGMENT"},
        {VK_SHADER_STAGE_COMPUTE_BIT, "COMPUTE"},
        {VK_SHADER_STAGE_RAYGEN_BIT_KHR, "RAYGEN"},
        {VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "ANY_HIT"},
        {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "CLOSEST_HIT"},
        {VK_SHADER_STAGE_MISS_BIT_KHR, "MISS"},
        {VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "INTERSECTION"},
        {VK_SHADER_STAGE_CALLABLE_BIT_KHR, "CALLABLE"},
        {VK_SHADER_STAGE_TASK_BIT_NV, "TASK"},
        {VK_SHADER_STAGE_MESH_BIT_NV, "MESH"}
    };
    
    return FlagsToString(stage, stageNames);
}

std::string VulkanUtils::GetBufferUsageString(VkBufferUsageFlags usage) {
    std::vector<std::pair<VkBufferUsageFlagBits, const char*>> usageNames = {
        {VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "TRANSFER_SRC"},
        {VK_BUFFER_USAGE_TRANSFER_DST_BIT, "TRANSFER_DST"},
        {VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, "UNIFORM_TEXEL_BUFFER"},
        {VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, "STORAGE_TEXEL_BUFFER"},
        {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "UNIFORM_BUFFER"},
        {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "STORAGE_BUFFER"},
        {VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "INDEX_BUFFER"},
        {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "VERTEX_BUFFER"},
        {VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "INDIRECT_BUFFER"},
        {VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, "SHADER_DEVICE_ADDRESS"},
        {VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT, "TRANSFORM_FEEDBACK_BUFFER"},
        {VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT, "TRANSFORM_FEEDBACK_COUNTER_BUFFER"},
        {VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT, "CONDITIONAL_RENDERING"},
        {VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, "RAY_TRACING"},
        {VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, "SHADER_BINDING_TABLE"},
        {VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, "ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY"},
        {VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, "ACCELERATION_STRUCTURE_STORAGE"},
        {VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT, "SAMPLER_DESCRIPTOR_BUFFER"},
        {VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, "RESOURCE_DESCRIPTOR_BUFFER"},
        {VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT, "PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER"}
    };
    
    return FlagsToString(usage, usageNames);
}

std::string VulkanUtils::GetImageUsageString(VkImageUsageFlags usage) {
    std::vector<std::pair<VkImageUsageFlagBits, const char*>> usageNames = {
        {VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "TRANSFER_SRC"},
        {VK_IMAGE_USAGE_TRANSFER_DST_BIT, "TRANSFER_DST"},
        {VK_IMAGE_USAGE_SAMPLED_BIT, "SAMPLED"},
        {VK_IMAGE_USAGE_STORAGE_BIT, "STORAGE"},
        {VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "COLOR_ATTACHMENT"},
        {VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "DEPTH_STENCIL_ATTACHMENT"},
        {VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, "TRANSIENT_ATTACHMENT"},
        {VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, "INPUT_ATTACHMENT"},
        {VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV, "SHADING_RATE_IMAGE"},
        {VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT, "FRAGMENT_DENSITY_MAP"},
        {VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, "FRAGMENT_SHADING_RATE_ATTACHMENT"},
        {VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR, "VIDEO_DECODE_DST"},
        {VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR, "VIDEO_DECODE_SRC"},
        {VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR, "VIDEO_DECODE_DPB"},
        {VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI, "INVOCATION_MASK"},
        {VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM, "SAMPLE_WEIGHT"},
        {VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM, "SAMPLE_BLOCK_MATCH"}
    };
    
    return FlagsToString(usage, usageNames);
}

bool VulkanUtils::AreExtensionsSupported([[maybe_unused]] VkInstance instance, const std::vector<const char*>& extensions) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    
    for (const char* extension : extensions) {
        bool found = false;
        for (const auto& ext : availableExtensions) {
            if (strcmp(extension, ext.extensionName) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            LogWarning("Extension not supported: " + std::string(extension), __FILE__, __LINE__);
            return false;
        }
    }
    
    return true;
}

bool VulkanUtils::AreValidationLayersSupported(const std::vector<const char*>& layers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : layers) {
        bool layerFound = false;
        
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            LogWarning("Validation layer not supported: " + std::string(layerName), __FILE__, __LINE__);
            return false;
        }
    }
    
    return true;
}

std::vector<const char*> VulkanUtils::GetRequiredInstanceExtensions(bool enableValidationLayers) {
    std::vector<const char*> extensions;
    
    // SDL3 için gerekli extension'lar
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    
    // Platforma özel surface extension'ları
#ifdef _WIN32
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
    extensions.push_back(VK_KHR_METAL_SURFACE_EXTENSION_NAME);
#endif
    
    // Debug extension'ları
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Modern Vulkan extension'ları
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    
    return extensions;
}

std::vector<const char*> VulkanUtils::GetRequiredDeviceExtensions() {
    std::vector<const char*> extensions;
    
    // Swapchain extension'ı
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    // Modern Vulkan extension'ları
    extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    
    // Performans için ek extension'lar
    extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    
    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanUtils::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    [[maybe_unused]] void* pUserData) {
    
    std::string severity;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity = "VERBOSE";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity = "INFO";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity = "WARNING";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity = "ERROR";
    }
    
    std::string type;
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type = "PERFORMANCE";
    }
    
    std::string message = "[" + severity + "][" + type + "] " + pCallbackData->pMessage;
    
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LogError(message, __FILE__, __LINE__);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LogWarning(message, __FILE__, __LINE__);
    } else {
        LogDebug(message, __FILE__, __LINE__);
    }
    
    return VK_FALSE;
}

void VulkanUtils::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info) {
    info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;
    info.pUserData = nullptr;
}

void VulkanUtils::LogError(const std::string& message, const std::string& file, int line) {
    if (file.empty()) {
        Logger::Error("VulkanUtils", message);
    } else {
        Logger::Error("VulkanUtils", message + " (" + file + ":" + std::to_string(line) + ")");
    }
}

void VulkanUtils::LogWarning(const std::string& message, const std::string& file, int line) {
    if (file.empty()) {
        Logger::Warning("VulkanUtils", message);
    } else {
        Logger::Warning("VulkanUtils", message + " (" + file + ":" + std::to_string(line) + ")");
    }
}

void VulkanUtils::LogInfo(const std::string& message, const std::string& file, int line) {
    if (file.empty()) {
        Logger::Info("VulkanUtils", message);
    } else {
        Logger::Info("VulkanUtils", message + " (" + file + ":" + std::to_string(line) + ")");
    }
}

void VulkanUtils::LogDebug(const std::string& message, const std::string& file, int line) {
    if (file.empty()) {
        Logger::Debug("VulkanUtils", message);
    } else {
        Logger::Debug("VulkanUtils", message + " (" + file + ":" + std::to_string(line) + ")");
    }
}

std::string VulkanUtils::FormatMemorySize(VkDeviceSize bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

std::string VulkanUtils::FormatVersion(uint32_t version) {
    std::ostringstream oss;
    oss << VK_VERSION_MAJOR(version) << "."
        << VK_VERSION_MINOR(version) << "."
        << VK_VERSION_PATCH(version);
    return oss.str();
}

std::string VulkanUtils::GetFormatFeatureString(VkFormatFeatureFlags features) {
    std::vector<std::pair<VkFormatFeatureFlagBits, const char*>> featureNames = {
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "SAMPLED_IMAGE"},
        {VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "STORAGE_IMAGE"},
        {VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT, "STORAGE_IMAGE_ATOMIC"},
        {VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, "UNIFORM_TEXEL_BUFFER"},
        {VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT, "STORAGE_TEXEL_BUFFER"},
        {VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT, "STORAGE_TEXEL_BUFFER_ATOMIC"},
        {VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT, "VERTEX_BUFFER"},
        {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "COLOR_ATTACHMENT"},
        {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, "COLOR_ATTACHMENT_BLEND"},
        {VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "DEPTH_STENCIL_ATTACHMENT"},
        {VK_FORMAT_FEATURE_BLIT_SRC_BIT, "BLIT_SRC"},
        {VK_FORMAT_FEATURE_BLIT_DST_BIT, "BLIT_DST"},
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "SAMPLED_IMAGE_FILTER_LINEAR"},
        {VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "TRANSFER_SRC"},
        {VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "TRANSFER_DST"},
        {VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT, "MIDPOINT_CHROMA_SAMPLES"},
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT, "SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER"},
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT, "SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER"},
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT, "SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT"},
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT, "SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE"},
        {VK_FORMAT_FEATURE_DISJOINT_BIT, "DISJOINT"},
        {VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT, "COSITED_CHROMA_SAMPLES"}
    };
    
    return FlagsToString(features, featureNames);
}

template<typename T, typename F>
std::string VulkanUtils::FlagsToString(T flags, const std::vector<std::pair<F, const char*>>& flagNames) {
    if (flags == 0) {
        return "NONE";
    }
    
    std::vector<std::string> activeFlags;
    
    for (const auto& flagPair : flagNames) {
        if (flags & flagPair.first) {
            activeFlags.push_back(flagPair.second);
        }
    }
    
    if (activeFlags.empty()) {
        std::ostringstream oss;
        oss << "UNKNOWN_" << std::hex << static_cast<uint32_t>(flags);
        return oss.str();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < activeFlags.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << activeFlags[i];
    }
    
    return oss.str();
}

} // namespace AstralEngine
