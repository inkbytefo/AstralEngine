#include "vk_device.hpp"

#include <astral/gpu/swapchain.hpp>

#include <algorithm>
#include <vector>

namespace astral::gpu {

struct Swapchain::Impl {
    Device* device = nullptr;
    Device::Impl* s = nullptr;
    SurfaceSource surface;
    SwapchainDesc desc;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<TextureHandle> images;
    std::vector<VkSemaphore> render_done; // one per image: safe reuse across frames

    struct Frame {
        VkSemaphore acquired = VK_NULL_HANDLE;
        std::uint64_t ticket = 0; // last submission of this frame slot
    };
    std::vector<Frame> frames;
    std::uint32_t frame = 0;
    std::uint32_t image_index = UINT32_MAX;
    bool dirty = false;

    ~Impl() {
        if (!s) return;
        vkDeviceWaitIdle(s->device);
        release_images();
        for (auto sem : render_done) vkDestroySemaphore(s->device, sem, nullptr);
        for (auto& f : frames) vkDestroySemaphore(s->device, f.acquired, nullptr);
        if (swapchain) vkDestroySwapchainKHR(s->device, swapchain, nullptr);
    }

    void release_images() {
        for (auto h : images) {
            if (const TextureRecord* r = s->textures.get(h)) {
                vkDestroyImageView(s->device, r->view, nullptr); // image belongs to the swapchain
                s->textures.destroy(h);
            }
        }
        images.clear();
    }

    VkSurfaceFormatKHR choose_format() const {
        std::uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(s->physical, s->surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(s->physical, s->surface, &count, formats.data());
        const VkFormat wanted = to_vk(desc.format);
        for (const auto& f : formats) {
            if (f.format == wanted && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
        }
        for (const auto& f : formats) {
            if (from_vk(f.format) != Format::Undefined) return f;
        }
        return formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
    }

    VkPresentModeKHR choose_present_mode() const {
        if (desc.vsync) return VK_PRESENT_MODE_FIFO_KHR; // always supported
        std::uint32_t count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(s->physical, s->surface, &count, nullptr);
        std::vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(s->physical, s->surface, &count, modes.data());
        for (auto m : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
            if (std::find(modes.begin(), modes.end(), m) != modes.end()) return m;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /// (Re)creates the swapchain. Returns false when the surface has zero size.
    bool rebuild() {
        const Extent2D wanted = surface.drawable_size ? surface.drawable_size() : Extent2D{};
        if (wanted.width == 0 || wanted.height == 0) return false;

        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->physical, s->surface, &caps);
        VkExtent2D ext = caps.currentExtent;
        if (ext.width == UINT32_MAX) {
            ext.width = std::clamp(wanted.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            ext.height = std::clamp(wanted.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (ext.width == 0 || ext.height == 0) return false;

        vkDeviceWaitIdle(s->device); // simple and safe; resizes are rare
        release_images();
        for (auto sem : render_done) vkDestroySemaphore(s->device, sem, nullptr);
        render_done.clear();

        const VkSurfaceFormatKHR surface_format = choose_format();
        std::uint32_t image_count = std::max(caps.minImageCount + 1, desc.frames_in_flight);
        if (caps.maxImageCount) image_count = std::min(image_count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = s->surface;
        info.minImageCount = image_count;
        info.imageFormat = surface_format.format;
        info.imageColorSpace = surface_format.colorSpace;
        info.imageExtent = ext;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = choose_present_mode();
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapchain;
        VkSwapchainKHR fresh = VK_NULL_HANDLE;
        if (vkCreateSwapchainKHR(s->device, &info, nullptr, &fresh) != VK_SUCCESS) {
            s->log_error("vkCreateSwapchainKHR failed");
            return false;
        }
        if (swapchain) vkDestroySwapchainKHR(s->device, swapchain, nullptr);
        swapchain = fresh;
        format = surface_format.format;
        extent = ext;

        std::uint32_t count = 0;
        vkGetSwapchainImagesKHR(s->device, swapchain, &count, nullptr);
        std::vector<VkImage> vk_images(count);
        vkGetSwapchainImagesKHR(s->device, swapchain, &count, vk_images.data());
        VkSemaphoreCreateInfo sem_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (VkImage image : vk_images) {
            TextureRecord record;
            record.image = image;
            record.view = create_view(*s, image, format, false, 1);
            record.desc = {.width = ext.width, .height = ext.height, .format = from_vk(format),
                           .usage = TextureUsage::ColorTarget | TextureUsage::TransferDst};
            images.push_back(s->textures.create(record));
            VkSemaphore sem = VK_NULL_HANDLE;
            vkCreateSemaphore(s->device, &sem_info, nullptr, &sem);
            render_done.push_back(sem);
        }
        dirty = false;
        return true;
    }
};

std::unique_ptr<Swapchain> Swapchain::create(Device& device, const SurfaceSource& surface,
                                             const SwapchainDesc& desc) {
    auto& s = device.impl();
    if (!s.surface) {
        s.log_error("Swapchain::create: device was created without a surface");
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->s = &s;
    impl->surface = surface;
    impl->desc = desc;
    impl->frames.resize(std::max(desc.frames_in_flight, 1u));
    VkSemaphoreCreateInfo sem_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (auto& f : impl->frames) vkCreateSemaphore(s.device, &sem_info, nullptr, &f.acquired);
    impl->rebuild(); // may fail for a 0x0 window; acquire() retries
    return std::unique_ptr<Swapchain>(new Swapchain(std::move(impl)));
}

Swapchain::Swapchain(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Swapchain::~Swapchain() = default;

TextureHandle Swapchain::acquire() {
    auto& m = *impl_;
    auto& s = *m.s;
    if (m.dirty || !m.swapchain) {
        if (!m.rebuild()) return {};
    }
    // Throttle: this frame slot's previous submission must be finished.
    auto& frame = m.frames[m.frame];
    s.wait_value(frame.ticket);

    VkResult result = vkAcquireNextImageKHR(s.device, m.swapchain, UINT64_MAX, frame.acquired,
                                            VK_NULL_HANDLE, &m.image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m.dirty = true;
        m.image_index = UINT32_MAX;
        return {};
    }
    if (result == VK_SUBOPTIMAL_KHR) m.dirty = true; // still usable this frame
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        m.image_index = UINT32_MAX;
        return {};
    }
    // The next submit waits for the image and signals that rendering finished.
    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = frame.acquired;
    wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = m.render_done[m.image_index];
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    s.next_waits.push_back(wait);
    s.next_signals.push_back(signal);

    TextureHandle image = m.images[m.image_index];
    if (TextureRecord* r = s.textures.get(image)) r->state = ResourceState::Undefined; // contents discarded
    return image;
}

void Swapchain::present() {
    auto& m = *impl_;
    auto& s = *m.s;
    if (m.image_index == UINT32_MAX) return;
    VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &m.render_done[m.image_index];
    info.swapchainCount = 1;
    info.pSwapchains = &m.swapchain;
    info.pImageIndices = &m.image_index;
    const VkResult result = vkQueuePresentKHR(s.queue, &info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) m.dirty = true;
    m.frames[m.frame].ticket = s.last_submitted;
    m.frame = (m.frame + 1) % static_cast<std::uint32_t>(m.frames.size());
    m.image_index = UINT32_MAX;
}

void Swapchain::invalidate() noexcept { impl_->dirty = true; }
Format Swapchain::format() const noexcept { return from_vk(impl_->format); }
Extent2D Swapchain::extent() const noexcept { return {impl_->extent.width, impl_->extent.height}; }

} // namespace astral::gpu
