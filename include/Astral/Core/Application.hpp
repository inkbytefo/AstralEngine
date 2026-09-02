// filepath: AstralEngine/include/Astral/Core/Application.hpp
#pragma once

#include <string>

namespace Astral {

/// Uygulamanin temel yasam dongusunu temsil eden cekirdek sinif.
/// Ileride Renderer, InputManager, SceneManager gibi alt-sistemler
/// bu sinif uzerinden yonetilecek.
class Application {
public:
    Application();
    ~Application();

    /// Ana donguyu baslatir. Pencere/olay dongusu burada yer alacak.
    void Run();

    /// Motorun adi ve versiyonu icin basit bir sorgulama.
    static std::string GetName();
    static std::string GetVersion();
};

} // namespace Astral