#include "AstralEngine.h"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "IBLReferenceChecks.hpp"

namespace {
using namespace Astral;
using Pixels = std::vector<unsigned char>;

Pixels Readback(VulkanContext& context, const SDFRenderer& renderer) {
    const size_t bytes = size_t(renderer.GetWidth()) * renderer.GetHeight() * 4;
    Buffer buffer(context.GetDevice(), context.GetPhysicalDevice(), bytes,
                  vk::BufferUsageFlagBits::eTransferDst,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = vk::ImageLayout::eGeneral;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = renderer.GetStorageImage();
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                            {}, {}, {}, barrier);
        vk::BufferImageCopy region{};
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent = vk::Extent3D(renderer.GetWidth(), renderer.GetHeight(), 1);
        cmd.copyImageToBuffer(renderer.GetStorageImage(), vk::ImageLayout::eTransferSrcOptimal,
                              buffer.GetBuffer(), region);
        std::swap(barrier.oldLayout, barrier.newLayout);
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands,
                            {}, {}, {}, barrier);
    });
    const auto* data = static_cast<const unsigned char*>(buffer.GetMappedData());
    return {data, data + bytes};
}

void Save(const std::filesystem::path& path, const Pixels& pixels, int width, int height) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i + 3] != 255) throw std::runtime_error("Non-opaque output");
        file.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    }
    if (!file) throw std::runtime_error("Cannot write capture");
}

void Compare(const Pixels& actual, const Pixels& reference, const std::string& label, double tolerance) {
    if (actual.size() != reference.size()) throw std::runtime_error("Image extent mismatch");
    double error = 0;
    size_t outliers = 0;
    for (size_t i = 0; i < actual.size(); ++i) {
        const int delta = std::abs(int(actual[i]) - int(reference[i]));
        error += delta;
        outliers += delta > 8;
    }
    error /= actual.size();
    const double fraction = double(outliers) / actual.size();
    std::cout << label << " MAE=" << error << " fraction>8=" << fraction << '\n';
    if (error > tolerance || fraction > 0.01) throw std::runtime_error(label + " exceeds tolerance");
}

std::vector<SDFEditGPU> Fixture() {
    std::vector<SDFEditGPU> edits(4);
    edits[0].primitiveType = 3;
    edits[0].position = {0, -1, 0};
    edits[0].scale = {1, 0, 1};
    edits[0].albedo = {0.2f, 0.2f, 0.2f};
    edits[1].position = {-0.8f, 0, 0};
    edits[1].scale = {0.5f, 0.7f, 0.3f};
    edits[1].metallic = 1;
    edits[1].roughness = 0.08f;
    edits[1].albedo = {0.8f, 0.65f, 0.2f};
    edits[2].primitiveType = 1;
    edits[2].position = {0.7f, 0, 0};
    edits[2].scale = {0.025f, 0.8f, 0.25f};
    edits[2].albedo = {0.1f, 0.7f, 0.3f};
    edits[3].primitiveType = 1;
    edits[3].position = {0, 0, 1};
    edits[3].scale = {0.35f, 0.45f, 0.2f};
    edits[3].albedo = {0.7f, 0.1f, 0.05f};
    for (auto& e : edits) {
        e.operation = 0;
        e.SetPrevPosition(e.position);
        e.prevRotation = e.rotation;
        e.prevScale = glm::vec4(e.scale, 0);
    }
    return edits;
}
class FrameCaptureHelper {
public:
    FrameCaptureHelper(VulkanContext& context, SDFRenderer& renderer)
        : m_Context(context), m_Renderer(renderer) {}

    Pixels Capture(RenderCamera& camera, uint32_t frame, bool grid, bool taa) {
        camera.view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        m_Renderer.SetCamera(camera, taa ? glm::vec2((frame % 2) * 0.5f - 0.25f, (frame % 3) / 3.0f - 0.333333f) : glm::vec2(0));
        m_Context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
            m_Renderer.Render(cmd, frame / 60.0f, 0, m_Renderer.GetWidth(), m_Renderer.GetHeight(), grid, true, taa, frame);
        });
        return Readback(m_Context, m_Renderer);
    }

private:
    VulkanContext& m_Context;
    SDFRenderer& m_Renderer;
};

