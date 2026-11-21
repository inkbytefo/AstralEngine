#pragma once

#include "Vk/VkCommon.hpp"

#include <span>
#include <string>
#include <vector>

namespace Astral::Render::Vk {

class Device;
class Image;
class Buffer;

std::span<const uint32_t> FindShader(std::string_view name); // throws if missing

// Resources a pass binds. Pass resources use push descriptors (set 0); material
// textures come from the bindless table (set 1) when `bindlessTextures` is set.
struct Binding {
    uint32_t binding;
    vk::DescriptorType type;
    uint32_t count = 1;
};

struct PipelineLayoutDesc {
    std::vector<Binding> bindings;
    uint32_t pushConstantSize = 0;
    bool bindlessTextures = false;
};

class Pipeline {
public:
    Pipeline() = default;
    ~Pipeline();
    Pipeline(Pipeline&& o) noexcept;
    Pipeline& operator=(Pipeline&& o) noexcept;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    static Pipeline Compute(Device& device, const std::string& shader, const PipelineLayoutDesc& layout,
                            vk::DescriptorSetLayout bindlessLayout = {});

    struct GraphicsDesc {
        std::string vertexShader;
        std::string fragmentShader;          // empty = depth-only
        std::vector<vk::Format> colorFormats;
        vk::Format depthFormat = vk::Format::eUndefined;
        bool depthTest = true;
        bool depthWrite = true;
        vk::CompareOp depthCompare = vk::CompareOp::eGreaterOrEqual; // reversed Z
        vk::CullModeFlags cull = vk::CullModeFlagBits::eBack;
        bool depthClamp = false;
        bool depthBias = false;
        bool meshVertexInput = true;         // binds Render::Vertex at binding 0
    };
    static Pipeline Graphics(Device& device, const GraphicsDesc& desc, const PipelineLayoutDesc& layout,
                             vk::DescriptorSetLayout bindlessLayout = {});

    vk::Pipeline Get() const { return m_Pipeline; }
    vk::PipelineLayout Layout() const { return m_Layout; }
    vk::PipelineBindPoint BindPoint() const { return m_BindPoint; }
    explicit operator bool() const { return static_cast<bool>(m_Pipeline); }

    void Bind(vk::CommandBuffer cmd) const { cmd.bindPipeline(m_BindPoint, m_Pipeline); }
    template <typename T>
    void Push(vk::CommandBuffer cmd, const T& constants) const {
        cmd.pushConstants(m_Layout, vk::ShaderStageFlagBits::eAll, 0, sizeof(T), &constants);
    }

private:
    void CreateLayout(Device& device, const PipelineLayoutDesc& desc, vk::DescriptorSetLayout bindless);
    void Release();

    Device* m_Device = nullptr;
    vk::Pipeline m_Pipeline;
    vk::PipelineLayout m_Layout;
    vk::DescriptorSetLayout m_SetLayout;
    vk::PipelineBindPoint m_BindPoint = vk::PipelineBindPoint::eCompute;
};

// Collects resource bindings and pushes them as set 0 of a pipeline.
class Bindings {
public:
    Bindings& StorageImage(uint32_t binding, vk::ImageView view);
    Bindings& SampledImage(uint32_t binding, vk::ImageView view, vk::Sampler sampler,
                           vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
    Bindings& StorageBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);
    Bindings& UniformBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);
    void Push(vk::CommandBuffer cmd, const Pipeline& pipeline);

private:
    std::vector<vk::WriteDescriptorSet> m_Writes;
    std::vector<vk::DescriptorImageInfo> m_Images;   // reserved up front so pointers stay valid
    std::vector<vk::DescriptorBufferInfo> m_Buffers;
    struct Pending { uint32_t binding; vk::DescriptorType type; bool image; size_t index; };
    std::vector<Pending> m_Pending;
};

// Global table of sampled textures (set 1, binding 0) indexed by material data.
class BindlessTextures {
public:
    static constexpr uint32_t kCapacity = 4096;
    explicit BindlessTextures(Device& device);
    ~BindlessTextures();

    uint32_t Allocate(vk::ImageView view, vk::Sampler sampler);
    void Free(uint32_t slot);
    vk::DescriptorSetLayout Layout() const { return m_Layout; }
    vk::DescriptorSet Set() const { return m_Set; }

private:
    Device& m_Device;
    vk::DescriptorPool m_Pool;
    vk::DescriptorSetLayout m_Layout;
    vk::DescriptorSet m_Set;
    std::vector<uint32_t> m_Free;
    uint32_t m_Next = 0;
};

} // namespace Astral::Render::Vk
