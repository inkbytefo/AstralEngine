#include "AstralEngine.h"
#include <iostream>

/// Minimal bos oyun uygulamasi — Astral::Application sinifindan turer.
/// Yeni bir oyun gelistirirken tum oyun mantigi ISubsystem siniflari
/// ve PushSystem<T>() araciligiyla buraya eklenir.
class EmptyGameApp : public Astral::Application {
public:
    EmptyGameApp()
        : Astral::Application() {
        std::cout << "[EmptyGameApp] Minimal Oyun Sablonu basariyla baslatildi.\n";
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
