#include "AstralEngine.h"
#include <iostream>

static Astral::AppConfig ParseCommandLine(const Astral::CommandLineArgs& args) {
    Astral::AppConfig config;
    for (int i = 1; i < args.argc; ++i) {
        if (std::string(args.argv[i]) == "--frames" && i + 1 < args.argc) {
            config.maxFrames = std::stoi(args.argv[++i]);
        }
    }
    return config;
}

/// Minimal bos oyun uygulamasi — Astral::Application sinifindan turer.
/// Yeni bir oyun gelistirirken tum oyun mantigi ISubsystem siniflari
/// ve PushSystem<T>() araciligiyla buraya eklenir.
class EmptyGameApp : public Astral::Application {
public:
    EmptyGameApp()
        : Astral::Application(ParseCommandLine(Astral::GetCommandLineArgs())) {
        std::cout << "[EmptyGameApp] Minimal Oyun Sablonu basariyla baslatildi.\n";
    }
protected:
    // No geometry or camera is inserted by the engine. Add your scene here.
    std::shared_ptr<Astral::Scene> CreateInitialScene() override {
        return std::make_shared<Astral::Scene>("Empty Game");
    }
};

namespace Astral {

/// Motor giris noktasi (EntryPoint) tarafindan cagirilan zorunlu sozlesme fonksiyonu.
Application* CreateApplication() {
    return new EmptyGameApp();
}

} // namespace Astral

// Motor ana dongusunu (main) bu translation unit'e dahil eder.
#include "Astral/EntryPoint.hpp"
