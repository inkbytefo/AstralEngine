# Astral::Platform

Katman 1 (Gpu'nun üstünde). Pencere ve input; içeride SDL3 ([ADR-0007](../decisions/0007-sdl3-platform.md)),
public API'de SDL tipi yok.

```cpp
auto window = astral::Window::create({.title = "Oyunum", .width = 1920, .height = 1080});
astral::Input input;
while (window->poll(input)) {             // olayları işler, input'u günceller
    if (input.pressed(astral::Key::Escape)) window->request_close();
    astral::Vec2 move = input.wasd();     // x: sağ/sol, y: ileri/geri
    if (input.down(astral::MouseButton::Left)) { /* saldırı */ }
}
```

## Input

- `down(key)`: basılı mı · `pressed(key)`: bu karede basıldı · `released(key)`: bu karede bırakıldı.
- Fare: `mouse_position()`, `mouse_delta()` (kare başına toplam), `scroll()`.
- Adlandırılmış eylemler (`move`, `fire`) App'tedir: [app.md → Oyun çatısı](app.md#oyun-çatısı-m11); gamepad eşlemesi de orada.
- `Input` saf veridir (SDL bilmez) → oyun mantığı pencere açmadan test edilebilir.
- Gamepad (ilk bağlı olan): `input.gamepad_connected()`, `down/pressed/released(GamepadButton)`, `axis(GamepadAxis)` (çubuklar -1..1, yukarı/sağ pozitif; tetikler 0..1). `Window::poll` SDL gamepad olaylarını işler; takılıp çıkarılabilir.
- FPS kamerası için `window->set_mouse_captured(true)`.

## Yollar

`executable_directory()`: çalışan exe'nin klasörü (SDL_GetBasePath). Paketlenmiş oyunun `.apak`'ı
burada aranır.

## GPU'ya bağlanma

`window->surface_source()` bir `gpu::SurfaceSource` döner; `gpu::Device` ve `gpu::Swapchain` bunu
alır. Platform GPU'yu bilir, GPU platformu bilmez.

## Test

Input testleri saf; pencere testleri SDL'in `dummy` sürücüsüyle ekran olmadan çalışır. Gerçek
pencere + present testi `tests/integration` altında, CI'da Xvfb ile.

## Bilinen sınırlar

- Gamepad, metin girişi (IME), dosya diyalogları, çoklu pencere henüz yok.
- Pencere olayları (resize, focus) callback olarak verilmiyor; swapchain boyutu kendisi takip ediyor.

## Metin girişi ve yollar (M8)

- `Window::set_text_input(bool)`, `Input::text()` (UTF-8, kare başına), `clear_keyboard` / `clear_mouse`
  (debug UI girdiyi yuttuğunda), `Window::mouse_captured()`, `pixel_density()`.
- `executable_directory()`: paketli oyun `.apak`'ı exe'nin yanında arar.
