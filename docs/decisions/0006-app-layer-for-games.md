# ADR-0006: Oyunlar App + Game + System üzerinden yazılır

**Durum:** Kabul · 2026-11-21 · ADR-0001'i tamamlar

**Bağlam:** ADR-0001 motoru bir kütüphane yaptı (oyun motoru çağırır). Ama oyun geliştirirken
her oyunun pencere, GPU, fizik adımı ve render sırasını kendisinin kurması gereksiz yük ve hata kaynağı.

**Karar:** İki katman:
1. **Kütüphane katmanı** (core, platform, gpu, render, world, physics…): bağımsız araçlar; oyun
   isterse doğrudan kullanır (kendi döngüsü, sunucu, testler).
2. **`Astral::App` katmanı:** main loop'un sahibi. Oyun `astral::Game`'den türer
   (`on_start`, isteğe bağlı `on_update`), mantığını **component** (düz struct) + **system**
   (her kare çalışan sınıf) olarak yazar. Kare sırası: input → oyun system'leri → fizik →
   animasyon → render.

```cpp
int main() {
    astral::App app({.title = "Sword Arena"});
    app.run<ArenaGame>();
}
```

**Sonuçlar:** Oyun kodu küçük ve motor detaylarından bağımsız; system'ler headless test edilir;
esneklik kaybolmaz (App opsiyonel). `World` (ECS) ve `App`, render graph'tan önceye alındı.