void WriteManifest(const std::filesystem::path& output, const vk::PhysicalDeviceProperties& props) {
    std::ofstream manifest(output / "manifest.json");
    manifest << "{\n"
             << "  \"gpu\": \"" << props.deviceName.data() << "\",\n"
             << "  \"driverVersion\": " << props.driverVersion << ",\n"
             << "  \"apiVersion\": \"" << VK_VERSION_MAJOR(props.apiVersion) << "." << VK_VERSION_MINOR(props.apiVersion) << "." << VK_VERSION_PATCH(props.apiVersion) << "\",\n"
             << "  \"resolution\": {\"width\": 320, \"height\": 180, \"resizeWidth\": 384},\n"
             << "  \"scenarios\": [\"pan\", \"thin\", \"metal\", \"fast\", \"disocclusion\", \"resize\"],\n"
             << "  \"frameCountPerScenario\": 32,\n"
             << "  \"jitter\": \"((frame % 2) * 0.5 - 0.25, (frame % 3) / 3.0 - 0.333333)\",\n"
             << "  \"hdrFixture\": \"constant_radiance.hdr\",\n"
             << "  \"exposure\": [0, 1]\n"
             << "}\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: VisualQualityGpuTests <output-directory>");
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        AppConfig config;
        config.width = 320; config.height = 180; config.maxFrames = 1;
        class Harness final : public Application {
        public:
            explicit Harness(const AppConfig& config) : Application(config) {}
        } app(config);
        app.Run();
        auto& renderer = *app.GetRenderer();
        auto& context = *app.GetVulkanContext();
        WriteManifest(output, context.GetPhysicalDevice().getProperties());
        const auto environment = Astral::Test::CheckIBLReferences(context, output);
        renderer.LoadEnvironment(environment);
        RenderCamera camera;
        camera.position = {0, 1, 4};
        camera.forward = glm::normalize(glm::vec3(0, -0.15f, -1));
        camera.right = {1, 0, 0}; camera.up = glm::cross(camera.right, camera.forward);
        camera.projection = glm::perspective(glm::radians(50.0f), 320.0f / 180, 0.01f, 50.0f);
        FrameCaptureHelper captureHelper(context, renderer);
        auto draw = [&](uint32_t frame, bool grid, bool taa) {
            return captureHelper.Capture(camera, frame, grid, taa);
        };
        for (bool deferred : {false, true}) {
            renderer.SetUseGBuffer(deferred);
            const std::string prefix = deferred ? "deferred" : "forward";
            auto edits = Fixture();
            renderer.UpdateEdits(edits);
            const auto plain = draw(0, false, false);
            const auto grid = draw(0, true, false);
            Compare(grid, plain, prefix + " grid", 0.5);
            Save(output / (prefix + "_reference.ppm"), plain, 320, 180);
            for (int variant = 0; variant < 6; ++variant) {
                auto boundary = Fixture();
                boundary[1].primitiveType = static_cast<uint32_t>(variant);
                boundary[1].scale = {0.5f, 0.2f, 0.7f};
                const auto rotation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1, 0, 1)));
                boundary[1].rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
                boundary[2].operation = variant % 2 ? 4u : 3u;
                boundary[2].blendFactor = variant * 0.4f;
                boundary[3].position.x = 13; // outside the grid must use exact evaluation
                camera.position.x = variant == 5 ? 13.0f : 0.0f;
                renderer.UpdateEdits(boundary);
                const auto exact = draw(0, false, false);
                Compare(draw(0, true, false), exact, prefix + " SDF " + std::to_string(variant), 0.5);
            }
            camera.position.x = 0;
            renderer.UpdateEdits(edits);
            renderer.SetExposure(0);
            const auto black = draw(0, true, false);
            for (size_t i = 0; i < black.size(); ++i)
                if (i % 4 != 3 && black[i] != 0) throw std::runtime_error("Zero exposure is not black");
            renderer.SetExposure(1);
            for (const std::string scenario : {"pan", "thin", "metal", "fast", "disocclusion", "resize"}) {
                const auto folder = output / (prefix + "_" + scenario);
                std::filesystem::create_directories(folder);
                renderer.ResetTemporalHistory();
                edits = Fixture();
                camera.position = {0, 1, 4};
                for (uint32_t frame = 0; frame < 32; ++frame) {
                    for (auto& e : edits) {
                        e.SetPrevPosition(e.position); e.prevRotation = e.rotation; e.prevScale = glm::vec4(e.scale, 0);
                    }
                    if (scenario == "pan" || scenario == "thin") camera.position.x = 0.02f * frame;
                    if (scenario == "metal") edits[1].roughness = 0.04f + frame * 0.025f;
                    if (scenario == "fast") {
                        edits[3].position.x = std::sin(frame * 0.35f);
                        const auto q = glm::angleAxis(frame * 0.2f, glm::vec3(0, 1, 0));
                        edits[3].rotation = {q.x, q.y, q.z, q.w};
                    }
                    if (scenario == "disocclusion" && frame == 16) edits[3].position.x = 3;
                    if (scenario == "resize" && (frame == 12 || frame == 24)) {
                        const int width = frame == 12 ? 384 : 320;
                        renderer.Resize(width, 180);
                        camera.projection = glm::perspective(glm::radians(50.0f), width / 180.0f, 0.01f, 50.0f);
                    }
                    renderer.UpdateEdits(edits);
                    auto pixels = draw(frame, true, true);
                    std::ostringstream name; name << std::setw(3) << std::setfill('0') << frame << ".ppm";
                    Save(folder / name.str(), pixels, renderer.GetWidth(), renderer.GetHeight());
                }
            }
            // An explicit camera cut must exactly reproduce a fresh first frame, including jitter.
            renderer.ResetTemporalHistory();
            const auto cut = draw(30, true, true);
            renderer.ResetTemporalHistory();
            Compare(draw(0, true, true), cut, prefix + " camera cut", 0.0);
        }
        std::cout << "Visual quality GPU checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
