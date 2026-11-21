// Per-view frame: world upload, SDF bake, shadow map, G-buffer (meshes then SDFs),
// deferred lighting, TAA, tonemapping and picking. Every pass goes through the render
// graph, which inserts the barriers.

#include "Detail/RendererImpl.hpp"
#include "Astral/Render/SdfMath.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>

namespace Astral::Render {

using Detail::kInvalidIndex;

namespace {

constexpr vk::Format kAlbedoFormat = vk::Format::eR8G8B8A8Unorm;
constexpr vk::Format kHalfFormat = vk::Format::eR16G16B16A16Sfloat;

// Reversed Z (1 at the near plane, 0 at the far plane) and Vulkan's downward Y.
glm::mat4 ReverseDepth(const glm::mat4& proj) {
    glm::mat4 flip(1.0f);
    flip[2][2] = -1.0f;
    flip[3][2] = 1.0f;
    return flip * proj;
}

float Halton(uint32_t index, uint32_t base) {
    float f = 1.0f, r = 0.0f;
    while (index > 0) {
        f /= float(base);
        r += f * float(index % base);
        index /= base;
    }
    return r;
}

struct Frustum {
    glm::vec4 planes[6];
    explicit Frustum(const glm::mat4& m) {
        glm::mat4 t = glm::transpose(m);
        planes[0] = t[3] + t[0];
        planes[1] = t[3] - t[0];
        planes[2] = t[3] + t[1];
        planes[3] = t[3] - t[1];
        planes[4] = t[2];         // reversed-Z far plane (z >= 0)
        planes[5] = t[3] - t[2];  // near plane (z <= w)
        for (auto& p : planes) p /= glm::length(glm::vec3(p));
    }
    bool Visible(const glm::vec3& c, float r) const {
        for (const auto& p : planes)
            if (glm::dot(glm::vec3(p), c) + p.w < -r) return false;
        return true;
    }
};

Vk::Image MakeTarget(Vk::Device& device, uint32_t w, uint32_t h, vk::Format format, vk::ImageUsageFlags usage, const char* name) {
    Vk::ImageDesc d;
    d.extent = vk::Extent3D{w, h, 1};
    d.format = format;
    d.usage = usage;
    d.name = name;
    return Vk::Image(device, d);
}

} // namespace

void Renderer::Impl::CreateViewTargets(ViewData& v, uint32_t w, uint32_t h) {
    using U = vk::ImageUsageFlagBits;
    const auto gbuffer = U::eColorAttachment | U::eStorage | U::eSampled;
    v.width = w;
    v.height = h;
    v.albedoMetal = MakeTarget(*device, w, h, kAlbedoFormat, gbuffer, "gbuffer albedo/metal");
    v.normalRough = MakeTarget(*device, w, h, kHalfFormat, gbuffer, "gbuffer normal/rough");
    v.emissive = MakeTarget(*device, w, h, kHalfFormat, gbuffer, "gbuffer emissive");
    v.motion = MakeTarget(*device, w, h, vk::Format::eR32G32Sfloat, gbuffer, "gbuffer motion");
    v.objectId = MakeTarget(*device, w, h, vk::Format::eR32Uint, gbuffer | U::eTransferSrc, "gbuffer object id");
    v.sceneDepth = MakeTarget(*device, w, h, vk::Format::eR32Sfloat, gbuffer | U::eTransferSrc, "scene depth");
    v.depth = MakeTarget(*device, w, h, vk::Format::eD32Sfloat, U::eDepthStencilAttachment | U::eSampled, "mesh depth");
    v.hdr = MakeTarget(*device, w, h, kHalfFormat, U::eStorage | U::eSampled, "hdr");
    for (auto& hist : v.history) hist = MakeTarget(*device, w, h, kHalfFormat, U::eStorage | U::eSampled, "taa history");
    v.output = MakeTarget(*device, w, h, kAlbedoFormat, U::eStorage | U::eSampled | U::eTransferSrc, "view output");
    v.historyValid = false;
}

// ---------------------------------------------------------------------------- world upload

void Renderer::Impl::UploadWorld(vk::CommandBuffer cmd) {
    FrameSlot& f = frames[slotIndex];
    World::Impl& w = world->GetImpl();

    // Materials: indexed by handle slot.
    const uint32_t materialSlots = std::max(1u, materials.SlotCount());
    EnsureCapacity(f.materials, sizeof(Detail::GpuMaterial) * materialSlots, vk::BufferUsageFlagBits::eStorageBuffer, "materials");
    auto* gpuMaterials = static_cast<Detail::GpuMaterial*>(f.materials.Mapped());
    auto slotOf = [&](TextureHandle t) {
        const auto* tex = textures.TryGet(t);
        return tex ? (*tex)->slot : kInvalidIndex;
    };
    materials.ForEach([&](MaterialHandle h, const MaterialDesc& m) {
        Detail::GpuMaterial g{};
        g.baseColor = m.baseColor;
        g.emissiveNormalScale = glm::vec4(m.emissive, m.normalScale);
        g.metallic = m.metallic;
        g.roughness = m.roughness;
        g.baseColorTexture = slotOf(m.baseColorTexture);
        g.normalTexture = slotOf(m.normalTexture);
        g.metallicRoughnessTexture = slotOf(m.metallicRoughnessTexture);
        g.emissiveTexture = slotOf(m.emissiveTexture);
        gpuMaterials[h.index] = g;
    });

    // Instances
    draws.clear();
    std::vector<Detail::GpuInstance> instances;
    w.instances.ForEach([&](InstanceHandle, InstanceData& inst) {
        if (!inst.visible || !meshes.Contains(inst.mesh)) return;
        Detail::GpuInstance g{};
        g.model = inst.transform;
        g.prevModel = inst.hasHistory ? inst.lastUploaded : inst.transform;
        g.normalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(inst.transform)));
        g.materialIndex = MaterialGpuIndex(inst.material);
        g.objectId = inst.objectId;
        inst.lastUploaded = inst.transform;
        inst.hasHistory = true;
        draws.push_back({inst.mesh, static_cast<uint32_t>(instances.size()), inst.castsShadows});
        instances.push_back(g);
    });
    EnsureCapacity(f.instances, sizeof(Detail::GpuInstance) * std::max<size_t>(1, instances.size()),
                   vk::BufferUsageFlagBits::eStorageBuffer, "instances");
    if (!instances.empty()) f.instances.Write(instances.data(), instances.size() * sizeof(Detail::GpuInstance));

    // Lights: the first directional light is the sun.
    sun.reset();
    std::vector<Detail::GpuPointLight> points;
    w.lights.ForEach([&](LightHandle, const LightDesc& l) {
        if (l.type == LightType::Directional) {
            if (!sun) sun = l;
        } else if (points.size() < kMaxPointLights) {
            points.push_back({glm::vec4(l.position, l.range), glm::vec4(l.color, l.intensity)});
        }
    });
    pointLightCount = static_cast<uint32_t>(points.size());
    EnsureCapacity(f.lights, sizeof(Detail::GpuPointLight) * std::max<size_t>(1, points.size()),
                   vk::BufferUsageFlagBits::eStorageBuffer, "lights");
    if (!points.empty()) f.lights.Write(points.data(), points.size() * sizeof(Detail::GpuPointLight));

    // SDF shapes, in evaluation order.
    const auto& sorted = w.SortedShapes();
    std::vector<Detail::GpuSdfShape> shapes;
    shapes.reserve(std::min<size_t>(sorted.size(), SdfVolume::kMaxShapes));
    for (const SdfShapeDesc& s : sorted) {
        if (shapes.size() >= SdfVolume::kMaxShapes) break;
        glm::quat q = glm::normalize(s.rotation);
        Detail::GpuSdfShape g{};
        g.positionBlend = glm::vec4(s.position, std::max(s.blend, 0.0f));
        g.rotation = glm::vec4(q.x, q.y, q.z, q.w);
        g.size = glm::max(s.size, glm::vec4(0.001f, 0.001f, 0.001f, 0.0f));
        g.albedoRoughness = glm::vec4(s.material.albedo, std::clamp(s.material.roughness, 0.03f, 1.0f));
        g.emissiveMetallic = glm::vec4(s.material.emissive, std::clamp(s.material.metallic, 0.0f, 1.0f));
        g.primitive = std::min(static_cast<uint32_t>(s.primitive), kSdfPrimitiveCount - 1);
        g.operation = std::min(static_cast<uint32_t>(s.operation), kSdfOperationCount - 1);
        g.objectId = s.objectId;
        SdfShapeDesc clamped = s;
        clamped.size = g.size;
        g.boundRadius = Sdf::BoundRadius(clamped);
        shapes.push_back(g);
    }
    sdfVolume->SetShapes(shapes);
    EnsureCapacity(f.sdfShapes, sizeof(Detail::GpuSdfShape) * std::max<size_t>(1, shapes.size()),
                   vk::BufferUsageFlagBits::eStorageBuffer, "sdf shapes");
    if (!shapes.empty()) f.sdfShapes.Write(shapes.data(), shapes.size() * sizeof(Detail::GpuSdfShape));
    sdfVolume->RecordBake(graph, cmd, slotIndex, f.sdfShapes.Get());

    worldUploaded = true;
}

