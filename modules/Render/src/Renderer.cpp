#include "Detail/RendererImpl.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Astral::Render {

using Detail::kInvalidIndex;

namespace {

Vk::PipelineLayoutDesc SceneLayout() {
    return {{{0, vk::DescriptorType::eUniformBuffer}, {1, vk::DescriptorType::eStorageBuffer}, {2, vk::DescriptorType::eStorageBuffer}},
            sizeof(uint32_t), true};
}

} // namespace

// ============================================================================ Impl

Renderer::Impl::Impl(const RendererDesc& d) : desc(d) {
    Vk::DeviceDesc deviceDesc;
    deviceDesc.applicationName = desc.applicationName;
    deviceDesc.validation = desc.enableValidation;
    deviceDesc.instanceExtensions = desc.instanceExtensions;
    device = std::make_unique<Vk::Device>(deviceDesc);
    graph = RenderGraph(device->ValidationEnabled());
    bindless = std::make_unique<Vk::BindlessTextures>(*device);

    auto worldImpl = std::make_unique<World::Impl>();
    worldImpl->volume = desc.sdfVolume;
    world.reset(new World(std::move(worldImpl)));
    // SDFs are optional: start with a 1-cell placeholder volume and allocate the real
    // one (tens of MB) only when the first SDF shape appears.
    SdfVolumeSettings placeholder = desc.sdfVolume;
    placeholder.cells = glm::uvec3(1);
    placeholder.atlasBricks = glm::uvec3(1);
    sdfVolume = std::make_unique<SdfVolume>(*device, placeholder);

    vk::Device dev = device->Get();
    vk::SamplerCreateInfo sampler;
    sampler.magFilter = sampler.minFilter = vk::Filter::eLinear;
    sampler.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler.maxLod = VK_LOD_CLAMP_NONE;
    linearClamp = dev.createSampler(sampler);
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler.anisotropyEnable = device->MaxAnisotropy() > 1.0f;
    sampler.maxAnisotropy = device->MaxAnisotropy();
    linearRepeat = dev.createSampler(sampler);
    sampler.anisotropyEnable = VK_FALSE;
    sampler.magFilter = sampler.minFilter = vk::Filter::eNearest;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    nearestClamp = dev.createSampler(sampler);

    commandPool = dev.createCommandPool({vk::CommandPoolCreateFlagBits::eResetCommandBuffer, device->QueueFamily()});
    auto cmds = dev.allocateCommandBuffers({commandPool, vk::CommandBufferLevel::ePrimary, kFrames});
    for (uint32_t i = 0; i < kFrames; ++i) {
        FrameSlot& f = frames[i];
        f.cmd = cmds[i];
        f.fence = dev.createFence({vk::FenceCreateFlagBits::eSignaled});
        f.imageAvailable = dev.createSemaphore({});
        f.timestamps = dev.createQueryPool({{}, vk::QueryType::eTimestamp, 2});
        f.frameData = Vk::Buffer(*device, kFrameDataStride * kMaxViewsPerFrame, vk::BufferUsageFlagBits::eUniformBuffer,
                                 Vk::MemoryUsage::Upload, "frame data");
        f.pickReadback = Vk::Buffer(*device, 16, vk::BufferUsageFlagBits::eTransferDst, Vk::MemoryUsage::Readback, "pick readback");
        EnsureCapacity(f.instances, sizeof(Detail::GpuInstance) * 64, vk::BufferUsageFlagBits::eStorageBuffer, "instances");
        EnsureCapacity(f.materials, sizeof(Detail::GpuMaterial) * 64, vk::BufferUsageFlagBits::eStorageBuffer, "materials");
        EnsureCapacity(f.lights, sizeof(Detail::GpuPointLight) * 16, vk::BufferUsageFlagBits::eStorageBuffer, "lights");
        EnsureCapacity(f.sdfShapes, sizeof(Detail::GpuSdfShape) * 64, vk::BufferUsageFlagBits::eStorageBuffer, "sdf shapes");
    }

    // ---- pipelines ----
    const std::vector<vk::Format> gbufferFormats = {vk::Format::eR8G8B8A8Unorm, vk::Format::eR16G16B16A16Sfloat,
                                                    vk::Format::eR16G16B16A16Sfloat, vk::Format::eR32G32Sfloat,
                                                    vk::Format::eR32Uint, vk::Format::eR32Sfloat};
    Vk::Pipeline::GraphicsDesc gbuffer;
    gbuffer.vertexShader = "MeshGBuffer.vert";
    gbuffer.fragmentShader = "MeshGBuffer.frag";
    gbuffer.colorFormats = gbufferFormats;
    gbuffer.depthFormat = vk::Format::eD32Sfloat;
    gbuffer.cull = vk::CullModeFlagBits::eBack;
    meshGBuffer = Vk::Pipeline::Graphics(*device, gbuffer, SceneLayout(), bindless->Layout());

    Vk::Pipeline::GraphicsDesc shadow;
    shadow.vertexShader = "Shadow.vert";
    shadow.depthFormat = vk::Format::eD32Sfloat;
    shadow.cull = vk::CullModeFlagBits::eNone;
    shadow.depthClamp = true;
    auto shadowLayout = SceneLayout();
    shadowLayout.bindlessTextures = false;
    shadowLayout.bindings.pop_back();
    shadowDepth = Vk::Pipeline::Graphics(*device, shadow, shadowLayout);

    using DT = vk::DescriptorType;
    sdfGBuffer = Vk::Pipeline::Compute(*device, "SdfGBuffer.comp",
        {{{0, DT::eUniformBuffer}, {1, DT::eStorageBuffer}, {2, DT::eUniformBuffer}, {3, DT::eStorageImage},
          {4, DT::eCombinedImageSampler}, {5, DT::eStorageImage}, {6, DT::eStorageImage}, {7, DT::eStorageImage},
          {8, DT::eStorageImage}, {9, DT::eStorageImage}, {10, DT::eStorageImage}}, 0, false});
    lighting = Vk::Pipeline::Compute(*device, "Lighting.comp",
        {{{0, DT::eUniformBuffer}, {1, DT::eStorageImage}, {2, DT::eStorageImage}, {3, DT::eStorageImage},
          {4, DT::eStorageImage}, {5, DT::eStorageImage}, {6, DT::eStorageImage}, {7, DT::eCombinedImageSampler},
          {8, DT::eCombinedImageSampler}, {9, DT::eCombinedImageSampler}, {10, DT::eCombinedImageSampler},
          {11, DT::eStorageBuffer}, {12, DT::eStorageImage}, {13, DT::eStorageBuffer}, {14, DT::eUniformBuffer},
          {15, DT::eStorageImage}, {16, DT::eCombinedImageSampler}}, 0, false});
    environmentBake = Vk::Pipeline::Compute(*device, "EnvironmentBake.comp",
        {{{0, DT::eUniformBuffer}, {1, DT::eStorageImage}, {2, DT::eStorageImage}}, 16, false});
    taa = Vk::Pipeline::Compute(*device, "Taa.comp",
        {{{0, DT::eUniformBuffer}, {1, DT::eStorageImage}, {2, DT::eCombinedImageSampler}, {3, DT::eStorageImage},
          {4, DT::eStorageImage}}, sizeof(uint32_t), false});
    tonemap = Vk::Pipeline::Compute(*device, "Tonemap.comp",
        {{{0, DT::eUniformBuffer}, {1, DT::eCombinedImageSampler}, {2, DT::eStorageImage}}, 0, false});

    // ---- shared images ----
    Vk::ImageDesc shadowDesc;
    shadowDesc.extent = vk::Extent3D{kShadowMapSize, kShadowMapSize, 1};
    shadowDesc.format = vk::Format::eD32Sfloat;
    shadowDesc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    shadowDesc.name = "shadow map";
    shadowMap = Vk::Image(*device, shadowDesc);

    Vk::ImageDesc cube;
    cube.format = vk::Format::eR16G16B16A16Sfloat;
    cube.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    cube.layers = 6;
    cube.cube = true;
    cube.extent = vk::Extent3D{kIrradianceSize, kIrradianceSize, 1};
    cube.name = "irradiance";
    irradiance = Vk::Image(*device, cube);
    cube.extent = vk::Extent3D{kPrefilterSize, kPrefilterSize, 1};
    cube.mipLevels = kPrefilterMips;
    cube.name = "prefiltered environment";
    prefiltered = Vk::Image(*device, cube);

    Vk::ImageDesc lut;
    lut.extent = vk::Extent3D{kBrdfLutSize, kBrdfLutSize, 1};
    lut.format = vk::Format::eR16G16B16A16Sfloat;
    lut.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    lut.name = "BRDF LUT";
    brdfLut = Vk::Image(*device, lut);

    MaterialDesc defaults;
    defaults.baseColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    defaults.roughness = 0.6f;
    defaultMaterial = materials.Create(defaults);
}

