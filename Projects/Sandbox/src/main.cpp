#include "AstralEngine.h"
#include <iostream>
#include <string>

static Astral::AppConfig ParseCommandLine(const Astral::CommandLineArgs& args) {
    Astral::AppConfig config;
    int maxFrames = -1;

    for (int i = 1; i < args.argc; ++i) {
        std::string arg = args.argv[i];
        if (arg == "--bench") {
            config.benchMode = true;
        } else if (arg == "--bench-frames" && i + 1 < args.argc) {
            config.benchFrames = std::stoi(args.argv[++i]);
            config.benchMode = true;
        } else if (arg == "--bench-out" && i + 1 < args.argc) {
            config.benchOutputFile = args.argv[++i];
            config.benchMode = true;
        } else if (arg == "--frames" && i + 1 < args.argc) {
            maxFrames = std::stoi(args.argv[++i]);
        } else if (arg == "--width" && i + 1 < args.argc) {
            config.width = std::stoi(args.argv[++i]);
        } else if (arg == "--height" && i + 1 < args.argc) {
            config.height = std::stoi(args.argv[++i]);
        } else if (arg == "--normal" && i + 1 < args.argc) {
            std::string mode = args.argv[++i];
            if (mode == "tetra" || mode == "tetrahedron" || mode == "1") {
                config.normalMode = 1;
            } else {
                config.normalMode = 0;
            }
        } else if (arg == "--shader" && i + 1 < args.argc) {
            config.shaderPath = args.argv[++i];
        } else if (arg == "--legacy-map") {
            config.legacyMap = true;
        } else if (arg == "--grid") {
            config.useGrid = true;
        } else if (arg == "--no-grid") {
            config.useGrid = false;
        } else if (arg == "--stress") {
            config.stressTest = true;
        } else if (arg == "--opt-shadow") {
            config.optShadow = true;
        } else if (arg == "--no-opt-shadow") {
            config.optShadow = false;
        } else if (arg == "--taa") {
            config.enableTAA = true;
        } else if (arg == "--no-taa") {
            config.enableTAA = false;
        }
    }

    config.maxFrames = maxFrames;
    return config;
}

/// Sandbox uygulamasi — AstralEngine'in ilk somut istemcisi (client).
/// Application sinifindan turer ve saf bir runtime/oyun simulasyonu sunar.
class SandboxApp : public Astral::Application {
public:
    SandboxApp()
        : Astral::Application(ParseCommandLine(Astral::GetCommandLineArgs())) {
        std::cout << "[SandboxApp] Sandbox Calisma Zamani Uygulamasi baslatildi.\n";
    }
};

namespace Astral {

/// Motorun giris noktasi (EntryPoint) tarafindan cagirilan istemci sozlesme fonksiyonu.
Application* CreateApplication() {
    return new SandboxApp();
}

} // namespace Astral

// Giris noktasi sozlesmesi: main() fonksiyonunu bu translation unit'e dahil eder.
#include "Astral/EntryPoint.hpp"
