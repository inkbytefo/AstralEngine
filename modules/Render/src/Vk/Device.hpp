#pragma once

#include "Vk/VkCommon.hpp"

#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace Astral::Render::Vk {

struct DeviceDesc {
    std::string applicationName;
    bool validation = false;
    std::vector<std::string> instanceExtensions;
};

// Instance, physical/logical device, the single graphics+compute queue and VMA.
// Also owns the deferred-destruction queue: GPU objects released while frames are
// still in flight are destroyed once those frames have completed.
class Device {
public:
    explicit Device(const DeviceDesc& desc);
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    vk::Instance Instance() const { return m_Instance; }
    vk::PhysicalDevice Physical() const { return m_Physical; }
    vk::Device Get() const { return m_Device; }
    vk::Queue Queue() const { return m_Queue; }
    uint32_t QueueFamily() const { return m_QueueFamily; }
    VmaAllocator Allocator() const { return m_Allocator; }
    const vk::PhysicalDeviceProperties& Properties() const { return m_Properties; }
    bool ValidationEnabled() const { return m_Validation; }
    bool DepthClampSupported() const { return m_DepthClamp; }
    float MaxAnisotropy() const { return m_MaxAnisotropy; }

    // Records commands into a one-off command buffer and waits for completion.
    void ImmediateSubmit(const std::function<void(vk::CommandBuffer)>& record);

    // Deferred destruction. `frameNumber` advances once per renderer frame;
    // objects retired in frame N are destroyed when frame N is known complete.
    void Retire(std::function<void()> destroy);
    void SetFrameNumber(uint64_t frame) { m_FrameNumber = frame; }
    void CollectGarbage(uint64_t completedFrame);
    void FlushGarbage();

    void SetDebugName(vk::ObjectType type, uint64_t handle, const std::string& name);

private:
    void CreateInstance(const DeviceDesc& desc);
    void PickPhysicalDevice();
    void CreateDevice();

    vk::Instance m_Instance;
    VkDebugUtilsMessengerEXT m_Messenger = VK_NULL_HANDLE;
    vk::PhysicalDevice m_Physical;
    vk::PhysicalDeviceProperties m_Properties;
    vk::Device m_Device;
    vk::Queue m_Queue;
    uint32_t m_QueueFamily = 0;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    vk::CommandPool m_UploadPool;
    vk::Fence m_UploadFence;
    bool m_Validation = false;
    bool m_Presentation = false; // instance has VK_KHR_surface
    bool m_DepthClamp = false;
    float m_MaxAnisotropy = 1.0f;

    struct Garbage { uint64_t frame; std::function<void()> destroy; };
    std::deque<Garbage> m_Garbage;
    uint64_t m_FrameNumber = 0;
};

} // namespace Astral::Render::Vk