Renderer::Impl::~Impl() {
    vk::Device dev = device->Get();
    dev.waitIdle();
    swapchain.reset();
    views.ForEach([](ViewHandle, std::unique_ptr<ViewData>& v) { v.reset(); });
    textures.ForEach([&](TextureHandle, std::unique_ptr<TextureData>& t) {
        if (t && t->slot != kInvalidIndex) bindless->Free(t->slot);
        t.reset();
    });
    meshes.ForEach([](MeshHandle, std::unique_ptr<MeshData>& m) { m.reset(); });
    for (FrameSlot& f : frames) {
        dev.destroyFence(f.fence);
        dev.destroySemaphore(f.imageAvailable);
        dev.destroyQueryPool(f.timestamps);
        f = FrameSlot{};
    }
    meshGBuffer = {}; shadowDepth = {}; sdfGBuffer = {}; lighting = {}; environmentBake = {}; taa = {}; tonemap = {};
    shadowMap = {}; irradiance = {}; prefiltered = {}; brdfLut = {};
    sdfVolume.reset();
    device->FlushGarbage(); // runs bindless slot recycling while the table still exists
    bindless.reset();
    dev.destroySampler(linearClamp);
    dev.destroySampler(linearRepeat);
    dev.destroySampler(nearestClamp);
    dev.destroyCommandPool(commandPool);
}

