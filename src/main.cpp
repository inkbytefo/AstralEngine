// filepath: AstralEngine/src/main.cpp
// Sandbox: Motoru test etmek icin kullanilan gecici uygulama.
// Asil oyun projesine gecildiginde bu dosya silinecek ve kendi
// oyununuzun main'i motora (AstralCore) baglanacaktir.

#include "Astral/Core/Application.hpp"

int main() {
    Astral::Application app;
    app.Run();
    return 0;
}