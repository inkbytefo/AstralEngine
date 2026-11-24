# AstralEngine Dokümantasyonu

Bu klasör motorun **güncel** durumunu anlatır. Her faz (M0, M1, …) bittiğinde güncellenir —
kurallar [process.md](process.md)'de.

| Doküman | İçerik |
|---|---|
| [architecture.md](architecture.md) | Katmanlar, SOLID kuralları, bağımlılık yönü, klasör yapısı |
| [roadmap.md](roadmap.md) | Fazlar, her fazın durumu ve bitiş kriterleri |
| [status/rendering-deep-audit-2026-11-25.md](status/rendering-deep-audit-2026-11-25.md) | Rendering statik denetimi: RenderGraph, history, assets, M14 VFX |
| [status/gpu-rhi-deep-audit-2026-11-25.md](status/gpu-rhi-deep-audit-2026-11-25.md) | GPU/Vulkan RHI statik denetimi: lifetime, WSI, RT, descriptor ve bounds |
| [status/core-ecs-deep-audit-2026-11-25.md](status/core-ecs-deep-audit-2026-11-25.md) | Core/ECS derin statik denetim: scheduler, events, serialization, lifecycle |
| [status/master-code-audit-2026-11-25.md](status/master-code-audit-2026-11-25.md) | Master teknik denetimi: kaynak bulguları, CI, riskler ve kanıt sınırları |
| [status/master-stabilization-backlog.md](status/master-stabilization-backlog.md) | Denetim bulgularının öncelikli düzeltme ve test planı |
| [game-development-experience-roadmap.md](game-development-experience-roadmap.md) | **GDX (E3–E10)** — proje oluşturma, editör, AI, gerçek Play, export ve bitmiş referans oyun |
| [process.md](process.md) | Geliştirme akışı ve "faz bitti" tanımı |
| [../templates/blank/](../templates/blank/README.md) | **Boş oyun şablonu** — yeni oyun için klasörü kopyala |
| [guides/writing-a-game.md](guides/writing-a-game.md) | **Oyun nasıl yazılır** — buradan başla |
| [../AGENTS.md](../AGENTS.md) | Yapay zekâ ajanları için oyun yazma kuralları (özellik paketleri, Luau, test döngüsü) |
| [guides/scenes-and-worlds.md](guides/scenes-and-worlds.md) | Sahne, harita ve dünya: bir oyun alanı nasıl tanımlanır (kod bilmeden okunur) |
| [guides/shaders-and-materials.md](guides/shaders-and-materials.md) | `.ashader` ve `.material` ile özel yüzeyler, cam, lav, post efektleri (hızlı rehber) |
| [guides/ashader-language.md](guides/ashader-language.md) | **Astral Shader Dili (`.ashader`) tam referans kılavuzu** — söz dizimi, built-in'ler, mimari, sınırlar ve yol haritası |
| [../features/](../features/README.md) | Ortak özellik paketleri (oyunlara kopyalanan tek kaynak) |
| [modules/](modules/) | Her modülün API özeti, tasarım kararları, bilinen sınırları |
| [decisions/](decisions/) | Mimari karar kayıtları (ADR) — neyi neden seçtik |
| [superpowers/plans/](superpowers/plans/) | Her fazın uygulama planı |
| [../CHANGELOG.md](../CHANGELOG.md) | Sürüm sürüm değişiklikler |