void Renderer::Impl::EnsureCapacity(Vk::Buffer& buffer, vk::DeviceSize bytes, vk::BufferUsageFlags usage, const char* name) {
    if (buffer && buffer.Size() >= bytes) return;
    vk::DeviceSize size = std::max<vk::DeviceSize>(bytes, 256);
    if (buffer) size = std::max(size, buffer.Size() * 2);
    buffer = Vk::Buffer(*device, size, usage, Vk::MemoryUsage::Upload, name);
}

uint32_t Renderer::Impl::MaterialGpuIndex(MaterialHandle handle) const {
    return materials.Contains(handle) ? handle.index : defaultMaterial.index;
}

// ---------------------------------------------------------------------------- frame

bool Renderer::Impl::BeginFrame() {
    if (inFrame) throw std::logic_error("BeginFrame called twice without EndFrame");
    slotIndex = static_cast<uint32_t>(frameNumber % kFrames);
    FrameSlot& f = frames[slotIndex];
    vk::Device dev = device->Get();
    (void)dev.waitForFences(f.fence, VK_TRUE, UINT64_MAX);

    // This slot's previous frame is complete, and with it every frame up to frameNumber - kFrames.
    if (frameNumber >= kFrames) device->CollectGarbage(frameNumber - kFrames);
    if (f.timestampsWritten) {
        uint64_t ts[2] = {0, 0};
        if (dev.getQueryPoolResults(f.timestamps, 0, 2, sizeof(ts), ts, sizeof(uint64_t), vk::QueryResultFlagBits::e64) ==
            vk::Result::eSuccess)
            gpuFrameMs = float(double(ts[1] - ts[0]) * device->Properties().limits.timestampPeriod * 1e-6);
        f.timestampsWritten = false;
    }
    if (f.pick) {
        f.pickReadback.Invalidate();
        uint32_t raw[2];
        std::memcpy(raw, f.pickReadback.Mapped(), sizeof(raw));
        float depth;
        std::memcpy(&depth, &raw[1], sizeof(float));
        PickResult r;
        r.view = f.pick->view;
        r.x = f.pick->x;
        r.y = f.pick->y;
        r.objectId = raw[0];
        if (depth > 0.0f) {
            glm::vec2 uv((f.pick->x + 0.5f) / f.pick->width, (f.pick->y + 0.5f) / f.pick->height);
            glm::vec4 w = f.pick->invViewProj * glm::vec4(uv * 2.0f - 1.0f, depth, 1.0f);
            r.worldPosition = glm::vec3(w) / w.w;
        }
        pickResults.push_back(r);
        f.pick.reset();
    }
    sdfVolume->CollectStats(slotIndex);

    if (swapchain && surfaceResizePending) {
        swapchain->Recreate(pendingSurfaceWidth, pendingSurfaceHeight);
        surfaceResizePending = false;
    }
    acquired = false;
    presentedThisFrame = false;
    if (swapchain) {
        if (swapchain->Extent().width == 0 || swapchain->Extent().height == 0) return false;
        if (!swapchain->Acquire(f.imageAvailable, swapchainImage)) {
            swapchain->Recreate(pendingSurfaceWidth ? pendingSurfaceWidth : swapchain->Extent().width,
                                pendingSurfaceHeight ? pendingSurfaceHeight : swapchain->Extent().height);
            return false;
        }
        acquired = true;
        // The acquire semaphore is waited at ALL_COMMANDS; chain the first barrier onto it.
        Vk::ImageState& state = swapchain->StateAt(swapchainImage);
        state = {};
        state.writeStages = vk::PipelineStageFlagBits2::eAllCommands;
    }

    dev.resetFences(f.fence);
    f.frameNumber = frameNumber;
    device->SetFrameNumber(frameNumber);
    f.cmd.reset();
    f.cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    f.cmd.resetQueryPool(f.timestamps, 0, 2);
    f.cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, f.timestamps, 0);
    inFrame = true;
    worldUploaded = false;
    viewsThisFrame = 0;
    return true;
}

