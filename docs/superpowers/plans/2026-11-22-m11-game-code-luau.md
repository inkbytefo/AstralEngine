# AstralEngine M11 — Modüler oyun kodu + Luau Uygulama Planı

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans.

**Goal:** Oyun kodu özellik paketleri (`feature.json` + `install`) olarak düzenlenir, kurallar Luau'da
yazılır, motor sınırları denetler. Pong paketlere bölünür, kuralları Luau'da çalışır ve aynı testleri geçer.

**Architecture:**
- `Astral::World`: ECS'e ham (tip-silinmiş) erişim (`add_raw/get_raw/has_raw/remove_raw`), `TypeRegistry`
  girdilerine **alan tablosu** (ad, tür, ofset), şemadan **dinamik component** (`define_component`),
  `EventRegistry` (adla olay gönder/oku, JSON yükü).
- Yeni `Astral::Script` (katman: Core + World; App'e ve Input'a bağlı değil): Luau VM (FetchContent, `src/`
  içinde, public header'da Luau tipi yok), `ScriptHost`; `ScriptServices` küçük arayüzüyle input/log gelir.
- `Astral::App`: `ctx.scripts`, `ctx.load_script`, script sistemleri fazlara bağlanır, hot-reload,
  `Feature`/`FeatureSet` (manifest, bağımlılık sırası, döngü hatası), `InputActions` (`.actions` dosyası),
  zamanlayıcılar, `astral-features` sınır denetimi, API belgesi üretimi.

**Spec:** ADR-0023, ADR-0017, roadmap M11.

**Kapsam kararı (gerekçe: tek fazda sağlam çekirdek, gerisi bilinen sınır):** gamepad (Input'ta yok),
tween/durum makinesi/tetik hacmi/kayıt yardımcıları, Luau bytecode cook ve luau-lsp entegrasyonu M11'de
yok; `docs/modules/script.md` "Bilinen sınırlar"da listelenir. Script başına `require` yok: tek global
`astral` tablosu (sandbox basit kalır).

## Review Focus
1. Script hatası (sözdizimi, çalışma hatası, sonsuz döngü) oyunu düşürmez; dosya:satır loglanır, o script durur.
2. Hot-reload: durum component'lerde olduğundan kaybolmaz; bozuk dosya eski sürümü bırakır.
3. Ölü/yanlış entity'ye erişim hata verir, çökmez; bilinmeyen component/alan adı açık hata mesajı verir.
4. Döngüsel özellik bağımlılığı ve eksik bağımlılık `install` öncesi hata verir.
5. Aynı tohum → aynı maç (Luau RNG deterministik, FixedUpdate sabit adım).

## Görevler (TDD; commit faz sonunda)
1. **M11.1 world: ham erişim + alan tablosu + dinamik component + EventRegistry.**
2. **M11.2 script: Luau VM, sandbox, hata/süre sınırı, `astral` temel API** (log, random, time).
3. **M11.3 script: component bağlama** (proxy, `get/has/add/remove`, `query`), `define_component`.
4. **M11.4 script: sistemler, davranışlar, olaylar, zamanlayıcılar**; fazlara bağlama.
5. **M11.5 app: `ctx.load_script`, hot-reload, `InputActions`, `astral.input`.**
6. **M11.6 app: `Feature`/`FeatureSet`, `astral-features` denetimi.**
7. **M11.7 API üretimi:** `astral.d.luau` + `API.md` (reflection'dan).
8. **M11.8 Pong paketlere bölünür:** `pong_rules` (Luau) + `pong_audio` (C++); `AGENTS.md`; belgeler; 0.16.0.
