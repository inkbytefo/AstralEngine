#include "ShaderCompiler.h"
#include "../../Core/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace AstralEngine {

ShaderCompiler::ShaderCompiler() {
    Logger::Debug("ShaderCompiler", "ShaderCompiler created");
}

ShaderCompiler::~ShaderCompiler() {
    Logger::Debug("ShaderCompiler", "ShaderCompiler destroyed");
}

bool ShaderCompiler::Initialize(VulkanDevice* device) {
    if (!device) {
        Logger::Error("ShaderCompiler", "Invalid Vulkan device provided");
        return false;
    }
    
    m_device = device;
    Logger::Info("ShaderCompiler", "Shader compiler initialized successfully");
    return true;
}

void ShaderCompiler::Shutdown() {
    m_device = nullptr;
    Logger::Info("ShaderCompiler", "Shader compiler shutdown complete");
}

std::vector<uint32_t> ShaderCompiler::CompileShader(const std::string& shaderPath, ShaderType type) {
    if (!m_device) {
        Logger::Error("ShaderCompiler", "Shader compiler not initialized");
        return {};
    }
    
    Logger::Info("ShaderCompiler", "Compiling shader: {}", shaderPath);
    
    // Shader source kodunu oku
    std::string shaderSource = ReadShaderSource(shaderPath);
    if (shaderSource.empty()) {
        Logger::Error("ShaderCompiler", "Failed to read shader source: {}", shaderPath);
        return {};
    }
    
    // GLSL kaynak kodunu SPIR-V'e derle
    std::vector<uint32_t> spirvCode = CompileGLSLToSPIRV(shaderSource, shaderPath, type);
    if (spirvCode.empty()) {
        Logger::Error("ShaderCompiler", "Failed to compile shader to SPIR-V: {}", shaderPath);
        return {};
    }
    
    Logger::Info("ShaderCompiler", "Shader compiled successfully: {}", shaderPath);
    return spirvCode;
}

std::string ShaderCompiler::ReadShaderSource(const std::string& shaderPath) {
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        Logger::Error("ShaderCompiler", "Failed to open shader file: {}", shaderPath);
        return "";
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0);
    
    std::string shaderSource;
    shaderSource.resize(fileSize);
    
    if (!file.read(shaderSource.data(), fileSize)) {
        Logger::Error("ShaderCompiler", "Failed to read shader file: {}", shaderPath);
        return "";
    }
    
    return shaderSource;
}

std::vector<uint32_t> ShaderCompiler::CompileGLSLToSPIRV(const std::string& glslSource, 
                                                         const std::string& shaderPath, 
                                                         ShaderType type) {
    // Bu implementasyon glslc kullanarak GLSL'yi SPIR-V'e derler
    // Gerçek bir uygulamada, glslc veya shaderc kullanılabilir
    
    std::string shaderTypeStr = GetShaderTypeString(type);
    std::string tempOutputPath = shaderPath + ".spv";
    
    // glslc komutunu oluştur
    std::string command = "glslc " + shaderPath + " -o " + tempOutputPath;
    
    // Komutu çalıştır
    int result = std::system(command.c_str());
    if (result != 0) {
        Logger::Error("ShaderCompiler", "glslc compilation failed with code: {}", result);
        return {};
    }
    
    // Derlenmiş SPIR-V kodunu oku
    std::vector<uint32_t> spirvCode = ReadSPIRVBinary(tempOutputPath);
    
    // Geçici dosyayı temizle
    std::filesystem::remove(tempOutputPath);
    
    return spirvCode;
}

std::vector<uint32_t> ShaderCompiler::ReadSPIRVBinary(const std::string& spirvPath) {
    std::ifstream file(spirvPath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        Logger::Error("ShaderCompiler", "Failed to open SPIR-V file: {}", spirvPath);
        return {};
    }
    
    size_t fileSize = file.tellg();
    if (fileSize % sizeof(uint32_t) != 0) {
        Logger::Error("ShaderCompiler", "Invalid SPIR-V file size: {}", fileSize);
        return {};
    }
    
    file.seekg(0);
    
    std::vector<uint32_t> spirvCode(fileSize / sizeof(uint32_t));
    
    if (!file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize)) {
        Logger::Error("ShaderCompiler", "Failed to read SPIR-V file: {}", spirvPath);
        return {};
    }
    
    return spirvCode;
}

std::string ShaderCompiler::GetShaderTypeString(ShaderType type) {
    switch (type) {
        case ShaderType::Vertex:
            return "vertex";
        case ShaderType::Fragment:
            return "fragment";
        case ShaderType::Compute:
            return "compute";
        case ShaderType::Geometry:
            return "geometry";
        case ShaderType::TessellationControl:
            return "tessellation_control";
        case ShaderType::TessellationEvaluation:
            return "tessellation_evaluation";
        default:
            return "unknown";
    }
}

bool ShaderCompiler::ValidateShader(const std::vector<uint32_t>& spirvCode) {
    if (spirvCode.empty()) {
        Logger::Error("ShaderCompiler", "Empty SPIR-V code provided for validation");
        return false;
    }
    
    // SPIR-V magic number kontrolü
    if (spirvCode[0] != 0x07230203) {
        Logger::Error("ShaderCompiler", "Invalid SPIR-V magic number");
        return false;
    }
    
    // Burada daha detaylı SPIR-V validasyonu yapılabilir
    // Örneğin, shader versiyonu, capability'ler vb.
    
    return true;
}

std::string ShaderCompiler::GetShaderCompilationError(const std::string& shaderPath) {
    // Bu fonksiyon shader derleme hatalarını döndürür
    // Gerçek implementasyonda, derleyici çıktısını parse etmek gerekir
    
    std::string errorLog;
    std::string logPath = shaderPath + ".log";
    
    std::ifstream logFile(logPath);
    if (logFile.is_open()) {
        std::string line;
        while (std::getline(logFile, line)) {
            errorLog += line + "\n";
        }
        logFile.close();
        
        // Log dosyasını temizle
        std::filesystem::remove(logPath);
    }
    
    return errorLog;
}

} // namespace AstralEngine
