#include "vk_device.hpp"

#include <astral/core/log.hpp>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace astral::gpu {

// --- Impl helpers ----------------------------------------------------------------------

void Device::Impl::log_error(std::string_view message) const {
    if (logger) {
        logger->log(LogLevel::Error, message);
    } else {
        std::fprintf(stderr, "[astral::gpu] %.*s\n", static_cast<int>(message.size()),
                     message.data());
    }
}

std::uint64_t Device::Impl::completed_value() const {
    std::uint64_t value = 0;
    vkGetSemaphoreCounterValue(device, timeline, &value);
    return value;
}

void Device::Impl::retire(std::function<void()> destroy) {
    // Submitted work may still use the resource; so may every list being recorded right now,
    // whatever ticket it ends up with (lists can be submitted in any order).
    Garbage g{last_submitted, std::move(destroy), {}};
    for (std::uint32_t i = 0; i < command_slots.size(); ++i) {
        if (command_slots[i].recording) g.open_slots.push_back(i);
    }
    garbage.push_back(std::move(g));
}

void Device::Impl::release_slot(std::uint32_t slot, std::uint64_t ticket) {
    for (Garbage& g : garbage) {
        const auto it = std::find(g.open_slots.begin(), g.open_slots.end(), slot);
        if (it == g.open_slots.end()) continue;
        g.open_slots.erase(it);
        g.value = std::max(g.value, ticket);
    }
}

bool Device::Impl::any_recording() const noexcept {
    for (const auto& c : command_slots) {
        if (c.recording) return true;
    }
    return false;
}

bool Device::Impl::can_run_immediate(const char* operation) const {
    if (!any_recording()) return true;
    log_error(std::string(operation) + ": refused while a CommandList is being recorded "
                                       "(submit it first, or use a host-visible buffer)");
    return false;
}