// ---------------------------------------------------------------------------- environment

void Renderer::Impl::BakeEnvironment(vk::CommandBuffer cmd, vk::DeviceSize frameDataOffset) {
    FrameSlot& f = frames[slotIndex];
    struct Push { uint32_t mode, size; float roughness; uint32_t pad; };
    graph.Pass(cmd, "EnvironmentBake",
        [&](RenderGraph::PassBuilder& b) {
            b.Image(irradiance, Use::StorageWrite, true).Image(prefiltered, Use::StorageWrite, true).Image(brdfLut, Use::StorageWrite, true);
        },
        [&](vk::CommandBuffer c) {
            environmentBake.Bind(c);
            auto bind = [&](vk::ImageView cubeView) {
                Vk::Bindings()
                    .UniformBuffer(0, f.frameData.Get(), frameDataOffset, sizeof(Detail::FrameData))
                    .StorageImage(1, cubeView)
                    .StorageImage(2, brdfLut.View())
                    .Push(c, environmentBake);
            };
            bind(irradiance.LayerArrayView());
            environmentBake.Push(c, Push{0, kIrradianceSize, 0.0f, 0});
            c.dispatch(Vk::DivUp(kIrradianceSize, 8), Vk::DivUp(kIrradianceSize, 8), 6);
            for (uint32_t mip = 0; mip < kPrefilterMips; ++mip) {
                uint32_t size = std::max(1u, kPrefilterSize >> mip);
                bind(prefiltered.MipView(mip));
                environmentBake.Push(c, Push{1, size, float(mip) / float(kPrefilterMips - 1), 0});
                c.dispatch(Vk::DivUp(size, 8), Vk::DivUp(size, 8), 6);
            }
            bind(irradiance.LayerArrayView());
            environmentBake.Push(c, Push{2, kBrdfLutSize, 0.0f, 0});
            c.dispatch(Vk::DivUp(kBrdfLutSize, 8), Vk::DivUp(kBrdfLutSize, 8), 1);
        });
    bakedEnvironmentVersion = world->GetImpl().environmentVersion;
}

