# ADR-0019 — Geliştirici araçları ve 2D katman: profiler, debug çizim, ImGui, HUD

- **Durum:** Kabul (uygulandı: M8.1–M8.4)
- **Tarih:** 2026-11-21
- **Bağlam:** yol haritası M8, ADR-0009 (render graph), ADR-0012 (render API), ADR-0013 (reflection)

## Sorun

Sonraki fazlar (materyal, atmosfer, su, VFX) panelsiz ayarlanamaz, ölçülmeden optimize edilemez. Pong
skoru pencere başlığına yazmak zorunda kaldı: ekrana metin çizilemiyordu.

## Karar

1. **Profiler global değil.** `Profiler` bir nesne; App onu `ctx.profiler` olarak verir.
   `ASTRAL_PROFILE_SCOPE(ctx.profiler, "isim")` iç içe kapsamları kaydeder. Testler kendi profiler'ını
   kurar; iki App aynı süreçte birbirini bozmaz.
2. **GPU zamanı render graph'tan gelir.** Her pass'in başına/sonuna timestamp yazılır (4 karelik halka,
   bekletmeyen okuma, `hostQueryReset`). Oyun kodu GPU sorgusu görmez; `Renderer::gpu_timings()` pass adı
   ve milisaniye döndürür.
3. **Debug çizim veri olarak akar.** `DebugDraw` yalnızca çizgi listesi biriktirir (`RenderScene::debug_lines`).
   Derinlik testli ve "her şeyin üstünde" iki pipeline; süreli çizgiler (`seconds`). Fizik ve ışık
   şekilleri App'te bu API ile çizilir — renderer fiziği tanımaz.
4. **Tek 2D katman.** ImGui, HUD ve ileride sprite'lar aynı biçimi üretir: `UiLayer` (vertex + index +
   komutlar: texture, kırpma, SDF yumuşaklığı). Renderer tonemap'ten sonra tek bir `ui` pass'iyle hepsini
   çizer. Yeni bir 2D tüketici renderer'a dokunmadan eklenir (Open/Closed).
5. **Dear ImGui, kendi arka ucumuzla.** Resmî Vulkan arka ucu RHI'mizi atlar; onun yerine ImGui 1.92'nin
   texture protokolü (`RendererHasTextures`) kendi texture'larımızla karşılanır ve çizim listesi `UiLayer`'a
   çevrilir. Inspector elle yazılmaz: reflect edilmiş her component JSON üzerinden düzenlenir
   (`component_to_json` / `component_from_json`).
6. **Metin SDF font atlası ile.** stb_truetype ile çalışma anında SDF atlası (Türkçe karakterler dahil);
   her ölçekte keskin, gölge ve yumuşaklık shader'da. Varsayılan font gömülü (Roboto).

## Sonuçlar

- Her yeni faz "ölç, gör, ayarla" döngüsüyle başlar: F1 paneli, profiler tabloları, debug çizim.
- Reflection'a eklenen her component editör panelinde kendiliğinden görünür; aynı kural ileride script
  bağlamasında da geçerli (ADR-0017).
- MSDF yerine SDF: çok büyük punto köşeleri hafif yuvarlanır; gerekirse atlas üreticisi değişir, API değişmez.