void Renderer::Impl::PresentView(ViewHandle handle) {
    if (!inFrame || !acquired) return;
    ViewData* view = views.TryGet(handle) ? views.Get(handle).get() : nullptr;
    if (!view) return;
    vk::CommandBuffer cmd = frames[slotIndex].cmd;
    vk::Image target = swapchain->ImageAt(swapchainImage);
    Vk::ImageState& targetState = swapchain->StateAt(swapchainImage);
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    graph.Pass(cmd, "Present",
        [&](RenderGraph::PassBuilder& b) {
            b.Image(view->output, Use::TransferSrc).RawImage(target, targetState, range, Use::TransferDst, true);
        },
        [&](vk::CommandBuffer c) {
            vk::Extent2D extent = swapchain->Extent();
            vk::ImageBlit blit;
            blit.srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
            blit.srcOffsets[1] = vk::Offset3D{int32_t(view->width), int32_t(view->height), 1};
            blit.dstSubresource = blit.srcSubresource;
            blit.dstOffsets[1] = vk::Offset3D{int32_t(extent.width), int32_t(extent.height), 1};
            c.blitImage(view->output.Get(), vk::ImageLayout::eTransferSrcOptimal, target,
                        vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);
        });
    presentedThisFrame = true;
}

void Renderer::Impl::EndFrame() {
    if (!inFrame) throw std::logic_error("EndFrame without BeginFrame");
    FrameSlot& f = frames[slotIndex];
    vk::CommandBuffer cmd = f.cmd;

    if (acquired) {
        vk::Image target = swapchain->ImageAt(swapchainImage);
        Vk::ImageState& state = swapchain->StateAt(swapchainImage);
        const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        if (!presentedThisFrame) { // nothing was presented: show black instead of garbage
            graph.Pass(cmd, "ClearSurface",
                [&](RenderGraph::PassBuilder& b) { b.RawImage(target, state, range, Use::TransferDst, true); },
                [&](vk::CommandBuffer c) {
                    c.clearColorImage(target, vk::ImageLayout::eTransferDstOptimal,
                                      vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 1}), range);
                });
        }
        if (overlay) {
            graph.Pass(cmd, "Overlay",
                [&](RenderGraph::PassBuilder& b) { b.RawImage(target, state, range, Use::ColorAttachment); },
                [&](vk::CommandBuffer c) {
                    OverlayContext ctx{c, swapchain->ViewAt(swapchainImage), swapchain->Extent()};
                    overlay(ctx);
                });
        }
        // To present: wait for all writes, no access afterwards (the semaphore covers visibility).
        vk::ImageMemoryBarrier2 toPresent;
        toPresent.srcStageMask = state.writeStages | state.readStages;
        toPresent.srcAccessMask = state.writeAccess;
        // Ordered before the semaphore signal, which happens at ALL_COMMANDS.
        toPresent.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
        toPresent.oldLayout = state.layout;
        toPresent.newLayout = vk::ImageLayout::ePresentSrcKHR;
        toPresent.image = target;
        toPresent.subresourceRange = range;
        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, toPresent));
        state = {};
        state.layout = vk::ImageLayout::ePresentSrcKHR;
    }

    cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, f.timestamps, 1);
    f.timestampsWritten = true;
    cmd.end();

    vk::CommandBufferSubmitInfo cmdInfo(cmd);
    vk::SemaphoreSubmitInfo waitInfo(f.imageAvailable, 0, vk::PipelineStageFlagBits2::eAllCommands);
    vk::SemaphoreSubmitInfo signalInfo(acquired ? swapchain->RenderFinished(swapchainImage) : vk::Semaphore{}, 0,
                                       vk::PipelineStageFlagBits2::eAllCommands);
    vk::SubmitInfo2 submit;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    if (acquired) {
        submit.waitSemaphoreInfoCount = 1;
        submit.pWaitSemaphoreInfos = &waitInfo;
        submit.signalSemaphoreInfoCount = 1;
        submit.pSignalSemaphoreInfos = &signalInfo;
    }
    device->Queue().submit2(submit, f.fence);

    if (acquired && !swapchain->Present(device->Queue(), swapchainImage)) {
        surfaceResizePending = true;
        if (!pendingSurfaceWidth) {
            pendingSurfaceWidth = swapchain->Extent().width;
            pendingSurfaceHeight = swapchain->Extent().height;
        }
    }
    inFrame = false;
    ++frameNumber;
}

