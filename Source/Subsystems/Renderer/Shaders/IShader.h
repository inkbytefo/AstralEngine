#pragma once

#include "../RendererTypes.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

namespace AstralEngine {

/**
 * @enum ShaderStage
 * @brief Shader pipeline stages supported by the engine
 */
enum class ShaderStage {
    Vertex = 0,     ///< Vertex shader stage
    Fragment,       ///< Fragment/Pixel shader stage
    Compute,        ///< Compute shader stage
    Geometry,       ///< Geometry shader stage
    TessControl,    ///< Tessellation control shader stage
    TessEvaluation, ///< Tessellation evaluation shader stage
    Task,           ///< Task shader stage (mesh shaders)
    Mesh            ///< Mesh shader stage
};

/**
 * @class IShader
 * @brief Abstract interface for shader objects used by the Material system
 *
 * This interface defines the contract for shader objects that can be used
 * polymorphically by the Material system. It provides a common interface
 * for different shader implementations (Vulkan, OpenGL, etc.) while
 * maintaining proper abstraction.
 *
 * The interface includes methods for:
 * - Shader stage identification
 * - Initialization status checking
 * - Error information retrieval
 * - Resource management
 *
 * @note This interface is designed to be implemented by concrete shader
 * classes like VulkanShader, OpenGLShader, etc.
 */
class IShader {
public:
    /**
     * @brief Virtual destructor for proper cleanup in derived classes
     */
    virtual ~IShader() = default;

    /**
     * @brief DEPRECATED: Initialize the shader with compiled shader code
     * @deprecated This method is deprecated and disabled. Use Initialize(VulkanDevice*, const std::vector<uint32_t>&, VkShaderStageFlagBits) instead.
     * @param stage The shader stage this shader represents
     * @param shaderCode The compiled shader bytecode/SPIR-V
     * @return Always returns false - this method is disabled
     * @note This method is kept for ABI compatibility but implementations should return false without side effects
     */
    virtual bool Initialize(ShaderStage stage, const std::vector<uint32_t>& shaderCode) = 0;

    /**
     * @brief Initialize the shader with Vulkan-specific parameters
     * @param device Pointer to Vulkan device
     * @param shaderCode The SPIR-V shader code
     * @param stage Vulkan shader stage flag bits
     * @return true if initialization successful, false otherwise
     * @note This method is specific to Vulkan implementation
     */
    virtual bool Initialize(VulkanDevice* device, const std::vector<uint32_t>& shaderCode,
                          VkShaderStageFlagBits stage) = 0;

    /**
     * @brief Clean up shader resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Get the shader stage this shader represents
     * @return The shader stage enum value
     */
    virtual ShaderStage GetShaderStage() const = 0;

    /**
     * @brief Get the Vulkan shader stage flag bits (if applicable)
     * @return Vulkan shader stage flags, or 0 if not Vulkan
     * @note This method may return 0 for non-Vulkan implementations
     */
    virtual VkShaderStageFlagBits GetVulkanShaderStage() const = 0;

    /**
     * @brief Check if the shader is properly initialized
     * @return true if shader is ready for use, false otherwise
     */
    virtual bool IsInitialized() const = 0;

    /**
     * @brief Get the last error message
     * @return String containing the last error, empty if no error
     */
    virtual const std::string& GetLastError() const = 0;

    /**
     * @brief Get the shader bytecode/SPIR-V code
     * @return Vector containing the compiled shader code
     */
    virtual const std::vector<uint32_t>& GetShaderCode() const = 0;

    /**
     * @brief Get the underlying shader module handle (Vulkan-specific)
     * @return Vulkan shader module handle, or VK_NULL_HANDLE if not Vulkan
     * @note This method may return VK_NULL_HANDLE for non-Vulkan implementations
     */
    virtual VkShaderModule GetShaderModule() const = 0;

    /**
     * @brief Get the hash of the shader code for comparison/caching
     * @return 64-bit hash of the shader bytecode
     */
    virtual uint64_t GetShaderHash() const = 0;

    /**
     * @brief Check if shader is compatible with a specific renderer API
     * @param api The renderer API to check compatibility with
     * @return true if compatible, false otherwise
     */
    virtual bool IsCompatibleWithAPI(RendererAPI api) const = 0;

    /**
     * @brief Get the size of the shader code in bytes
     * @return Size of shader code in bytes
     */
    virtual size_t GetShaderCodeSize() const = 0;

    /**
     * @brief Validate shader compatibility with current system
     * @return true if shader is valid and compatible, false otherwise
     */
    virtual bool Validate() const = 0;

protected:
    /**
     * @brief Set error message for derived classes
     * @param error The error message to set
     */
    virtual void SetError(const std::string& error) = 0;
};

} // namespace AstralEngine