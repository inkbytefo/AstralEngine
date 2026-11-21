#pragma once

// Internal Vulkan include. Only files under modules/Render/src include this.
#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include "Astral/Core/Log.hpp"

#include <stdexcept>
#include <string>

namespace Astral::Render::Vk {

inline void Check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(what) + " failed: " + vk::to_string(vk::Result(result)));
}

inline uint32_t MipCount(uint32_t width, uint32_t height) {
    uint32_t levels = 1;
    while ((width | height) >> levels) ++levels;
    return levels;
}

inline uint32_t DivUp(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

} // namespace Astral::Render::Vk
