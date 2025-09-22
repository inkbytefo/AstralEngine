
#pragma once

#include <cstdint>

// Forward declarations to avoid including heavy headers
struct VkCommandBuffer_T;
typedef VkCommandBuffer_T* VkCommandBuffer;

namespace AstralEngine {

class RenderSubsystem;
class VulkanRenderer;
class GraphicsDevice;

/**
 * @class IRenderPass
 * @brief Interface for a single pass in the rendering pipeline.
 *
 * This class defines the contract for a self-contained rendering pass,
 * such as G-Buffer, Lighting, Shadows, or Post-Processing. Each pass is
 * responsible for its own resources (pipelines, render targets if not shared)
 * and command recording.
 */
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    /**
     * @brief Initializes the render pass and its resources.
     * @param owner A pointer to the parent RenderSubsystem.
     * @return true if initialization was successful, false otherwise.
     */
    virtual bool Initialize(RenderSubsystem* owner) = 0;

    /**
     * @brief Shuts down the render pass and releases its resources.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Records the Vulkan commands for this render pass.
     * @param commandBuffer The command buffer to record into.
     * @param frameIndex The current frame index in flight.
     */
    virtual void Record(VkCommandBuffer commandBuffer, uint32_t frameIndex) = 0;

    /**
     * @brief Handles window resize events to recreate or resize resources.
     * @param width The new width.
     * @param height The new height.
     */
    virtual void OnResize(uint32_t width, uint32_t height) {};

    /**
     * @brief Returns the name of the render pass for debugging.
     * @return A const char* to the pass's name.
     */
    virtual const char* GetName() const = 0;
};

} // namespace AstralEngine
