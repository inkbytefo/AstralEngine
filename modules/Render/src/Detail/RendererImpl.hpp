#pragma once

#include "Astral/Render/Renderer.hpp"
#include "Astral/Render/VulkanInterop.hpp"
#include "Detail/GpuTypes.hpp"
#include "Detail/WorldImpl.hpp"
#include "Graph/RenderGraph.hpp"
#include "SdfVolume.hpp"
#include "Vk/Device.hpp"
#include "Vk/Pipelines.hpp"
#include "Vk/Resources.hpp"
#include "Vk/Swapchain.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace Astral::Render {

struct MeshData {
    Vk::Buffer vertices;
    Vk::Buffer indices;
    uint32_t indexCount = 0;
    glm::vec3 boundsCenter{0.0f};
    float boundsRadius = 0.0f;
};

struct TextureData {
    Vk::Image image;
    uint32_t slot = Detail::kInvalidIndex;
};

// Render targets and temporal state of one view.
struct ViewData {
    uint32_t width = 0, height = 0;
    Vk::Image albedoMetal;   // RGBA8
    Vk::Image normalRough;   // RGBA16F
    Vk::Image emissive;      // RGBA16F
    Vk::Image motion;        // RG32F
    Vk::Image objectId;      // R32_UINT
    Vk::Image sceneDepth;    // R32F, depth of the closest surface (mesh or SDF)
    Vk::Image depth;         // D32, mesh depth buffer
    Vk::Image hdr;           // RGBA16F lit colour
    std::array<Vk::Image, 2> history; // TAA ping-pong
    Vk::Image output;        // RGBA8 display-encoded result
    uint32_t historyIndex = 0;
    bool historyValid = false;
    glm::mat4 prevViewProjNoJitter{1.0f};
    bool hasPrevMatrix = false;
    uint64_t frameCount = 0;
};

struct PendingPick {
    ViewHandle view;
    uint32_t x = 0, y = 0;
    glm::mat4 invViewProj{1.0f};
    uint32_t width = 1, height = 1;
};

struct FrameSlot {
    vk::CommandBuffer cmd;
    vk::Fence fence;
    vk::Semaphore imageAvailable;
    vk::QueryPool timestamps;
    bool timestampsWritten = false;
    uint64_t frameNumber = 0;

    Vk::Buffer frameData;    // one FrameData per view rendered this frame
    Vk::Buffer instances;
    Vk::Buffer materials;
    Vk::Buffer lights;
    Vk::Buffer sdfShapes;

    Vk::Buffer pickReadback; // objectId + depth of the picked pixel
    BufferState pickSync;
    std::optional<PendingPick> pick;
};

struct DrawItem {
    MeshHandle mesh;
    uint32_t instanceIndex;
    bool castsShadows;
};

struct Renderer::Impl {
    static constexpr uint32_t kFrames = 2;
    static constexpr uint32_t kMaxViewsPerFrame = 8;
    static constexpr uint32_t kFrameDataStride = 1024;
    static constexpr uint32_t kShadowMapSize = 2048;
    static constexpr uint32_t kIrradianceSize = 32;
    static constexpr uint32_t kPrefilterSize = 128;
    static constexpr uint32_t kPrefilterMips = 6;
    static constexpr uint32_t kBrdfLutSize = 128;
    static constexpr uint32_t kMaxPointLights = 1024;

    explicit Impl(const RendererDesc& desc);
    ~Impl();

    // ---- frame ----
    bool BeginFrame();
    void RenderView(ViewHandle view, const Camera& camera, const ViewSettings& settings);
    void PresentView(ViewHandle view);
    void EndFrame();

    void UploadWorld(vk::CommandBuffer cmd);
    void BakeEnvironment(vk::CommandBuffer cmd, vk::DeviceSize frameDataOffset);
    void CreateViewTargets(ViewData& view, uint32_t width, uint32_t height);
    void EnsureCapacity(Vk::Buffer& buffer, vk::DeviceSize bytes, vk::BufferUsageFlags usage, const char* name);
    uint32_t MaterialGpuIndex(MaterialHandle handle) const;

    RendererDesc desc;
    std::unique_ptr<Vk::Device> device;
    std::unique_ptr<Vk::BindlessTextures> bindless;
    std::unique_ptr<Vk::Swapchain> swapchain;
    std::unique_ptr<World> world;
    std::unique_ptr<SdfVolume> sdfVolume;
    RenderGraph graph{false};

    HandlePool<MeshTag, std::unique_ptr<MeshData>> meshes;
    HandlePool<TextureTag, std::unique_ptr<TextureData>> textures;
    HandlePool<MaterialTag, MaterialDesc> materials;
    HandlePool<ViewTag, std::unique_ptr<ViewData>> views;
    MaterialHandle defaultMaterial;

    vk::CommandPool commandPool;
    std::array<FrameSlot, kFrames> frames;
    uint64_t frameNumber = 0;       // frames begun so far
    uint32_t slotIndex = 0;
    bool inFrame = false;
    bool worldUploaded = false;
    uint32_t viewsThisFrame = 0;
    float gpuFrameMs = 0.0f;

    // swapchain state for the current frame
    bool acquired = false;
    uint32_t swapchainImage = 0;
    bool presentedThisFrame = false;
    uint32_t pendingSurfaceWidth = 0, pendingSurfaceHeight = 0;
    bool surfaceResizePending = false;
    std::function<void(const OverlayContext&)> overlay;

    // per-frame world data
    std::vector<DrawItem> draws;
    uint32_t pointLightCount = 0;
    std::optional<LightDesc> sun;

    // pipelines
    Vk::Pipeline meshGBuffer, shadowDepth, sdfGBuffer, lighting, environmentBake, taa, tonemap;
    vk::Sampler linearClamp, linearRepeat, nearestClamp;

    // shared images
    Vk::Image shadowMap;
    Vk::Image irradiance, prefiltered, brdfLut;
    uint64_t bakedEnvironmentVersion = 0;
    bool brdfLutBaked = false;

    std::optional<PendingPick> requestedPick;
    std::vector<PickResult> pickResults;
};

} // namespace Astral::Render
