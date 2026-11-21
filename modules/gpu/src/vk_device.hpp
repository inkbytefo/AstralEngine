#pragma once
// Private Vulkan backend state shared by the gpu module's translation units.

#include <astral/core/handle.hpp>
#include <astral/gpu/device.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace astral::gpu {

struct BufferRecord {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    void* mapped = nullptr;
    std::uint64_t size = 0;
    std::uint64_t address = 0;
    BufferUsage usage = BufferUsage::None;
    MemoryUsage memory = MemoryUsage::GpuOnly;
};

struct TextureRecord {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr; ///< Null for swapchain images (not owned).
    TextureDesc desc{};
    std::uint32_t bindless = UINT32_MAX;
    ResourceState state = ResourceState::Undefined;
};

struct PipelineRecord {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct Device::Impl {
    Logger* logger = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family = 0;
    VmaAllocator allocator = nullptr;
    std::string adapter;
    bool validation = false;
    bool presentation = false;
    bool debug_utils = false;
    VkSurfaceKHR surface = VK_NULL_HANDLE; ///< Created from DeviceDesc::surface, owned here.

    // Synchronization: one timeline semaphore, value = submission count.
    VkSemaphore timeline = VK_NULL_HANDLE;
    std::uint64_t last_submitted = 0;
    // Extra binary semaphores for the next submit (set by the swapchain).
    std::vector<VkSemaphoreSubmitInfo> next_waits;
    std::vector<VkSemaphoreSubmitInfo> next_signals;

    // Command buffers, recycled once their submission completed.
    struct CommandSlot {
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        std::uint64_t ticket = 0;
        bool recording = false;
    };
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<CommandSlot> command_slots;

    // Bindless: one descriptor set shared by every pipeline.
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkSampler samplers[3] = {};
    std::uint32_t max_textures = 0;
    std::uint32_t next_bindless = 0;
    std::vector<std::uint32_t> free_bindless;

    HandlePool<BufferRecord, BufferTag> buffers;
    HandlePool<TextureRecord, TextureTag> textures;
    HandlePool<PipelineRecord, PipelineTag> pipelines;

    // Deferred destruction: run `destroy` once the timeline reaches `value`.
    struct Garbage {
        std::uint64_t value;
        std::function<void()> destroy;
    };
    std::deque<Garbage> garbage;

    PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_label = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_label = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT set_object_name = nullptr;

    ~Impl();

    std::uint64_t completed_value() const;
    void retire(std::function<void()> destroy);
    void collect_garbage();
    void wait_value(std::uint64_t value);
    /// Records, submits and waits for a one-off command buffer.
    void immediate(const std::function<void(VkCommandBuffer)>& record);
    void name_object(VkObjectType type, std::uint64_t handle, std::string_view name);
    std::uint32_t allocate_bindless();
    void free_bindless_index(std::uint32_t index);
    void write_texture_descriptors(const TextureRecord& texture);
    void log_error(std::string_view message) const;
};

VkFormat to_vk(Format format) noexcept;
Format from_vk(VkFormat format) noexcept;

struct StateInfo {
    VkImageLayout layout;
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};
StateInfo state_info(ResourceState state, bool depth) noexcept;

void destroy_buffer_record(Device::Impl& s, const BufferRecord& r);
void destroy_texture_record(Device::Impl& s, const TextureRecord& r);
VkImageView create_view(Device::Impl& s, VkImage image, VkFormat format, bool depth,
                        std::uint32_t mips);

} // namespace astral::gpu
