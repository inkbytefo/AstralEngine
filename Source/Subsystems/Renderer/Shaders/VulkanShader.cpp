#include "VulkanShader.h"
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
    
    Logger::Info("VulkanShader", "Initializing shader from SPIR-V code (stage: {})",
                static_cast<uint32_t>(m_stage));
    
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

} // namespace AstralEngine
