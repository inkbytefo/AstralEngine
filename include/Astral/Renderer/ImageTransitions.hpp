#pragma once

#include <vulkan/vulkan.hpp>
#include <span>

namespace Astral {

/**
 * @brief Represents high-level GPU usage intents for an image resource.
 */
enum class ImageUse {
    ComputeRead,    ///< Storage image or sampled image read in compute shader
    ComputeWrite,   ///< Storage image write in compute shader
    FragmentRead,   ///< Sampled image read in fragment shader (e.g. ImGui viewport)
    TransferRead,   ///< Read in transfer queue/stage (copyImageToBuffer, blit source)
    TransferWrite   ///< Write in transfer queue/stage (clearColorImage, blit dest, copyBufferToImage)
};

/**
 * @brief Low-level pipeline stage, access mask, and image layout mapping for an ImageUse.
 */
struct ImageAccessInfo {
    vk::PipelineStageFlags stageMask;
    vk::AccessFlags accessMask;
    vk::ImageLayout layout;
};

/**
 * @brief Returns the Vulkan pipeline stage, access mask, and image layout for an ImageUse.
 */
ImageAccessInfo GetImageAccessInfo(ImageUse use) noexcept;

/**
 * @brief Transitions a single image from 'before' use to 'after' use.
 * Preserves contents (oldLayout is derived from 'before').
 */
void TransitionImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse before,
    ImageUse after,
    vk::ImageSubresourceRange range = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
);

/**
 * @brief Explicit initialization for freshly created images or initial frame state.
 * Uses oldLayout = eUndefined and srcStage = eTopOfPipe, srcAccess = {}.
 */
void InitializeImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse targetUse,
    vk::ImageSubresourceRange range = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
);

/**
 * @brief Discards existing contents (oldLayout = eUndefined) but preserves pipeline synchronization
 * from the previous GPU work ('before' stage and access) that accessed the image.
 */
void DiscardAndTransitionImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse before,
    ImageUse after,
    vk::ImageSubresourceRange range = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
);

/**
 * @brief Item descriptor for batch image transitions.
 */
struct ImageTransitionItem {
    vk::Image image;
    ImageUse before;
    ImageUse after;
    vk::ImageSubresourceRange range = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    bool discardContent = false;
};

/**
 * @brief Batches multiple image transitions into a single vkCmdPipelineBarrier call.
 */
void TransitionImages(
    vk::CommandBuffer cmd,
    std::span<const ImageTransitionItem> transitions
);

} // namespace Astral
