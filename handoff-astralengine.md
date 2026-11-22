# AstralEngine devir notu

22 Kasım 2026 · C++20 ve Vulkan 1.3 oyun motoru · sürüm `0.17.0`

## Yeni sohbetin başlangıcı

Kullanıcının son kararı **M13 — Gelişmiş PBR** ile devam etmek. [Yol haritasındaki M13 kapsamını](docs/roadmap.md#yol-haritası) ve render, shader, materyal belgelerini okuyup planla. Kapsam clearcoat, sheen, anizotropi, transmission, subsurface, ıslaklık, decal ve path tracer referansını içeriyor. M13.5’in kalan R3 ve R4 işleri beklemede. R3’ü ayrı kullanıcı onayıyla başlat.

Tek uzun ömürlü dal `master`. Repo `https://github.com/inkbytefo/AstralEngine.git`, yerel dizin `C:\Dev\Astral`. Başlamadan `git status`, `git log` ve `git worktree list` ile güncel durumu doğrula. Önce [CLAUDE.md](CLAUDE.md), [mimari](docs/architecture.md) ve [geliştirme süreci](docs/process.md). Oyun ve örnek kodu için [AGENTS.md](AGENTS.md) geçerli.

## Tamamlanan işler

- M12 fizik alanları, eklemler, tetik ve temas olayları ile `physics_playground` örneği tamamlandı. Ayrıntılar [ADR-0029](docs/decisions/0029-physics-fields-and-joints.md) ve [CHANGELOG](CHANGELOG.md) içinde.
- M13.5 R0, R1 ve R2 işleri M13’ten önce uygulandı ve master’a alındı.

| İş | Master’daki commit | Sonuç |
|---|---|---|
| R0 | `4f80c56`, `a9bc7a3`, `7ff546a`, `c9532b2` | MSVC Debug CI, doctest yükseltmesi, LF/CRLF kapsamı, boyut ve katman denetimleri, yazılım ve donanım golden ayrımı, hafif `ImageData` başlığı |
| R1a / R1b | `7b6f8b8`, `c0b5de3` | Renderer kaynak depoları, kare hazırlığı, pass’ler ve raster/RT yolları ayrıldı. `renderer.cpp` 150 satır, render boyut istisnası kalmadı. |
| R2a / R2b | `7b68ef4`, `f7fb9a8` | App servisleri ve FrameLoop ayrıldı. Context servisleri gruplandı, debug panelleri ayrıldı. Eski metotlar M14’e kadar deprecated sarmalayıcı olarak, mevcut pointer alanları kendi tipleriyle duruyor. |

Yeni Context çağrıları `scenes()`, `maps()`, `materials()`, `audio_service()`, `script_service()` ve `physics_service()`. Örnekler için [oyun yazma rehberi](docs/guides/writing-a-game.md) ve [App belgesi](docs/modules/app.md).

## Doğrulama ve bekleyenler

- R2b’nin son Windows kapısı MSVC Debug `/WX`, 52/52 görüntü dışı test, 12/12 donanım golden ve yerel referansla 12/12 bayt eşleşmesi verdi. Strict boyut denetiminde 0 istisnasız ihlal ve 14 mevcut istisna vardı. İlk Pong denemesi Luau’nun 100 ms sınırına takıldı. Aynı commit ile seri tekrar geçti.
- RTX 3060 üzerinde `golden-hw` çalışır, 12 yazılım `golden` testi beklenen şekilde atlanır. Lavapipe koşusunda yazılım etiketi çalışır. Ayrıntılar [ADR-0031](docs/decisions/0031-hardware-independent-golden-tests.md).
- Uzak GitHub Actions işleri son doğrulamada hesap faturalandırma kilidi nedeniyle başlamadı. Linux lavapipe ve uzak CI doğrulaması bekliyor. [R2b CI koşusu](https://github.com/inkbytefo/AstralEngine/actions/runs/45068643255)
- Yerel Conductor kayıtları ve golden taban çizgisi `.conductor/` altında. Üretilen Graphify çıktısı `graphify-out/` altında. Bunlar yerel çalışma çıktılarıdır.
- R3 assets için shader cook teşhisleri ve çıktı baytları taşımadan önce testle sabitlenmeli. R4 derleme performansı ve strict boyut denetiminin CI’da açılması M13.5 kapanışında bekliyor.

Kullanıcıyla kısa ve doğrudan Türkçe iletişim kur. Kod ve kod yorumları İngilizce, `docs/` Türkçe. Motor davranışı için önce test ekle veya mevcut davranışı karakterize et. Ayrı worktree kullan, doğrulanan işleri master’a alıp push et.
