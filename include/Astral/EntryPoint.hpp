#pragma once

#include "Astral/Core/Application.hpp"
#include <memory>
#include <iostream>
#include <exception>

// ============================================================================
// KRITIK MIMARI UYARI / GIRIS NOKTASI SOZLESMESI:
// Bu baslik dosyasi (EntryPoint.hpp), oyun veya istemci projesinin yalnizca ve
// yalnizca BIR (1) adet kaynak dosyasinda (genellikle main.cpp veya projenin ana
// giris dosyasinda) include edilmelidir!
//
// Eger birden fazla .cpp dosyasinda include edilirse, derleyici/baglayici (linker)
// 'main' sembolu icin "multiple definition of main" (MSVC'de LNK2005 / LNK1169)
// hatasi uretecektir. Bu beklenen, bilerek tasarlanmis ve motor sozlesmesini
// garanti altina alan mimari bir kisitlamadir.
// ============================================================================

int main(int argc, char** argv) {
    Astral::SetCommandLineArgs(argc, argv);

    try {
        std::unique_ptr<Astral::Application> app(Astral::CreateApplication());
        if (!app) {
            std::cerr << "[Astral::EntryPoint Kritik Hata]: Astral::CreateApplication() nullptr dondurdu!\n";
            return 1;
        }

        app->Run();
    } catch (const std::exception& e) {
        std::cerr << "[Astral::EntryPoint Kritik Hata]: Yakalanmamis istisna: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[Astral::EntryPoint Kritik Hata]: Bilinmeyen kritik istisna yakalandi!\n";
        return 1;
    }

    return 0;
}
