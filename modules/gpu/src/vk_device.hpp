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
    ResourceState state = ResourceState::Undefined; ///< As of the end of everything recorded so far.
    std::uint64_t last_recorder = 0;                ///< CommandList serial that last transitioned it (0: none open).
};

struct PipelineRecord {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct AccelRecord {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    BufferRecord storage;       ///< Backing memory of the structure.
    BufferRecord scratch;       ///< TLAS only: build scratch, reused every build.
    std::uint64_t address = 0;
    std::uint32_t max_instances = 0; ///< 0 for a BLAS.
};

struct QueryRecord {
    VkQueryPool pool = VK_NULL_HANDLE;
    std::uint32_t count = 0;
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
    struct AdapterRecord {
        std::string name;
        std::string driver_name;
        std::uint32_t vendor_id = 0;
        std::uint32_t device_id = 0;
        std::uint32_t driver_id = 0;
        AdapterType type = AdapterType::Other;
    };
    AdapterRecord adapter;
    struct Capabilities {
        bool validation = false;
        bool presentation = false;
        bool anisotropy = false;
        bool texture_compression_bc = false;
        bool debug_utils = false;
        bool timestamps = false;
        double timestamp_period = 1.0;       ///< Nanoseconds per tick.
        std::uint64_t timestamp_mask = ~0ull; ///< Valid bits of a timestamp.
    };
    Capabilities caps;
    VkSurfaceKHR surface = VK_NULL_HANDLE; ///< Created from DeviceDesc::surface, owned here.

    // Synchronization: one timeline semaphore, value = submission count.
    VkSemaphore timeline = VK_NULL_HANDLE;
    std::uint64_t last_submitted = 0;

    // Command buffers, recycled once their submission completed.
    struct CommandSlot {
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        VkCommandBuffer fixup = VK_NULL_HANDLE; ///< Layout fix-ups submitted just before `buffer` (allocated on demand).
        std::uint64_t ticket = 0;
        bool recording = false;
    };
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<CommandSlot> command_slots;
    /// Transitions of dropped lists that later lists built on: run before the next submitted list.
    struct LayoutFixup {
        TextureHandle texture;
        ResourceState from;
        ResourceState to;
    };
    struct Recording {
        std::uint64_t next_serial = 1;
        std::vector<LayoutFixup> fixups;
    };
    Recording recording;

    // Bindless: one descriptor set shared by every pipeline.
    struct BindlessState {
        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VkSampler samplers[3] = {};
        std::uint32_t max_textures = 0;
        std::uint32_t next_bindless = 0;
        std::vector<std::uint32_t> free_bindless;
    };
    BindlessState bindless;

    HandlePool<BufferRecord, BufferTag> buffers;
    HandlePool<TextureRecord, TextureTag> textures;
    HandlePool<PipelineRecord, PipelineTag> pipelines;
    HandlePool<AccelRecord, AccelerationStructureTag> accels;
    HandlePool<QueryRecord, QueryPoolTag> queries;

    // Ray tracing (VK_KHR_acceleration_structure + VK_KHR_ray_query), when supported.
    struct RayTracingDispatch {
        bool supported = false;
        std::uint32_t scratch_alignment = 256;
        PFN_vkCreateAccelerationStructureKHR create_as = nullptr;
        PFN_vkDestroyAccelerationStructureKHR destroy_as = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR as_build_sizes = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR cmd_build_as = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR as_address = nullptr;
    };
    RayTracingDispatch rt;

    // Deferred destruction: run `destroy` once the timeline reaches `value`.
    struct Garbage {
        std::uint64_t value;
        std::function<void()> destroy;
        /// Command slots that were recording at retire time: the resource may be referenced by
        /// them, so it waits until each is submitted (raising `value`) or dropped.
        std::vector<std::uint32_t> open_slots;
    };
    std::deque<Garbage> garbage;

    struct DebugUtilsDispatch {
        PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_label = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_label = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT set_object_name = nullptr;
    };
    DebugUtilsDispatch debug;

    ~Impl();

    std::uint64_t completed_value() const;
    void retire(std::function<void()> destroy);
    void collect_garbage();
    /// A recording slot ended: submitted as `ticket`, or dropped/failed when `ticket` is 0.
    void release_slot(std::uint32_t slot, std::uint64_t ticket);
    /// Records `recording.fixups` into `slot.fixup`; false when none applies (textures gone).
    bool record_layout_fixups(CommandSlot& slot);
    void wait_value(std::uint64_t value);
    /// Records, submits and waits for a one-off command buffer.
    void immediate(const std::function<void(VkCommandBuffer)>& record);
    void name_object(VkObjectType type, std::uint64_t handle, std::string_view name);
    std::uint32_t allocate_bindless();
    void free_bindless_index(std::uint32_t index);
    void write_texture_descriptors(const TextureRecord& texture);
    void log_error(std::string_view message) const;
    bool any_recording() const noexcept;
    /// Refuses (with a log) blocking uploads/readbacks while a CommandList records.
    bool can_run_immediate(const char* operation) const;
};

bool has_device_extension(VkPhysicalDevice device, const char* name);
bool init_vulkan_device(Device::Impl& s, const DeviceDesc& desc);

VkFormat to_vk(Format format) noexcept;
Format from_vk(VkFormat format) noexcept;

struct StateInfo {
    VkImageLayout layout;
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};
StateInfo state_info(ResourceState state, bool depth) noexcept;

void destroy_buffer_record(Device::Impl& s, const BufferRecord& r);
void destroy_accel_record(Device::Impl& s, const AccelRecord& r);
/// A buffer with raw Vulkan usage flags (acceleration structure storage/scratch/inputs).
bool create_buffer_raw_usage(Device::Impl& s, std::uint64_t size, VkBufferUsageFlags usage,
                             MemoryUsage memory, std::uint64_t alignment, BufferRecord& out);
void destroy_texture_record(Device::Impl& s, const TextureRecord& r);
VkImageView create_view(Device::Impl& s, VkImage image, VkFormat format, bool depth,
                        std::uint32_t mips);

} // namespace astral::gpu
