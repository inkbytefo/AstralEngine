#include "VulkanShader.h"
#include "IShader.h"
#include "../../../Core/Logger.h"
#include <stdexcept>
#include <vector>
#include <string>

namespace AstralEngine {

VulkanShader::VulkanShader() 
    : m_device(nullptr)
    , m_shaderModule(VK_NULL_HANDLE)
    , m_stage(VK_SHADER_STAGE_VERTEX_BIT)  // Default değer
    , m_isInitialized(false) {
    
    Logger::Debug("VulkanShader", "VulkanShader created");
}

VulkanShader::~VulkanShader() {
    if (m_isInitialized) {
        Shutdown();
    }
    Logger::Debug("VulkanShader", "VulkanShader destroyed");
}

bool VulkanShader::Initialize(VulkanDevice* device, const std::vector<uint32_t>& spirvCode, VkShaderStageFlagBits stage) {
    if (m_isInitialized) {
        Logger::Warning("VulkanShader", "VulkanShader already initialized");
        return true;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    if (spirvCode.empty()) {
        SetError("Empty SPIR-V code");
        return false;
    }
    
    m_device = device;
    m_stage = stage;
    m_shaderCode = spirvCode;

    // Calculate shader hash
    m_shaderHash = 0;
    if (!spirvCode.empty()) {
        // Simple hash calculation using first and last 64 bits of shader code
        size_t codeSize = spirvCode.size();
        m_shaderHash = (static_cast<uint64_t>(spirvCode[0]) << 32) | (codeSize > 1 ? spirvCode[codeSize - 1] : 0);
    }

    Logger::Info("VulkanShader", "Initializing shader from SPIR-V code (stage: {}, hash: {})",
                 static_cast<uint32_t>(m_stage), m_shaderHash);
    
    try {
        Logger::Debug("VulkanShader", "SPIR-V code size: {} words",
                     spirvCode.size());
        
        // Shader module oluştur
        if (!CreateShaderModule(spirvCode)) {
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanShader", "Shader initialized successfully from SPIR-V code");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during shader initialization: ") + e.what());
        Logger::Error("VulkanShader", "Shader initialization failed: {}", e.what());
        return false;
    }
}

void VulkanShader::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    Logger::Info("VulkanShader", "Shutting down shader");
    
    if (m_shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device->GetDevice(), m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
        Logger::Debug("VulkanShader", "Shader module destroyed");
    }
    
    m_device = nullptr;
    m_lastError.clear();
    m_isInitialized = false;
    m_shaderCode.clear();
    m_shaderHash = 0;
    
    Logger::Info("VulkanShader", "Shader shutdown completed");
}

bool VulkanShader::CreateShaderModule(const std::vector<uint32_t>& spirvCode) {
    Logger::Debug("VulkanShader", "Creating shader module");
    
    // Shader module create info yapısı
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();
    
    // Shader module oluştur
    VkResult result = vkCreateShaderModule(m_device->GetDevice(), &createInfo, nullptr, &m_shaderModule);
    
    if (result != VK_SUCCESS) {
        std::string error = "Failed to create shader module, VkResult: " + std::to_string(result);
        SetError(error);
        Logger::Error("VulkanShader", "Failed to create shader module: {}", error);
        return false;
    }
    
    Logger::Debug("VulkanShader", "Shader module created successfully");
    return true;
}

void VulkanShader::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanShader", "Error: {}", error);
}

// IShader interface implementations
bool VulkanShader::Initialize(ShaderStage stage, const std::vector<uint32_t>& shaderCode) {
    // DISABLED: This method is fully disabled to prevent unsafe usage
    // The deprecated Initialize(ShaderStage, ...) method remains callable via IShader
    // but is now fully disabled and will not use m_device which may be nullptr
    m_lastError = "Disabled overload: use Initialize(VulkanDevice*, const std::vector<uint32_t>&, VkShaderStageFlagBits) instead";
    Logger::Error("VulkanShader", "DISABLED: Attempted to use deprecated Initialize(ShaderStage, ...) method. "
                  "This method is fully disabled. Use Initialize(VulkanDevice*, ...) instead.");
    return false;
}

ShaderStage VulkanShader::GetShaderStage() const {
    switch (m_stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return ShaderStage::Vertex;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return ShaderStage::Fragment;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return ShaderStage::Compute;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return ShaderStage::Geometry;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return ShaderStage::TessControl;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return ShaderStage::TessEvaluation;
        case VK_SHADER_STAGE_TASK_BIT_EXT:
            return ShaderStage::Task;
        case VK_SHADER_STAGE_MESH_BIT_EXT:
            return ShaderStage::Mesh;
        default:
            return ShaderStage::Vertex; // Default fallback
    }
}

VkShaderStageFlagBits VulkanShader::GetVulkanShaderStage() const {
    return m_stage;
}

const std::vector<uint32_t>& VulkanShader::GetShaderCode() const {
    return m_shaderCode;
}

uint64_t VulkanShader::GetShaderHash() const {
    return m_shaderHash;
}

bool VulkanShader::IsCompatibleWithAPI(RendererAPI api) const {
    return api == RendererAPI::Vulkan;
}

size_t VulkanShader::GetShaderCodeSize() const {
    return m_shaderCode.size() * sizeof(uint32_t);
}

bool VulkanShader::Validate() const {
    if (!m_isInitialized) {
        return false;
    }

    if (m_shaderModule == VK_NULL_HANDLE) {
        return false;
    }

    if (m_device == nullptr) {
        return false;
    }

    return true;
}

bool VulkanShader::IsInitialized() const {
    return m_isInitialized;
}

VkShaderModule VulkanShader::GetShaderModule() const {
    return m_shaderModule;
}

const std::string& VulkanShader::GetLastError() const {
    return m_lastError;
}

} // namespace AstralEngine
