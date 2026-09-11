#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <unordered_map>

namespace Astral {

inline vk::UniqueDescriptorPool CreateDescriptorPoolFromBindings(
    vk::Device device,
    const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
    uint32_t maxSets)
{
    std::unordered_map<vk::DescriptorType, uint32_t> counts;
    for (const auto& b : bindings) {
        counts[b.descriptorType] += b.descriptorCount * maxSets;
    }
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.reserve(counts.size());
    for (const auto& [type, count] : counts) {
        poolSizes.push_back({type, count});
    }
    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        maxSets,
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    );
    return device.createDescriptorPoolUnique(poolInfo);
}

} // namespace Astral