// ---------------------------------------------------------------------------- view

void Renderer::Impl::RenderView(ViewHandle handle, const Camera& camera, const ViewSettings& settings) {
    if (!inFrame) throw std::logic_error("RenderView outside BeginFrame/EndFrame");
    auto* viewPtr = views.TryGet(handle);
    if (!viewPtr) return;
    if (viewsThisFrame >= kMaxViewsPerFrame) throw std::runtime_error("too many views rendered in one frame");
    ViewData& v = **viewPtr;
    FrameSlot& f = frames[slotIndex];
    vk::CommandBuffer cmd = f.cmd;

    if (!worldUploaded) UploadWorld(cmd);
    const EnvironmentDesc& env = world->GetEnvironment();
    const bool debug = settings.debugView != DebugView::None;
    const bool useTaa = settings.taa && !debug;

    // ---- camera matrices ----
    const float aspect = float(v.width) / float(v.height);
    glm::mat4 proj = ReverseDepth(glm::perspectiveRH_ZO(camera.verticalFov, aspect, camera.nearPlane, camera.farPlane));
    proj[1][1] *= -1.0f;
    glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.0f), camera.position) * glm::mat4_cast(glm::normalize(camera.rotation)));
    glm::mat4 viewProjNoJitter = proj * view;
    glm::vec2 jitter(0.0f);
    if (useTaa) {
        uint32_t i = static_cast<uint32_t>(v.frameCount % 8) + 1;
        jitter = glm::vec2(Halton(i, 2) - 0.5f, Halton(i, 3) - 0.5f) * glm::vec2(2.0f / v.width, 2.0f / v.height);
    }
    glm::mat4 jittered = proj;
    jittered[2][0] += jitter.x;
    jittered[2][1] += jitter.y;

    // ---- sun shadow matrix: a box around the near part of the view, snapped to texels ----
    const bool shadowsOn = settings.shadows && sun && sun->castShadows && !draws.empty();
    glm::mat4 shadowViewProj(1.0f);
    float shadowTexel = 0.0f;
    glm::vec3 toSun = sun ? -glm::normalize(sun->direction) : glm::vec3(0, 1, 0);
    if (sun) {
        const float radius = settings.shadowDistance * 0.5f;
        glm::vec3 forward = glm::normalize(camera.rotation) * glm::vec3(0, 0, -1);
        glm::vec3 center = camera.position + forward * radius;
        glm::vec3 up = std::abs(toSun.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
        glm::mat4 lightView = glm::lookAtRH(glm::vec3(0.0f), -toSun, up);
        shadowTexel = 2.0f * radius / float(kShadowMapSize);
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(center, 1.0f));
        ls.x = std::floor(ls.x / shadowTexel) * shadowTexel;
        ls.y = std::floor(ls.y / shadowTexel) * shadowTexel;
        glm::mat4 lightProj = ReverseDepth(glm::orthoRH_ZO(ls.x - radius, ls.x + radius, ls.y - radius, ls.y + radius,
                                                           -ls.z - radius * 4.0f, -ls.z + radius * 4.0f));
        shadowViewProj = lightProj * lightView;
    }

    // ---- frame constants ----
    Detail::FrameData fd{};
    fd.view = view;
    fd.proj = jittered;
    fd.viewProj = jittered * view;
    fd.invViewProj = glm::inverse(fd.viewProj);
    fd.viewProjNoJitter = viewProjNoJitter;
    fd.prevViewProjNoJitter = v.hasPrevMatrix ? v.prevViewProjNoJitter : viewProjNoJitter;
    fd.shadowViewProj = shadowViewProj;
    fd.cameraPos = glm::vec4(camera.position, float(frameNumber) / 60.0f);
    fd.viewport = glm::vec4(float(v.width), float(v.height), 1.0f / v.width, 1.0f / v.height);
    fd.jitter = glm::vec4(jitter, 0.0f, 0.0f);
    fd.sunDirection = glm::vec4(toSun, sun ? sun->intensity : 0.0f);
    fd.sunColor = glm::vec4(sun ? sun->color : glm::vec3(0.0f), sun ? 1.0f : 0.0f);
    fd.shadowParams = glm::vec4(shadowTexel, shadowsOn ? 1.0f : 0.0f, settings.sdfShadows ? 1.0f : 0.0f,
                                settings.sdfAmbientOcclusion ? 1.0f : 0.0f);
    fd.envParams = glm::vec4(env.ambientIntensity, env.exposure, env.skyIntensity, float(kPrefilterMips));
    fd.zenithColor = glm::vec4(env.zenithColor, 0.0f);
    fd.horizonColor = glm::vec4(env.horizonColor, 0.0f);
    fd.groundColor = glm::vec4(env.groundColor, 0.0f);
    fd.counts = glm::uvec4(pointLightCount, sdfVolume->ShapeCount(), static_cast<uint32_t>(settings.debugView),
                           static_cast<uint32_t>(v.frameCount));
    fd.clip = glm::vec4(camera.nearPlane, camera.farPlane, 0.0f, 0.0f);
    const vk::DeviceSize fdOffset = vk::DeviceSize(viewsThisFrame) * kFrameDataStride;
    f.frameData.Write(&fd, sizeof(fd), fdOffset);
    ++viewsThisFrame;

    auto frameUbo = [&](Vk::Bindings& b, uint32_t binding) -> Vk::Bindings& {
        return b.UniformBuffer(binding, f.frameData.Get(), fdOffset, sizeof(Detail::FrameData));
    };

    if (bakedEnvironmentVersion != world->GetImpl().environmentVersion) BakeEnvironment(cmd, fdOffset);

    auto drawMeshes = [&](vk::CommandBuffer c, const Vk::Pipeline& pipeline, bool shadowCasters, const Frustum* cull) {
        for (const DrawItem& d : draws) {
            if (shadowCasters && !d.castsShadows) continue;
            const MeshData& mesh = *meshes.Get(d.mesh);
            if (cull) {
                const auto* inst = static_cast<const Detail::GpuInstance*>(f.instances.Mapped()) + d.instanceIndex;
                glm::vec3 c0 = glm::vec3(inst->model * glm::vec4(mesh.boundsCenter, 1.0f));
                float scale = std::max(glm::length(glm::vec3(inst->model[0])),
                                       std::max(glm::length(glm::vec3(inst->model[1])), glm::length(glm::vec3(inst->model[2]))));
                if (!cull->Visible(c0, mesh.boundsRadius * scale)) continue;
            }
            vk::DeviceSize offset = 0;
            c.bindVertexBuffers(0, mesh.vertices.Get(), offset);
            c.bindIndexBuffer(mesh.indices.Get(), 0, vk::IndexType::eUint32);
            pipeline.Push(c, d.instanceIndex);
            c.drawIndexed(mesh.indexCount, 1, 0, 0, 0);
        }
    };

    // ---- shadow map (mesh casters) ----
    if (shadowsOn) {
        graph.Pass(cmd, "ShadowMap",
            [&](RenderGraph::PassBuilder& b) { b.Image(shadowMap, Use::DepthAttachment, true); },
            [&](vk::CommandBuffer c) {
                vk::RenderingAttachmentInfo depth(shadowMap.View(), vk::ImageLayout::eDepthAttachmentOptimal);
                depth.loadOp = vk::AttachmentLoadOp::eClear;
                depth.storeOp = vk::AttachmentStoreOp::eStore;
                depth.clearValue.depthStencil = vk::ClearDepthStencilValue(0.0f, 0);
                vk::RenderingInfo info({}, vk::Rect2D({0, 0}, {kShadowMapSize, kShadowMapSize}), 1, 0, {}, &depth);
                c.beginRendering(info);
                c.setViewport(0, vk::Viewport(0, 0, float(kShadowMapSize), float(kShadowMapSize), 0, 1));
                c.setScissor(0, vk::Rect2D({0, 0}, {kShadowMapSize, kShadowMapSize}));
                shadowDepth.Bind(c);
                Vk::Bindings b;
                frameUbo(b, 0).StorageBuffer(1, f.instances.Get()).Push(c, shadowDepth);
                drawMeshes(c, shadowDepth, true, nullptr);
                c.endRendering();
            });
    }

    // ---- G-buffer: meshes ----
    const vk::Extent2D extent(v.width, v.height);
    Frustum frustum(viewProjNoJitter);
    graph.Pass(cmd, "GBuffer.Meshes",
        [&](RenderGraph::PassBuilder& b) {
            for (Vk::Image* img : {&v.albedoMetal, &v.normalRough, &v.emissive, &v.motion, &v.objectId, &v.sceneDepth})
                b.Image(*img, Use::ColorAttachment, true);
            b.Image(v.depth, Use::DepthAttachment, true);
        },
        [&](vk::CommandBuffer c) {
            std::array<vk::RenderingAttachmentInfo, 6> colors;
            Vk::Image* targets[6] = {&v.albedoMetal, &v.normalRough, &v.emissive, &v.motion, &v.objectId, &v.sceneDepth};
            for (int i = 0; i < 6; ++i) {
                colors[i] = vk::RenderingAttachmentInfo(targets[i]->View(), vk::ImageLayout::eColorAttachmentOptimal);
                colors[i].loadOp = vk::AttachmentLoadOp::eClear;
                colors[i].storeOp = vk::AttachmentStoreOp::eStore;
                colors[i].clearValue.color = i == 4 ? vk::ClearColorValue(std::array<uint32_t, 4>{0, 0, 0, 0})
                                                    : vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 0});
            }
            vk::RenderingAttachmentInfo depth(v.depth.View(), vk::ImageLayout::eDepthAttachmentOptimal);
            depth.loadOp = vk::AttachmentLoadOp::eClear;
            depth.storeOp = vk::AttachmentStoreOp::eStore;
            depth.clearValue.depthStencil = vk::ClearDepthStencilValue(0.0f, 0);
            vk::RenderingInfo info({}, vk::Rect2D({0, 0}, extent), 1, 0, colors, &depth);
            c.beginRendering(info);
            if (!draws.empty()) {
                c.setViewport(0, vk::Viewport(0, 0, float(v.width), float(v.height), 0, 1));
                c.setScissor(0, vk::Rect2D({0, 0}, extent));
                meshGBuffer.Bind(c);
                Vk::Bindings b;
                frameUbo(b, 0).StorageBuffer(1, f.instances.Get()).StorageBuffer(2, f.materials.Get()).Push(c, meshGBuffer);
                c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, meshGBuffer.Layout(), 1, bindless->Set(), {});
                drawMeshes(c, meshGBuffer, false, &frustum);
            }
            c.endRendering();
        });

    const uint32_t groupsX = Vk::DivUp(v.width, 8), groupsY = Vk::DivUp(v.height, 8);
    const bool sdfDebug = settings.debugView == DebugView::SdfBricks || settings.debugView == DebugView::SdfSteps;

    // ---- G-buffer: SDF shapes (depth-tested against the meshes) ----
    if (sdfVolume->ShapeCount() > 0 || sdfDebug) {
        graph.Pass(cmd, "GBuffer.Sdf",
            [&](RenderGraph::PassBuilder& b) {
                for (Vk::Image* img : {&v.albedoMetal, &v.normalRough, &v.emissive, &v.motion, &v.objectId, &v.sceneDepth})
                    b.Image(*img, Use::StorageReadWrite);
                b.Image(sdfVolume->Map(), Use::StorageRead).Image(sdfVolume->Atlas(), Use::Sampled);
            },
            [&](vk::CommandBuffer c) {
                sdfGBuffer.Bind(c);
                Vk::Bindings b;
                frameUbo(b, 0)
                    .StorageBuffer(1, f.sdfShapes.Get())
                    .UniformBuffer(2, sdfVolume->ParamsBuffer(slotIndex))
                    .StorageImage(3, sdfVolume->Map().View())
                    .SampledImage(4, sdfVolume->Atlas().View(), linearClamp)
                    .StorageImage(5, v.albedoMetal.View())
                    .StorageImage(6, v.normalRough.View())
                    .StorageImage(7, v.emissive.View())
                    .StorageImage(8, v.motion.View())
                    .StorageImage(9, v.objectId.View())
                    .StorageImage(10, v.sceneDepth.View())
                    .Push(c, sdfGBuffer);
                c.dispatch(groupsX, groupsY, 1);
            });
    }

    // ---- deferred lighting ----
    graph.Pass(cmd, "Lighting",
        [&](RenderGraph::PassBuilder& b) {
            for (Vk::Image* img : {&v.albedoMetal, &v.normalRough, &v.emissive, &v.objectId, &v.sceneDepth, &v.motion})
                b.Image(*img, Use::StorageRead);
            b.Image(shadowMap, Use::Sampled).Image(irradiance, Use::Sampled).Image(prefiltered, Use::Sampled).Image(brdfLut, Use::Sampled);
            b.Image(sdfVolume->Map(), Use::StorageRead).Image(sdfVolume->Atlas(), Use::Sampled);
            b.Image(v.hdr, Use::StorageWrite, true);
        },
        [&](vk::CommandBuffer c) {
            lighting.Bind(c);
            Vk::Bindings b;
            frameUbo(b, 0)
                .StorageImage(1, v.albedoMetal.View())
                .StorageImage(2, v.normalRough.View())
                .StorageImage(3, v.emissive.View())
                .StorageImage(4, v.objectId.View())
                .StorageImage(5, v.sceneDepth.View())
                .StorageImage(6, v.motion.View())
                .SampledImage(7, shadowMap.View(), nearestClamp, vk::ImageLayout::eDepthReadOnlyOptimal)
                .SampledImage(8, irradiance.View(), linearClamp)
                .SampledImage(9, prefiltered.View(), linearClamp)
                .SampledImage(10, brdfLut.View(), linearClamp)
                .StorageBuffer(11, f.lights.Get())
                .StorageImage(12, v.hdr.View())
                .StorageBuffer(13, f.sdfShapes.Get())
                .UniformBuffer(14, sdfVolume->ParamsBuffer(slotIndex))
                .StorageImage(15, sdfVolume->Map().View())
                .SampledImage(16, sdfVolume->Atlas().View(), linearClamp)
                .Push(c, lighting);
            c.dispatch(groupsX, groupsY, 1);
        });

    // ---- temporal anti-aliasing ----
    Vk::Image* resolved = &v.hdr;
    if (useTaa) {
        Vk::Image& prev = v.history[v.historyIndex];
        Vk::Image& next = v.history[1 - v.historyIndex];
        graph.Pass(cmd, "Taa",
            [&](RenderGraph::PassBuilder& b) {
                b.Image(v.hdr, Use::StorageRead).Image(prev, Use::Sampled).Image(v.motion, Use::StorageRead).Image(next, Use::StorageWrite, true);
            },
            [&](vk::CommandBuffer c) {
                taa.Bind(c);
                Vk::Bindings b;
                frameUbo(b, 0)
                    .StorageImage(1, v.hdr.View())
                    .SampledImage(2, prev.View(), linearClamp)
                    .StorageImage(3, v.motion.View())
                    .StorageImage(4, next.View())
                    .Push(c, taa);
                taa.Push(c, uint32_t(v.historyValid ? 1 : 0));
                c.dispatch(groupsX, groupsY, 1);
            });
        v.historyIndex = 1 - v.historyIndex;
        v.historyValid = true;
        resolved = &next;
    } else {
        v.historyValid = false;
    }

    // ---- tonemap into the view's output ----
    graph.Pass(cmd, "Tonemap",
        [&](RenderGraph::PassBuilder& b) { b.Image(*resolved, Use::Sampled).Image(v.output, Use::StorageWrite, true); },
        [&](vk::CommandBuffer c) {
            tonemap.Bind(c);
            Vk::Bindings b;
            frameUbo(b, 0).SampledImage(1, resolved->View(), nearestClamp).StorageImage(2, v.output.View()).Push(c, tonemap);
            c.dispatch(groupsX, groupsY, 1);
        });

    // ---- picking: copy the id and depth under the cursor ----
    if (requestedPick && requestedPick->view == handle && !f.pick) {
        PendingPick pick = *requestedPick;
        requestedPick.reset();
        pick.invViewProj = fd.invViewProj;
        pick.width = v.width;
        pick.height = v.height;
        graph.Pass(cmd, "Pick",
            [&](RenderGraph::PassBuilder& b) {
                b.Image(v.objectId, Use::TransferSrc).Image(v.sceneDepth, Use::TransferSrc).Buffer(f.pickSync, Use::TransferDst);
            },
            [&](vk::CommandBuffer c) {
                vk::BufferImageCopy copy;
                copy.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
                copy.imageOffset = vk::Offset3D{int32_t(pick.x), int32_t(pick.y), 0};
                copy.imageExtent = vk::Extent3D{1, 1, 1};
                c.copyImageToBuffer(v.objectId.Get(), vk::ImageLayout::eTransferSrcOptimal, f.pickReadback.Get(), copy);
                copy.bufferOffset = 4;
                c.copyImageToBuffer(v.sceneDepth.Get(), vk::ImageLayout::eTransferSrcOptimal, f.pickReadback.Get(), copy);
            });
        graph.Pass(cmd, "PickToHost", [&](RenderGraph::PassBuilder& b) { b.Buffer(f.pickSync, Use::HostRead); },
                   [](vk::CommandBuffer) {});
        f.pickSync = {};
        f.pick = pick;
    }

    // Leave the output readable by UI code (the editor samples it).
    graph.Pass(cmd, "ViewOutput", [&](RenderGraph::PassBuilder& b) { b.Image(v.output, Use::Sampled); },
               [](vk::CommandBuffer) {});

    v.prevViewProjNoJitter = viewProjNoJitter;
    v.hasPrevMatrix = true;
    ++v.frameCount;
}

} // namespace Astral::Render
