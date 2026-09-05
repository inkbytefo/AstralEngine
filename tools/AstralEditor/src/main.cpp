#include "AstralEngine.h"
#include "EditorUISubsystem.hpp"
#include <string>

namespace {

static std::string s_ProjectArg = "";

Astral::AppConfig ParseCommandLine(const Astral::CommandLineArgs& args) {
    Astral::AppConfig config;
    int maxFrames = -1;

    for (int i = 1; i < args.argc; ++i) {
        std::string arg = args.argv[i];
        if (arg == "--project" && i + 1 < args.argc) {
            s_ProjectArg = args.argv[++i];
        } else if (arg.size() >= 11 && arg.compare(arg.size() - 11, 11, ".astralproj") == 0) {
            s_ProjectArg = arg;
        } else if (arg == "--frames" && i + 1 < args.argc) {
            maxFrames = std::stoi(args.argv[++i]);
        } else if (arg == "--width" && i + 1 < args.argc) {
            config.width = std::stoi(args.argv[++i]);
        } else if (arg == "--height" && i + 1 < args.argc) {
            config.height = std::stoi(args.argv[++i]);
        } else if (arg == "--normal" && i + 1 < args.argc) {
            std::string mode = args.argv[++i];
            config.normalMode = (mode == "tetra" || mode == "tetrahedron" || mode == "1") ? 1 : 0;
        } else if (arg == "--shader" && i + 1 < args.argc) {
            config.shaderPath = args.argv[++i];
        } else if (arg == "--legacy-map") {
            config.legacyMap = true;
        } else if (arg == "--grid") {
            config.useGrid = true;
        } else if (arg == "--no-grid") {
            config.useGrid = false;

        } else if (arg == "--opt-shadow") {
            config.optShadow = true;
        } else if (arg == "--no-opt-shadow") {
            config.optShadow = false;
        } else if (arg == "--taa") {
            config.enableTAA = true;
        } else if (arg == "--no-taa") {
            config.enableTAA = false;
        } else if (arg == "--gbuffer") {
            config.useGBuffer = true;
        } else if (arg == "--no-gbuffer") {
            config.useGBuffer = false;
        } else if (arg == "--debug-mode" && i + 1 < args.argc) {
            config.debugMode = std::stoi(args.argv[++i]);
        }
    }

    config.maxFrames = maxFrames;
    config.simulatePhysics = false;
    return config;
}

} // anonymous namespace

class AstralEditorApp : public Astral::Application {
public:
    AstralEditorApp()
        : Astral::Application(ParseCommandLine(Astral::GetCommandLineArgs())) {
        
        // Aktif projeyi yukle veya varsayilana baglan
        if (!s_ProjectArg.empty() && std::filesystem::exists(s_ProjectArg)) {
            Astral::Project::LoadProject(s_ProjectArg);
        } else if (std::filesystem::exists("Projects/Sandbox/Sandbox.astralproj")) {
            Astral::Project::LoadProject("Projects/Sandbox/Sandbox.astralproj");
        } else if (std::filesystem::exists("../Projects/Sandbox/Sandbox.astralproj")) {
            Astral::Project::LoadProject("../Projects/Sandbox/Sandbox.astralproj");
        } else {
            // Varsayılan proje hazırla
            Astral::Project::NewProject(".", "DefaultProject");
        }

        // AstralEditor'un temel arayuz ve panel sistemini kaydet

    }
protected:
    std::shared_ptr<Astral::Scene> CreateInitialScene() override {
        auto scene = std::make_shared<Astral::Scene>("Untitled Scene");
        if (auto project = Astral::Project::GetActive()) {
            const auto path = project->GetProjectDirectory() / project->GetConfig().startScene;
            if (std::filesystem::exists(path) && !Astral::SceneSerializer::Deserialize(scene, path)) {
                throw std::runtime_error("Cannot load initial project scene: " + path.string());
            }
        }
        // Editor-owned navigation camera for projects without a runtime camera.
        if (!Astral::ExtractActiveCamera(scene->GetRegistry(), 1.0f)) {
            auto camera = scene->CreateEntity();
            camera.AddComponent<Astral::TransformComponent>(glm::vec3(0.0f, 1.5f, 4.0f),
                glm::quatLookAt(glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f)), glm::vec3(0, 1, 0)));
            camera.AddComponent<Astral::CameraComponent>(2.0f * std::atan(0.5f / 1.5f), 0.01f, 50.0f, 1u);
            camera.AddComponent<Astral::TagComponent>("Editor Camera");
        }
        return scene;
    }
    void OnInitialize() override {
        PushSystem<Astral::EditorUISubsystem>(*this);
    }
};

namespace Astral {

Application* CreateApplication() {
    return new AstralEditorApp();
}

} // namespace Astral

// Giris noktasi sozlesmesi
#include "Astral/EntryPoint.hpp"
