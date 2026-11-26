# E3 — Project Manager ve ProjectContext uygulama planı

**25 Kasım 2026 · Durum: planlı (henüz implementasyon yok)**  
Üst plan: [GDX](../../game-development-experience-roadmap.md). E2 kullanıcı deneyimi ve mevcut CMake kütüphane sınırları korunur.

## Hedef ve kapsam

AstralEditor, workshop örneğine bağlı olmadan herhangi bir desteklenen bağımsız Astral oyun projesini açabilsin, Blank şablonundan yeni proje üretebilsin ve sahne authoring yapabilsin. Bu faz tam gameplay Play-in-Editor (E7), gelişmiş asset import (E4), AI code tools (E6) veya export GUI (E9) teslimi değildir.

## İnceleme ve karar kapısı (E3.0)

- Sabit commit üzerinde mevcut examples/world_editor/src/main.cpp, CMakeLists, EditorWorkspace, Editor_core document sessions, templates/blank, AstralGame.cmake, AGENTS.md, ADR-0035..0037 ve F1 contracts incele.
- ProjectContext ve manifest için mevcut sahiplik/bağımlılık grafiğini çıkar; root, document, game target ve engine version sözleşmesini ADR ile seç. Önerilen manifest JSON, relative paths, schema_version, project_id, name, engine_version, asset_root, startup_document, game_target, build_profiles; isim ve format karar aşamasında kesinleşir.
- Windows/Linux path, symlink, Unicode, boşluk, duplicate directory ve engine ABI/versiyon uyumu için test matrisi yaz. Mevcut dosyaları değiştirmeme ve atomic create ilkesi.
- Faz öncesi clean build ve editor capture baseline'ı kaydet, mevcut test başarısızlıklarını ayrı takip et.

## İş paketleri (test-first)

| ID | İş | Kabul / test |
|---|---|---|
| E3.1 | GPU'suz manifest model, loader, validator ve ProjectContext | roundtrip, schema version, missing field, invalid target/path, root dışı erişim, Unicode; no third-party public type |
| E3.2 | Şablon kataloğu ve transaction tabanlı generator | templates/blank tek kaynak; temp staging + commit/rollback; hata/iptal sonrası yarım proje yok; CLI ve GUI aynı sonuç |
| E3.3 | Proje açma/kapatma/session lifecycle | iki proje arasında document, asset root, undo, selection, provider context sızıntısı yok; dirty close Save/Discard/Cancel; missing recent item |
| E3.4 | Editor entry point ve CLI | --project/--document önerisini ADR'ye göre uygula; --advanced/--capture ve workshop örneği regresyonsuz; hatalı proje net hata |
| E3.5 | Project Manager UI | New/Open/Recent; isim, dizin, şablon, dil, version; doğrulama ve hata durumları; Focus/Advanced, DPI, keyboard/focus; offline |
| E3.6 | Başlangıç scene ve build metadata | Blank oluşur, yüklenir, entity düzenlenir, Ctrl+S, yeniden açılır; oyun dışarıda CMake configure/build/test/run; C++/Luau/Hybrid seçeneği ancak gerçekten desteklenirse aktif |
| E3.7 | Dokümantasyon ve dogfood | TRIALS için bağımsız proje iskeleti, ilk sahne; Windows/Linux test/capture, quickstart, ADR, CHANGELOG, GDX durumu, bağımsız review |

## Test stratejisi

- Unit: manifest, root resolver, generator, recent store, session transition. Testleri Editor_core veya uygun GPU'suz katmanda tut.
- Integration: yeni proje → aç → başlangıç scene → entity değiştir → save → close → reopen; dosya ve ECS eşdeğerliği. İkinci proje ile isolation; autosave/external change ve rollback.
- Consumer: engine install veya mevcut build ile bağımsız Blank configure/build/ctest/run; --script-api ve paket komutu mevcut template sözleşmesiyle çalışır. Tam runtime PIE iddiası E7'ye ait.
- GUI: New/Open/Recent, invalid path, keyboard/Focus/Advanced, offscreen capture; gerçek ekran görüntüleri.
- Regresyon: process.md içindeki debug/release/TSan, ilgili golden/hardware ve layer/editor isolation/feature/script checks. Ortamda mümkün olmayan iş blocked ve gerekçeli; sonuç uydurulmaz.

## Çıkış kriteri

İki ayrı projeyi AI olmadan oluştur/aç/düzenle/kaydet ve standalone çalıştır. Proje seçimi ve asset root sabit workshop yolundan bağımsız olsun. Hiçbir E3 kabulü E7'nin gerçek runtime köprüsüyle karıştırılmasın. Test kanıtlarını SHA/donanım/komutlarla kaydet, review bulgularını kapat, GDX'i güncelle. Ardından E4.
