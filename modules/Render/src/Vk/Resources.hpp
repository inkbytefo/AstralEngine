#pragma once

#include "Vk/VkCommon.hpp"

#include <string>

namespace Astral::Render::Vk {

class Device;

enum class MemoryUsage {
    GpuOnly,   // device local
    Upload,    // host visible, persistently mapped, written by the CPU each frame
    Readback,  // host visible + cached, read by the CPU
};

// Move-only GPU buffer. Destruction goes through the device's retire queue.
class Buffer {
public:
    Buffer() = default;
    Buffer(Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage, MemoryUsage memory, const std::string& name = {});
    ~Buffer();
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    vk::Buffer Get() const { return m_Buffer; }
    vk::DeviceSize Size() const { return m_Size; }
    void* Mapped() const { return m_Mapped; }
    explicit operator bool() const { return static_cast<bool>(m_Buffer); }

    // Writes into a mapped (Upload/Readback) buffer.
    void Write(const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);
    void Invalidate(); // for Readback buffers before reading

private:
    void Release();
    Device* m_Device = nullptr;
    vk::Buffer m_Buffer;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    vk::DeviceSize m_Size = 0;
    void* m_Mapped = nullptr;
};

// Synchronisation history of a resource, used by the render graph to place barriers.
// It records the last write and which stages/accesses that write has already been
// made visible to, plus the readers since then (for write-after-read hazards).
// Resources keep this across frames, so history images and swapchain images stay correct.
struct SyncState {
    vk::PipelineStageFlags2 writeStages = vk::PipelineStageFlagBits2::eNone;
    vk::AccessFlags2 writeAccess = vk::AccessFlagBits2::eNone;
    vk::PipelineStageFlags2 visibleStages = vk::PipelineStageFlagBits2::eNone;
    vk::AccessFlags2 visibleAccess = vk::AccessFlagBits2::eNone;
    vk::PipelineStageFlags2 readStages = vk::PipelineStageFlagBits2::eNone;
};

struct ImageState : SyncState {
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

struct ImageDesc {
    vk::Extent3D extent{1, 1, 1};
    vk::Format format = vk::Format::eR8G8B8A8Unorm;
    vk::ImageUsageFlags usage;
    uint32_t mipLevels = 1;
    uint32_t layers = 1;
    bool cube = false;
    bool is3D = false;
    std::string name;
};

class Image {
public:
    Image() = default;
    Image(Device& device, const ImageDesc& desc);
    ~Image();
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    vk::Image Get() const { return m_Image; }
    vk::ImageView View() const { return m_View; }          // all mips/layers
    vk::ImageView MipView(uint32_t mip) const;               // single mip (all layers, 2D array for cubes)
    vk::ImageView LayerArrayView() const { return m_ArrayView ? m_ArrayView : m_View; }
    const ImageDesc& Desc() const { return m_Desc; }
    vk::Extent3D Extent() const { return m_Desc.extent; }
    vk::Format Format() const { return m_Desc.format; }
    vk::ImageAspectFlags Aspect() const;
    vk::ImageSubresourceRange FullRange() const;
    explicit operator bool() const { return static_cast<bool>(m_Image); }

    ImageState& State() { return m_State; }

private:
    void Release();
    Device* m_Device = nullptr;
    ImageDesc m_Desc;
    vk::Image m_Image;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    vk::ImageView m_View;
    vk::ImageView m_ArrayView;
    std::vector<vk::ImageView> m_MipViews;
    ImageState m_State;
};

bool IsDepthFormat(vk::Format format);

// Records a barrier for a single image and updates its tracked state.
void TransitionImage(vk::CommandBuffer cmd, Image& image, vk::ImageLayout layout,
                     vk::PipelineStageFlags2 stages, vk::AccessFlags2 access);

// Uploads RGBA8 pixels into mip 0 and (optionally) generates the mip chain.
void UploadImage(Device& device, Image& image, const void* pixels, size_t size, bool generateMips);

} // namespace Astral::Render::Vk
