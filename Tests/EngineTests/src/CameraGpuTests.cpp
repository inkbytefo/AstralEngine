#include "DemoScene.hpp"
#include "AstralEngine.h"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
enum class Scenario { Demo, Empty, Removed, Moved, Wide };
class LifecycleProbe final : public Astral::ISubsystem {
public:
    explicit LifecycleProbe(std::shared_ptr<bool> initialized) : m_Initialized(std::move(initialized)) {}
    void OnInit() override { *m_Initialized = true; }
    void OnUpdate(Astral::FrameContext&) override {}
    void OnShutdown() override { *m_Initialized = false; }
private:
    std::shared_ptr<bool> m_Initialized;
};

class CaptureApp final : public Astral::Application {
public:
    explicit CaptureApp(const Astral::AppConfig& config, Scenario scenario)
        : Application(config), m_Scenario(scenario) {}
protected:
    std::shared_ptr<Astral::Scene> CreateInitialScene() override {
        if (m_Scenario == Scenario::Empty) return Application::CreateInitialScene();
        auto scene = m_Demo.Create(false);
        for (auto&& [entity, camera] : scene->GetRegistry().GetView<Astral::CameraComponent>()) {
            if (m_Scenario == Scenario::Wide) camera.verticalFovRadians = glm::radians(75.0f);
            if (m_Scenario == Scenario::Moved) {
                scene->GetRegistry().GetComponent<Astral::TransformComponent>(entity).position.x += 2.0f;
            }
        }
        return scene;
    }
    void OnInitialize() override {
        if (!GetActiveScene() || !GetRenderer()) throw std::runtime_error("Initialization order");
        PushSystem<LifecycleProbe>(m_Initialized);
    }
    void OnUpdate(Astral::FrameContext& context, uint32_t frame) override {
        if (!*m_Initialized) throw std::runtime_error("Client system was not initialized before update");
        if (m_Scenario != Scenario::Empty) m_Demo.Update(*GetActiveScene(), false, frame);
        if (m_Scenario == Scenario::Removed && frame == 1 &&
            !Astral::SetActiveCamera(context.registry, Astral::NullEntityHandle))
            throw std::runtime_error("Cannot clear active camera");
        if (frame == 3) GetRenderer()->SetPickingRequest(160, 90);
        if (GetLastPickResult().hasHit) SetHighlightEntity(GetLastPickResult().hitEntity);
    }
private:
    Sandbox::DemoScene m_Demo;
    Scenario m_Scenario;
    std::shared_ptr<bool> m_Initialized = std::make_shared<bool>(false);
};

std::vector<uint8_t> Capture(bool deferred, const std::filesystem::path& output,
                             Scenario scenario = Scenario::Demo) {
    Astral::AppConfig config;
    config.width = 320;
    config.height = 180;
    config.maxFrames = 5;
    config.fixedTimeStep = 0.016f;
    config.fixedDeltaTime = 0.016f;
    config.useGBuffer = deferred;
    CaptureApp app(config, scenario);
    app.Run();
    if (app.GetTotalFramesRendered() != 5) throw std::runtime_error("Incomplete frame loop");
    if (scenario == Scenario::Empty && app.GetActiveScene()->GetRegistry().GetAliveEntityCount() != 0)
        throw std::runtime_error("Default scene contains implicit entities");
    auto& context = *app.GetVulkanContext();
    const auto* renderer = app.GetRenderer();
    const auto bytes = static_cast<vk::DeviceSize>(config.width * config.height * 4);
    Astral::Buffer readback(context.GetDevice(), context.GetPhysicalDevice(), bytes,
        vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = vk::ImageLayout::eGeneral;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = renderer->GetStorageImage();
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
        vk::BufferImageCopy region{};
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent = vk::Extent3D(config.width, config.height, 1);
        cmd.copyImageToBuffer(renderer->GetStorageImage(), vk::ImageLayout::eTransferSrcOptimal,
            readback.GetBuffer(), region);
    });
    std::ofstream stream(output, std::ios::binary);
    stream.write(static_cast<const char*>(readback.GetMappedData()), static_cast<std::streamsize>(bytes));
    if (!stream) throw std::runtime_error("Cannot write GPU capture");
    const auto* begin = static_cast<const uint8_t*>(readback.GetMappedData());
    return {begin, begin + bytes};
}

void RequireBlack(const std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i+1] || pixels[i+2] || pixels[i+3] != 255)
            throw std::runtime_error("Camera-less output is not opaque black");
}

void Compare(const std::vector<uint8_t>& pixels, const std::filesystem::path& reference) {
    std::ifstream stream(reference, std::ios::binary);
    std::vector<uint8_t> baseline((std::istreambuf_iterator<char>(stream)), {});
    if (baseline.size() != pixels.size()) throw std::runtime_error("Invalid baseline size");
    double error = 0;
    size_t significant = 0;
    int maximum = 0;
    for (size_t i = 0; i < pixels.size(); ++i) {
        const int delta = std::abs(int(pixels[i]) - int(baseline[i]));
        error += delta;
        maximum = std::max(maximum, delta);
        significant += delta > 8;
    }
    error /= static_cast<double>(pixels.size());
    std::cout << "REGRESSION " << reference.filename() << " MAE=" << error
              << " max=" << maximum << " channels>8=" << significant << '\n';
    if (error > 0.5 || significant > pixels.size() / 100)
        throw std::runtime_error("Sandbox image regression exceeds tolerance");
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3) throw std::runtime_error("Usage: CameraGpuTests <output-directory> [baseline-directory]");
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        for (const bool deferred : {false, true}) {
            const std::string name = deferred ? "deferred" : "forward";
            const auto demo = Capture(deferred, output / (name + ".rgba"));
            if (argc == 3) Compare(demo, std::filesystem::path(argv[2]) / (name + ".rgba"));
            RequireBlack(Capture(deferred, output / (name + "-empty.rgba"), Scenario::Empty));
            RequireBlack(Capture(deferred, output / (name + "-removed.rgba"), Scenario::Removed));
            if (demo == Capture(deferred, output / (name + "-moved.rgba"), Scenario::Moved))
                throw std::runtime_error("Moving camera did not affect output");
            if (demo == Capture(deferred, output / (name + "-wide.rgba"), Scenario::Wide))
                throw std::runtime_error("Changing FOV did not affect output");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
