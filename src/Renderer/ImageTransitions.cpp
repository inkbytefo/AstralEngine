#include "Astral/Renderer/ImageTransitions.hpp"
#include <vector>

namespace Astral {

ImageAccessInfo GetImageAccessInfo(ImageUse use) noexcept {
    switch (use) {
        case ImageUse::ComputeRead:
            return {
                vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eGeneral
            };
        case ImageUse::ComputeWrite:
            return {
                vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eShaderWrite,
                vk::ImageLayout::eGeneral
            };
        case ImageUse::FragmentRead:
            return {
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eGeneral
            };
        case ImageUse::TransferRead:
            return {
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal
            };
        case ImageUse::TransferWrite:
            return {
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eTransferDstOptimal
            };
    }
    return {
        vk::PipelineStageFlagBits::eTopOfPipe,
        {},
        vk::ImageLayout::eUndefined
    };
}

void TransitionImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse before,
    ImageUse after,
    vk::ImageSubresourceRange range
) {
    auto srcInfo = GetImageAccessInfo(before);
    auto dstInfo = GetImageAccessInfo(after);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = srcInfo.layout;
    barrier.newLayout = dstInfo.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = range;
    barrier.srcAccessMask = srcInfo.accessMask;
    barrier.dstAccessMask = dstInfo.accessMask;

    cmd.pipelineBarrier(
        srcInfo.stageMask,
        dstInfo.stageMask,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void InitializeImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse targetUse,
    vk::ImageSubresourceRange range
) {
    auto dstInfo = GetImageAccessInfo(targetUse);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = dstInfo.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = range;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = dstInfo.accessMask;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        dstInfo.stageMask,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void DiscardAndTransitionImage(
    vk::CommandBuffer cmd,
    vk::Image image,
    ImageUse before,
    ImageUse after,
    vk::ImageSubresourceRange range
) {
    auto srcInfo = GetImageAccessInfo(before);
    auto dstInfo = GetImageAccessInfo(after);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = dstInfo.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = range;
    barrier.srcAccessMask = srcInfo.accessMask;
    barrier.dstAccessMask = dstInfo.accessMask;

    cmd.pipelineBarrier(
        srcInfo.stageMask,
        dstInfo.stageMask,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void TransitionImages(
    vk::CommandBuffer cmd,
    std::span<const ImageTransitionItem> transitions
) {
    if (transitions.empty()) return;

    vk::PipelineStageFlags combinedSrcStage{};
    vk::PipelineStageFlags combinedDstStage{};
    std::vector<vk::ImageMemoryBarrier> barriers;
    barriers.reserve(transitions.size());

    for (const auto& t : transitions) {
        auto srcInfo = GetImageAccessInfo(t.before);
        auto dstInfo = GetImageAccessInfo(t.after);

        combinedSrcStage |= srcInfo.stageMask;
        combinedDstStage |= dstInfo.stageMask;

        vk::ImageMemoryBarrier b{};
        b.oldLayout = t.discardContent ? vk::ImageLayout::eUndefined : srcInfo.layout;
        b.newLayout = dstInfo.layout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = t.image;
        b.subresourceRange = t.range;
        b.srcAccessMask = srcInfo.accessMask;
        b.dstAccessMask = dstInfo.accessMask;
        barriers.push_back(b);
    }

    cmd.pipelineBarrier(
        combinedSrcStage,
        combinedDstStage,
        {},
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()),
        barriers.data()
    );
}

} // namespace Astral
