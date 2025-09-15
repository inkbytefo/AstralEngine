#pragma once

#include <string>
#include <vector>

namespace AstralEngine {

/**
 * @class ShaderValidator
 * @brief Shader validation utilities for Vulkan shaders
 * 
 * This class provides methods to validate GLSL and SPIR-V shaders
 * using external tools like glslangValidator and spirv-val.
 */
class ShaderValidator {
public:
    /**
     * @brief Validate a GLSL shader file using glslangValidator
     * 
     * @param glslFilePath Path to the GLSL shader file
     * @param shaderType Shader type (vertex, fragment, etc.)
     * @param outputPath Optional path for compiled SPIR-V output
     * @return true if validation succeeded, false otherwise
     */
    static bool ValidateGLSL(const std::string& glslFilePath, 
                           const std::string& shaderType,
                           const std::string& outputPath = "");

    /**
     * @brief Validate a SPIR-V shader file using spirv-val
     * 
     * @param spirvFilePath Path to the SPIR-V shader file
     * @return true if validation succeeded, false otherwise
     */
    static bool ValidateSPIRV(const std::string& spirvFilePath);

    /**
     * @brief Compile GLSL to SPIR-V and validate the result
     * 
     * @param glslFilePath Path to the GLSL shader file
     * @param shaderType Shader type (vertex, fragment, etc.)
     * @param outputPath Path for compiled SPIR-V output
     * @return true if compilation and validation succeeded, false otherwise
     */
    static bool CompileAndValidateGLSL(const std::string& glslFilePath,
                                     const std::string& shaderType,
                                     const std::string& outputPath);

    /**
     * @brief Check if required validation tools are available
     * 
     * @return true if both glslangValidator and spirv-val are available
     */
    static bool AreValidationToolsAvailable();

private:
    /**
     * @brief Execute a command and capture its output
     * 
     * @param command Command to execute
     * @param output Reference to store command output
     * @return true if command succeeded, false otherwise
     */
    static bool ExecuteCommand(const std::string& command, std::string& output);

    /**
     * @brief Get the appropriate shader stage flag for glslangValidator
     * 
     * @param shaderType String representation of shader type
     * @return Corresponding shader stage flag for glslangValidator
     */
    static std::string GetShaderStageFlag(const std::string& shaderType);
};

} // namespace AstralEngine