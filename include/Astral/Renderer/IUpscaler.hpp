#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <string>

namespace Astral {

enum class UpscalerType {
    None = 0,
    NativeTAA = 1,
    FSR2 = 2,
    DLSS = 3
};

struct UpscalerParameters {
    uint32_t renderWidth = 1280;
    uint32_t renderHeight = 720;
    uint32_t displayWidth = 1280;
    uint32_t displayHeight = 720;
    float jitterX = 0.0f;
    float jitterY = 0.0f;
    float sharpness = 0.0f;
    float deltaTime = 0.0166f;
    bool resetAccumulation = false;
};

/// NVIDIA Streamline / DLSS ve AMD FSR 2.x / 3.x upscaling sistemleri icin
/// ortak yuksek basarimli arayuz ve pass sozlesmesi (Faz 3).
class IUpscaler {
public:
    virtual ~IUpscaler() = default;

    virtual UpscalerType GetType() const = 0;
    virtual std::string GetName() const = 0;

    virtual void Init(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight) = 0;
    virtual void Resize(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight) = 0;

    virtual void Evaluate(
        vk::CommandBuffer cmd,
        vk::ImageView colorInput,
        vk::ImageView depthInput,
        vk::ImageView motionVectorsInput,
        vk::ImageView outputImage,
        const UpscalerParameters& params
    ) = 0;
};

} // namespace Astral
