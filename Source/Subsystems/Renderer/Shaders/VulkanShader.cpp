#include "VulkanShader.h"
#include "../../../Core/Logger.h"
#include <fstream>
#include <stdexcept>

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

bool VulkanShader::Initialize(VulkanDevice* device, const Config& config) {
    if (m_isInitialized) {
        Logger::Warning("VulkanShader", "VulkanShader already initialized");
        return true;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    if (config.filePath.empty()) {
        SetError("Empty shader file path");
        return false;
    }
    
    m_device = device;
    m_filePath = config.filePath;
    m_stage = config.stage;
    
    Logger::Info("VulkanShader", "Initializing shader: {} (stage: {})", 
                m_filePath, static_cast<uint32_t>(m_stage));
    
    try {
        // Shader dosyasını oku
        auto shaderCode = ReadShaderFile(m_filePath);
        if (shaderCode.empty()) {
            SetError("Failed to read shader file: " + m_filePath);
            return false;
        }
        
        Logger::Debug("VulkanShader", "Shader file read successfully, size: {} bytes", 
                     shaderCode.size());
        
        // Shader module oluştur
        if (!CreateShaderModule(shaderCode)) {
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanShader", "Shader initialized successfully: {}", m_filePath);
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
    
    Logger::Info("VulkanShader", "Shutting down shader: {}", m_filePath);
    
    if (m_shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device->GetDevice(), m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
        Logger::Debug("VulkanShader", "Shader module destroyed");
    }
    
    m_device = nullptr;
    m_filePath.clear();
    m_lastError.clear();
    m_isInitialized = false;
    
    Logger::Info("VulkanShader", "Shader shutdown completed");
}

std::vector<char> VulkanShader::ReadShaderFile(const std::string& filePath) {
    Logger::Debug("VulkanShader", "Reading shader file: {}", filePath);
    
    // Binary modda dosyayı aç
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        SetError("Failed to open shader file: " + filePath);
        Logger::Error("VulkanShader", "Failed to open shader file: {}", filePath);
        return {};
    }
    
    // Dosya boyutunu al
    size_t fileSize = static_cast<size_t>(file.tellg());
    Logger::Debug("VulkanShader", "Shader file size: {} bytes", fileSize);
    
    // Dosya başına geri dön
    file.seekg(0);
    
    // Buffer oluştur
    std::vector<char> buffer(fileSize);
    
    // Dosyayı buffer'a oku
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    
    // Okuma başarılı mı kontrol et
    if (!file) {
        SetError("Failed to read entire shader file: " + filePath);
        Logger::Error("VulkanShader", "Failed to read entire shader file: {}", filePath);
        return {};
    }
    
    file.close();
    
    Logger::Debug("VulkanShader", "Shader file read successfully");
    return buffer;
}

bool VulkanShader::CreateShaderModule(const std::vector<char>& shaderCode) {
    Logger::Debug("VulkanShader", "Creating shader module");
    
    // Shader module create info yapısı
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    
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