// ============================================================================ Renderer (public)

Renderer::Renderer(const RendererDesc& desc) : m_Impl(std::make_unique<Impl>(desc)) {}
Renderer::~Renderer() = default;

MeshHandle Renderer::CreateMesh(const MeshDesc& desc) {
    if (desc.vertices.empty() || desc.indices.empty()) throw std::invalid_argument("CreateMesh: empty mesh");
    auto& impl = *m_Impl;
    auto mesh = std::make_unique<MeshData>();
    const vk::DeviceSize vbytes = desc.vertices.size() * sizeof(Vertex);
    const vk::DeviceSize ibytes = desc.indices.size() * sizeof(uint32_t);
    const auto dst = vk::BufferUsageFlagBits::eTransferDst;
    mesh->vertices = Vk::Buffer(*impl.device, vbytes, vk::BufferUsageFlagBits::eVertexBuffer | dst, Vk::MemoryUsage::GpuOnly, desc.debugName);
    mesh->indices = Vk::Buffer(*impl.device, ibytes, vk::BufferUsageFlagBits::eIndexBuffer | dst, Vk::MemoryUsage::GpuOnly, desc.debugName);
    mesh->indexCount = static_cast<uint32_t>(desc.indices.size());

    glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
    for (const Vertex& v : desc.vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    mesh->boundsCenter = 0.5f * (lo + hi);
    for (const Vertex& v : desc.vertices) mesh->boundsRadius = std::max(mesh->boundsRadius, glm::length(v.position - mesh->boundsCenter));

    Vk::Buffer staging(*impl.device, vbytes + ibytes, vk::BufferUsageFlagBits::eTransferSrc, Vk::MemoryUsage::Upload);
    staging.Write(desc.vertices.data(), vbytes);
    staging.Write(desc.indices.data(), ibytes, vbytes);
    impl.device->ImmediateSubmit([&](vk::CommandBuffer cmd) {
        cmd.copyBuffer(staging.Get(), mesh->vertices.Get(), vk::BufferCopy(0, 0, vbytes));
        cmd.copyBuffer(staging.Get(), mesh->indices.Get(), vk::BufferCopy(vbytes, 0, ibytes));
    });
    return impl.meshes.Create(std::move(mesh));
}

void Renderer::DestroyMesh(MeshHandle mesh) { m_Impl->meshes.Destroy(mesh); }

TextureHandle Renderer::CreateTexture(const TextureDesc& desc) {
    if (desc.width == 0 || desc.height == 0 || desc.pixels.size() < size_t(desc.width) * desc.height * 4)
        throw std::invalid_argument("CreateTexture: pixel data does not match width * height * 4");
    auto& impl = *m_Impl;
    auto tex = std::make_unique<TextureData>();
    Vk::ImageDesc image;
    image.extent = vk::Extent3D{desc.width, desc.height, 1};
    image.format = desc.format == TextureFormat::RGBA8_sRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
    image.mipLevels = desc.generateMips ? Vk::MipCount(desc.width, desc.height) : 1;
    image.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    image.name = desc.debugName;
    tex->image = Vk::Image(*impl.device, image);
    Vk::UploadImage(*impl.device, tex->image, desc.pixels.data(), size_t(desc.width) * desc.height * 4, desc.generateMips);
    tex->slot = impl.bindless->Allocate(tex->image.View(), impl.linearRepeat);
    return impl.textures.Create(std::move(tex));
}

void Renderer::DestroyTexture(TextureHandle texture) {
    auto& impl = *m_Impl;
    if (auto* t = impl.textures.TryGet(texture)) {
        impl.bindless->Free((*t)->slot);
        impl.textures.Destroy(texture);
    }
}

MaterialHandle Renderer::CreateMaterial(const MaterialDesc& desc) { return m_Impl->materials.Create(desc); }
void Renderer::UpdateMaterial(MaterialHandle m, const MaterialDesc& desc) {
    if (auto* mat = m_Impl->materials.TryGet(m)) *mat = desc;
}
void Renderer::DestroyMaterial(MaterialHandle m) {
    if (m != m_Impl->defaultMaterial) m_Impl->materials.Destroy(m);
}
MaterialHandle Renderer::GetDefaultMaterial() const { return m_Impl->defaultMaterial; }

World& Renderer::GetWorld() { return *m_Impl->world; }
const World& Renderer::GetWorld() const { return *m_Impl->world; }

ViewHandle Renderer::CreateView(uint32_t width, uint32_t height) {
    auto view = std::make_unique<ViewData>();
    m_Impl->CreateViewTargets(*view, std::max(1u, width), std::max(1u, height));
    return m_Impl->views.Create(std::move(view));
}

void Renderer::ResizeView(ViewHandle handle, uint32_t width, uint32_t height) {
    auto* v = m_Impl->views.TryGet(handle);
    if (!v) return;
    width = std::max(1u, width);
    height = std::max(1u, height);
    if ((*v)->width == width && (*v)->height == height) return;
    m_Impl->CreateViewTargets(**v, width, height);
}

void Renderer::DestroyView(ViewHandle view) { m_Impl->views.Destroy(view); }

glm::uvec2 Renderer::GetViewSize(ViewHandle handle) const {
    const auto* v = m_Impl->views.TryGet(handle);
    return v ? glm::uvec2((*v)->width, (*v)->height) : glm::uvec2(0);
}

bool Renderer::BeginFrame() { return m_Impl->BeginFrame(); }
void Renderer::RenderView(ViewHandle view, const Camera& camera, const ViewSettings& settings) {
    m_Impl->RenderView(view, camera, settings);
}
void Renderer::PresentView(ViewHandle view) { m_Impl->PresentView(view); }
void Renderer::EndFrame() { m_Impl->EndFrame(); }

void Renderer::ResizeSurface(uint32_t width, uint32_t height) {
    m_Impl->pendingSurfaceWidth = width;
    m_Impl->pendingSurfaceHeight = height;
    m_Impl->surfaceResizePending = true;
}

bool Renderer::HasSurface() const { return static_cast<bool>(m_Impl->swapchain); }

void Renderer::RequestPick(ViewHandle view, uint32_t x, uint32_t y) {
    auto* v = m_Impl->views.TryGet(view);
    if (!v || x >= (*v)->width || y >= (*v)->height) return;
    PendingPick p;
    p.view = view;
    p.x = x;
    p.y = y;
    m_Impl->requestedPick = p; // picked up the next time this view is rendered
}

std::optional<PickResult> Renderer::TakePickResult() {
    auto& results = m_Impl->pickResults;
    if (results.empty()) return std::nullopt;
    PickResult r = results.front();
    results.erase(results.begin());
    return r;
}

std::vector<uint8_t> Renderer::ReadViewPixels(ViewHandle handle) {
    auto* v = m_Impl->views.TryGet(handle);
    if (!v) return {};
    ViewData& view = **v;
    auto& dev = *m_Impl->device;
    dev.Get().waitIdle();
    const vk::DeviceSize bytes = vk::DeviceSize(view.width) * view.height * 4;
    Vk::Buffer readback(dev, bytes, vk::BufferUsageFlagBits::eTransferDst, Vk::MemoryUsage::Readback, "view readback");
    dev.ImmediateSubmit([&](vk::CommandBuffer cmd) {
        Vk::TransitionImage(cmd, view.output, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eAllTransfer,
                            vk::AccessFlagBits2::eTransferRead);
        vk::BufferImageCopy copy;
        copy.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        copy.imageExtent = vk::Extent3D{view.width, view.height, 1};
        cmd.copyImageToBuffer(view.output.Get(), vk::ImageLayout::eTransferSrcOptimal, readback.Get(), copy);
        Vk::TransitionImage(cmd, view.output, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader,
                            vk::AccessFlagBits2::eShaderSampledRead);
    });
    readback.Invalidate();
    std::vector<uint8_t> pixels(bytes);
    std::memcpy(pixels.data(), readback.Mapped(), bytes);
    return pixels;
}

RenderStats Renderer::GetStats() const {
    const auto& impl = *m_Impl;
    RenderStats s;
    s.meshInstances = impl.world->InstanceCount();
    s.sdfShapes = impl.world->SdfShapeCount();
    s.lights = impl.world->LightCount();
    s.sdfBricksUsed = impl.sdfVolume->Used();
    s.sdfBrickCapacity = impl.sdfVolume->Capacity();
    s.sdfBricksDropped = impl.sdfVolume->Dropped();
    s.sdfLastBakeCells = impl.sdfVolume->LastBakeCells();
    s.gpuFrameMs = impl.gpuFrameMs;
    return s;
}

void Renderer::WaitIdle() { m_Impl->device->Get().waitIdle(); }

// ============================================================================ Vulkan interop

VulkanHandles GetVulkanHandles(Renderer& renderer) {
    auto& d = *renderer.GetImpl().device;
    VulkanHandles h;
    h.instance = d.Instance();
    h.physicalDevice = d.Physical();
    h.device = d.Get();
    h.queue = d.Queue();
    h.queueFamily = d.QueueFamily();
    h.getInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    h.apiVersion = VK_API_VERSION_1_3;
    return h;
}

void AttachSurface(Renderer& renderer, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    auto& impl = renderer.GetImpl();
    impl.swapchain = std::make_unique<Vk::Swapchain>(*impl.device, surface, width, height);
    impl.pendingSurfaceWidth = width;
    impl.pendingSurfaceHeight = height;
}

VkFormat GetSurfaceFormat(Renderer& renderer) {
    auto& impl = renderer.GetImpl();
    return impl.swapchain ? static_cast<VkFormat>(impl.swapchain->Format()) : VK_FORMAT_UNDEFINED;
}

uint32_t GetSwapchainImageCount(Renderer& renderer) {
    auto& impl = renderer.GetImpl();
    return impl.swapchain ? impl.swapchain->ImageCount() : 0;
}

void SetOverlayCallback(Renderer& renderer, std::function<void(const OverlayContext&)> callback) {
    renderer.GetImpl().overlay = std::move(callback);
}

VkImageView GetViewImageView(Renderer& renderer, ViewHandle view) {
    auto* v = renderer.GetImpl().views.TryGet(view);
    return v ? static_cast<VkImageView>((*v)->output.View()) : VK_NULL_HANDLE;
}

VkSampler GetLinearSampler(Renderer& renderer) { return renderer.GetImpl().linearClamp; }

} // namespace Astral::Render
