# AstralEngine Dokümantasyonu

Bu klasör motorun **güncel** durumunu anlatır. Her faz (M0, M1, …) bittiğinde güncellenir —
kurallar [process.md](process.md)'de.

| Doküman | İçerik |
|---|---|
| [architecture.md](architecture.md) | Katmanlar, SOLID kuralları, bağımlılık yönü, klasör yapısı |
| [roadmap.md](roadmap.md) | Fazlar, her fazın durumu ve bitiş kriterleri |
| [design/astral-motion-architecture.md](design/astral-motion-architecture.md) | Hybrid animasyon, modül sahipliği, Game API ve AN/CH teslimleri |
| [design/atlas-mannequin.md](design/atlas-mannequin.md) | ATLAS referans karakteri, görsel, rig ve üretim standardı |
| [technology-labs-roadmap.md](technology-labs-roadmap.md) | Oyun odaklı teknoloji lab'leri, native Game API, reusable FeatureSet paketleri ve performans kabulü |
| [editor/studio-intelligence-target-architecture.md](editor/studio-intelligence-target-architecture.md) | Studio/Intelligence hedef mimarisi ve sıfırdan refaktör sözleşmesi (öneri) |
| [status/app-gameplay-deep-audit-2026-11-25.md](status/app-gameplay-deep-audit-2026-11-25.md) | App/Gameplay statik denetimi: input, feature composition, Luau lifecycle, save ve world transition |
| [status/assets-shader-deep-audit-2026-11-25.md](status/assets-shader-deep-audit-2026-11-25.md) | Assets/Shader statik denetimi: glTF, cook, dependency graph, hot reload ve ABI |
| [status/physics-jolt-deep-audit-2026-11-25.md](status/physics-jolt-deep-audit-2026-11-25.md) | Physics/Jolt statik denetimi: contact identity, fixed step, origin, joints ve shape validation |
| [status/rendering-deep-audit-2026-11-25.md](status/rendering-deep-audit-2026-11-25.md) | Rendering statik denetimi: RenderGraph, history, assets, M14 VFX |
| [status/gpu-rhi-deep-audit-2026-11-25.md](status/gpu-rhi-deep-audit-2026-11-25.md) | GPU/Vulkan RHI statik denetimi: lifetime, WSI, RT, descriptor ve bounds |
| [status/core-ecs-deep-audit-2026-11-25.md](status/core-ecs-deep-audit-2026-11-25.md) | Core/ECS derin statik denetim: scheduler, events, serialization, lifecycle |
| [status/master-code-audit-2026-11-25.md](status/master-code-audit-2026-11-25.md) | Master teknik denetimi: kaynak bulguları, CI, riskler ve kanıt sınırları |
| [status/master-stabilization-roadmap.md](status/master-stabilization-roadmap.md) | Merkezi stabilizasyon roadmap'i: 137 kayıt, bağımlılık ve kabul kapıları |
| [guides/game-architecture.md](guides/game-architecture.md) | World/Scene/Prefab/Entity/Component ve gerçek gameplay geliştirme akışı |
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
