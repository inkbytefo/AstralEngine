#pragma once

#include <memory>
#include "Subsystems/Renderer/Shaders/VulkanShader.h"

namespace AstralEngine {

/**
 * @brief Shader programını temsil eder - vertex ve fragment shader'ları bir arada tutar
 */
struct ShaderProgram {
    std::shared_ptr<VulkanShader> vertexShader;
    std::shared_ptr<VulkanShader> fragmentShader;
    
    ShaderProgram() = default;
    ShaderProgram(std::shared_ptr<VulkanShader> vert, std::shared_ptr<VulkanShader> frag)
        : vertexShader(vert), fragmentShader(frag) {}
    
    bool IsValid() const {
        return vertexShader != nullptr && fragmentShader != nullptr;
    }
    
    void Invalidate() {
        vertexShader.reset();
        fragmentShader.reset();
    }
};

} // namespace AstralEngine