void Device::Impl::collect_garbage() {
    const std::uint64_t done = completed_value();
    // Take the finished entries out first: a destroy callback may retire more resources, which
    // appends to `garbage` and would invalidate iterators into it.
    std::vector<std::function<void()>> ready;
    for (auto it = garbage.begin(); it != garbage.end();) {
        if (it->open_slots.empty() && it->value <= done) {
            ready.push_back(std::move(it->destroy));
            it = garbage.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& destroy : ready) destroy();
}

void Device::Impl::wait_value(std::uint64_t value) {
    if (value == 0) return;
    VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    info.semaphoreCount = 1;
    info.pSemaphores = &timeline;
    info.pValues = &value;
    vkWaitSemaphores(device, &info, UINT64_MAX);
}

void Device::Impl::name_object(VkObjectType type, std::uint64_t handle, std::string_view name) {
    if (!debug.set_object_name || name.empty()) return;
    const std::string copy(name);
    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = copy.c_str();
    debug.set_object_name(device, &info);
}

std::uint32_t Device::Impl::allocate_bindless() {
    if (!bindless.free_bindless.empty()) {
        const std::uint32_t index = bindless.free_bindless.back();
        bindless.free_bindless.pop_back();
        return index;
    }
    return bindless.next_bindless < bindless.max_textures ? bindless.next_bindless++ : UINT32_MAX;
}

void Device::Impl::free_bindless_index(std::uint32_t index) {
    if (index != UINT32_MAX) bindless.free_bindless.push_back(index);
}

void Device::Impl::write_texture_descriptors(const TextureRecord& texture) {
    VkDescriptorImageInfo sampled{VK_NULL_HANDLE, texture.view,
                                  state_info(ResourceState::ShaderRead, is_depth(texture.desc.format)).layout};
    VkDescriptorImageInfo storage{VK_NULL_HANDLE, texture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet writes[2]{};
    std::uint32_t count = 0;
    if (has_flag(texture.desc.usage, TextureUsage::Sampled)) {
        auto& w = writes[count++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = bindless.descriptor_set;
        w.dstBinding = 0;
        w.dstArrayElement = texture.bindless;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w.pImageInfo = &sampled;
    }
    if (has_flag(texture.desc.usage, TextureUsage::Storage)) {
        auto& w = writes[count++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = bindless.descriptor_set;
        w.dstBinding = 1;
        w.dstArrayElement = texture.bindless;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo = &storage;
    }
    if (count) vkUpdateDescriptorSets(device, count, writes, 0, nullptr);
}

void Device::Impl::immediate(const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc, &cmd);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    // Order against everything submitted before and after (uploads are rare: simplicity wins).
    VkMemoryBarrier2 full{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    full.srcStageMask = full.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    full.srcAccessMask = full.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &full;
    vkCmdPipelineBarrier2(cmd, &dep);
    record(cmd);
    full.dstAccessMask |= VK_ACCESS_2_HOST_READ_BIT;
    full.dstStageMask |= VK_PIPELINE_STAGE_2_HOST_BIT;
    vkCmdPipelineBarrier2(cmd, &dep);
    vkEndCommandBuffer(cmd);

    const std::uint64_t value = ++last_submitted;
    VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmd_info.commandBuffer = cmd;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = timeline;
    signal.value = value;
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmd_info;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
    vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE);
    wait_value(value);
    vkFreeCommandBuffers(device, command_pool, 1, &cmd);
}

Device::Impl::~Impl() {
    if (device) {
        vkDeviceWaitIdle(device);
        while (!garbage.empty()) {
            garbage.front().destroy();
            garbage.pop_front();
        }
        for (auto& sampler : bindless.samplers) vkDestroySampler(device, sampler, nullptr);
        vkDestroyPipelineLayout(device, bindless.pipeline_layout, nullptr);
        vkDestroyDescriptorPool(device, bindless.descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, bindless.set_layout, nullptr);
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroySemaphore(device, timeline, nullptr);
        if (allocator) vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
    }
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (messenger) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(instance, messenger, nullptr);
    }
    if (instance) vkDestroyInstance(instance, nullptr);
}

std::unique_ptr<Device> Device::create(const DeviceDesc& desc) {
    auto state = std::make_unique<Impl>();
    state->logger = desc.logger;
    if (!init_vulkan_device(*state, desc)) return nullptr;
    return std::unique_ptr<Device>(new Device(std::move(state)));
}

Device::Device(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Device::~Device() {
    if (!impl_ || !impl_->device) return;
    auto& s = *impl_;
    vkDeviceWaitIdle(s.device);
    s.collect_garbage();
    // Resources the user never destroyed are released here.
    s.buffers.for_each([&](BufferHandle, BufferRecord& r) { destroy_buffer_record(s, r); });
    s.textures.for_each([&](TextureHandle, TextureRecord& r) { destroy_texture_record(s, r); });
    s.accels.for_each([&](AccelerationStructureHandle, AccelRecord& r) { destroy_accel_record(s, r); });
    s.queries.for_each([&](QueryPoolHandle, QueryRecord& r) { vkDestroyQueryPool(s.device, r.pool, nullptr); });
    s.pipelines.for_each([&](PipelineHandle, PipelineRecord& r) {
        vkDestroyPipeline(s.device, r.pipeline, nullptr);
    });
}

std::string_view Device::adapter_name() const noexcept { return impl_->adapter.name; }

AdapterInfo Device::adapter_info() const noexcept {
    const auto& a = impl_->adapter;
    return AdapterInfo{
        .name = a.name,
        .vendor_id = a.vendor_id,
        .device_id = a.device_id,
        .driver_id = a.driver_id,
        .driver_name = a.driver_name,
        .type = a.type,
    };
}

void Device::report_error(std::string_view message) const { impl_->log_error(message); }

bool Device::supports(Feature feature) const noexcept {
    switch (feature) {
    case Feature::BufferDeviceAddress:
    case Feature::Bindless: return true;
    case Feature::Validation: return impl_->caps.validation;
    case Feature::Presentation: return impl_->caps.presentation;
    case Feature::RayTracing: return impl_->rt.supported;
    case Feature::TextureCompressionBC: return impl_->caps.texture_compression_bc;
    case Feature::Timestamps: return impl_->caps.timestamps;
    case Feature::MeshShaders:
        return has_device_extension(impl_->physical, "VK_EXT_mesh_shader");
    }
    return false;
}

// --- Work submission -------------------------------------------------------------------

CommandList Device::begin_commands() {
    auto& s = *impl_;
    s.collect_garbage();
    const std::uint64_t done = s.completed_value();
    std::uint32_t slot = UINT32_MAX;
    for (std::uint32_t i = 0; i < s.command_slots.size(); ++i) {
        const auto& c = s.command_slots[i];
        if (!c.recording && c.ticket <= done) {
            slot = i;
            break;
        }
    }
    if (slot == UINT32_MAX) {
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool = s.command_pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        Impl::CommandSlot fresh;
        vkAllocateCommandBuffers(s.device, &alloc, &fresh.buffer);
        s.command_slots.push_back(fresh);
        slot = static_cast<std::uint32_t>(s.command_slots.size() - 1);
    }
    auto& c = s.command_slots[slot];
    c.recording = true;
    vkResetCommandBuffer(c.buffer, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(c.buffer, &begin);
    // Bindless set is bound once for both bind points; every pipeline shares the layout.
    vkCmdBindDescriptorSets(c.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s.bindless.pipeline_layout, 0, 1,
                            &s.bindless.descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(c.buffer, VK_PIPELINE_BIND_POINT_COMPUTE, s.bindless.pipeline_layout, 0, 1,
                            &s.bindless.descriptor_set, 0, nullptr);
    return CommandList(this, c.buffer, slot);
}

SubmitTicket Device::submit(CommandList& commands) {
    return submit_internal(commands, nullptr, 0, nullptr, 0);
}

SubmitTicket Device::submit_internal(CommandList& commands, const void* waits, std::uint32_t wait_count,
                                     const void* signals, std::uint32_t signal_count) {
    auto& s = *impl_;
    if (!commands.valid()) return {s.last_submitted};
    if (commands.device_ != this) {
        s.log_error("submit: the command list belongs to another device");
        return {s.last_submitted};
    }
    if (commands.rendering_) commands.end_rendering();
    auto& slot = s.command_slots[commands.slot_];
    vkEndCommandBuffer(slot.buffer);

    const std::uint64_t value = s.last_submitted + 1;
    VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmd_info.commandBuffer = slot.buffer;
    std::vector<VkSemaphoreSubmitInfo> signal_infos;
    if (signal_count) {
        auto* begin = static_cast<const VkSemaphoreSubmitInfo*>(signals);
        signal_infos.assign(begin, begin + signal_count);
    }
    VkSemaphoreSubmitInfo timeline_signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    timeline_signal.semaphore = s.timeline;
    timeline_signal.value = value;
    timeline_signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signal_infos.push_back(timeline_signal);

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = wait_count;
    submit.pWaitSemaphoreInfos = static_cast<const VkSemaphoreSubmitInfo*>(waits);
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmd_info;
    submit.signalSemaphoreInfoCount = static_cast<std::uint32_t>(signal_infos.size());
    submit.pSignalSemaphoreInfos = signal_infos.data();
    slot.recording = false;
    commands.device_ = nullptr; // consumed
    commands.touched_.clear();
    auto callbacks = std::move(commands.on_submitted_);
    commands.on_submitted_.clear();
    if (vkQueueSubmit2(s.queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
        s.log_error("vkQueueSubmit2 failed (device lost?)");
        slot.ticket = 0;
        s.release_slot(commands.slot_, 0);
        return {s.last_submitted}; // nothing will signal `value`: do not advance
    }
    s.last_submitted = value;
    slot.ticket = value;
    s.release_slot(commands.slot_, value);
    for (auto& callback : callbacks) callback({value});
    return {value};
}

void Device::wait(SubmitTicket ticket) {
    impl_->wait_value(ticket.value);
    impl_->collect_garbage();
}

bool Device::is_complete(SubmitTicket ticket) const {
    return impl_->completed_value() >= ticket.value;
}

void Device::wait_idle() {
    auto& s = *impl_;
    vkDeviceWaitIdle(s.device);
    s.collect_garbage();
    if (!s.any_recording()) {
        // Nothing can reference retired resources any more.
        while (!s.garbage.empty()) {
            s.garbage.front().destroy();
            s.garbage.pop_front();
        }
    }
}

} // namespace astral::gpu
