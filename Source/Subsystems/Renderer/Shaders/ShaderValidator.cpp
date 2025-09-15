#include "ShaderValidator.h"
#include "Core/Logger.h"
#include <filesystem>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace AstralEngine {

bool ShaderValidator::ValidateGLSL(const std::string& glslFilePath, 
                                 const std::string& shaderType,
                                 const std::string& outputPath) {
    
    if (!std::filesystem::exists(glslFilePath)) {
        AstralEngine::Logger::Error("ShaderValidator", "GLSL file not found: {}", glslFilePath);
        return false;
    }

    std::string stageFlag = GetShaderStageFlag(shaderType);
    if (stageFlag.empty()) {
        AstralEngine::Logger::Error("ShaderValidator", "Unknown shader type: {}", shaderType);
        return false;
    }

    // Build glslangValidator command
    std::string command = "glslangValidator " + stageFlag + " -V \"" + glslFilePath + "\"";
    
    if (!outputPath.empty()) {
        command += " -o \"" + outputPath + "\"";
    }

    AstralEngine::Logger::Info("ShaderValidator", "Validating GLSL: {}", command);
    
    std::string output;
    bool success = ExecuteCommand(command, output);
    
    if (success) {
        AstralEngine::Logger::Info("ShaderValidator", "GLSL validation successful: {}", glslFilePath);
        if (!output.empty()) {
            AstralEngine::Logger::Debug("ShaderValidator", "Validation output: {}", output);
        }
        return true;
    } else {
        AstralEngine::Logger::Error("ShaderValidator", "GLSL validation failed: {}", glslFilePath);
        if (!output.empty()) {
            AstralEngine::Logger::Error("ShaderValidator", "Validation errors: {}", output);
        }
        return false;
    }
}

bool ShaderValidator::ValidateSPIRV(const std::string& spirvFilePath) {
    
    if (!std::filesystem::exists(spirvFilePath)) {
        AstralEngine::Logger::Error("ShaderValidator", "SPIR-V file not found: {}", spirvFilePath);
        return false;
    }

    // Build spirv-val command
    std::string command = "spirv-val \"" + spirvFilePath + "\"";
    
    AstralEngine::Logger::Info("ShaderValidator", "Validating SPIR-V: {}", command);
    
    std::string output;
    bool success = ExecuteCommand(command, output);
    
    if (success) {
        AstralEngine::Logger::Info("ShaderValidator", "SPIR-V validation successful: {}", spirvFilePath);
        if (!output.empty()) {
            AstralEngine::Logger::Debug("ShaderValidator", "Validation output: {}", output);
        }
        return true;
    } else {
        AstralEngine::Logger::Error("ShaderValidator", "SPIR-V validation failed: {}", spirvFilePath);
        if (!output.empty()) {
            AstralEngine::Logger::Error("ShaderValidator", "Validation errors: {}", output);
        }
        return false;
    }
}

bool ShaderValidator::CompileAndValidateGLSL(const std::string& glslFilePath,
                                           const std::string& shaderType,
                                           const std::string& outputPath) {
    
    // First validate the GLSL source
    if (!ValidateGLSL(glslFilePath, shaderType, outputPath)) {
        return false;
    }

    // If output path is provided, validate the compiled SPIR-V
    if (!outputPath.empty() && std::filesystem::exists(outputPath)) {
        return ValidateSPIRV(outputPath);
    }

    return true;
}

bool ShaderValidator::AreValidationToolsAvailable() {
    std::string output;
    
    // Check glslangValidator
    bool glslangAvailable = ExecuteCommand("glslangValidator --version", output);
    if (!glslangAvailable) {
        AstralEngine::Logger::Warning("ShaderValidator", "glslangValidator not found in PATH");
    } else {
        AstralEngine::Logger::Info("ShaderValidator", "glslangValidator found: {}", output);
    }
    
    // Check spirv-val
    bool spirvValAvailable = ExecuteCommand("spirv-val --version", output);
    if (!spirvValAvailable) {
        AstralEngine::Logger::Warning("ShaderValidator", "spirv-val not found in PATH");
    } else {
        AstralEngine::Logger::Info("ShaderValidator", "spirv-val found: {}", output);
    }
    
    return glslangAvailable && spirvValAvailable;
}

bool ShaderValidator::ExecuteCommand(const std::string& command, std::string& output) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    
    if (!pipe) {
        AstralEngine::Logger::Error("ShaderValidator", "Failed to execute command: {}", command);
        return false;
    }

    // Read command output
    char buffer[128];
    output.clear();
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#ifdef _WIN32
    int result = _pclose(pipe);
#else
    int result = pclose(pipe);
#endif

    return result == 0;
}

std::string ShaderValidator::GetShaderStageFlag(const std::string& shaderType) {
    if (shaderType == "vertex" || shaderType == "vert") {
        return "-S vert";
    } else if (shaderType == "fragment" || shaderType == "frag") {
        return "-S frag";
    } else if (shaderType == "geometry" || shaderType == "geom") {
        return "-S geom";
    } else if (shaderType == "tessellation" || shaderType == "tess") {
        return "-S tess";
    } else if (shaderType == "compute" || shaderType == "comp") {
        return "-S comp";
    }
    
    return "";
}

} // namespace AstralEngine