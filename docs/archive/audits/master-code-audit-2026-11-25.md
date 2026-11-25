# Master Code Audit — 25 Kasım 2026

**Durum:** İlk derin kaynak denetimi tamamlandı; düzeltmeler açık.  
**Taban:** [f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c](https://github.com/inkbytefo/AstralEngine/commit/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c) (master, GDX merge).  
**Kapsam:** salt okunur motor denetimi + dokümantasyon; üretim kodu değiştirilmedi.

## 1. Yönetici özeti ve kanıt sınıfları

| Kategori | Adet | Durum |
|---|---:|---|
| A01–A14 | 14 | Kaynak akışında gösterilen hata/sözleşme kusuru; tamamı açık |
| Bunların içinden A01 ve A11 | 2 | Bağımsız C++ kontrol-ifadesi deneyi de yapıldı |
| R01–R03 | 3 | Güvenlik/dayanıklılık riski, ayrıca doğrulanacak |
| V01 | 1 | CI doğrulama engeli |
| D01–D05 | 5 | Bilinen F0 takip kayıtları; kapanış kanıtı yeniden isteniyor |

**Kanıt:** S = sabit commit kaynak akışıyla doğrulandı (motor çalıştırılmadı); L = bağımsız C++ mantık testi de yapıldı; R = risk, platform/entegrasyon deneyi gerekiyor; V = dış doğrulama kaydı. L motor entegrasyon testi değildir. Öncelik P1: veri kaybı, ciddi çalışma zamanı/üretim engeli; P2: işlevsel hata veya dayanıklılık sorunu; P3: iyileştirme. Güvenlik açığı sömürüsü, crash veya GPU hang bu oturumda çalıştırılarak gösterilmedi. Her öneri TDD ile önce başarısız regresyon testi gerektirir.

**Karar:** V01 ve P1 kayıtları kapatılmadan E3'ün güvenilir proje yönetimi tamamlanmış sayılmamalı. Motorun M fazları ve GDX roadmap'i değiştirilmez; bu belge ayrı stabilizasyon kuyruğudur.

## 2. CI / doğrulama engeli

### V01 — Master CI kırmızı (P1, V)

[Run 45507753989](https://github.com/inkbytefo/AstralEngine/actions/runs/45507753989), sabit commit, completed/failure: windows-msvc-debug, windows-msvc, linux-gcc, linux-clang, linux-clang-include-hygiene, linux-tsan; **6/6 job failure**. Önceki dört master run da failure: [45492791530](https://github.com/inkbytefo/AstralEngine/actions/runs/45492791530), [45488572883](https://github.com/inkbytefo/AstralEngine/actions/runs/45488572883), [45487122625](https://github.com/inkbytefo/AstralEngine/actions/runs/45487122625), [45485241341](https://github.com/inkbytefo/AstralEngine/actions/runs/45485241341). Job step listeleri boş; Windows Debug, GCC ve TSan log indirmeleri BlobNotFound/404 döndü. Bu yüzden 6 ayrı kod regresyonu iddia edilmez; runner, workflow, dependency veya test kaynaklı olabilir.

**Öneri:** Actions runner ve hesap/altyapı durumunu, ilk başarısız step'i ve job loglarını kontrol et. Sabit SHA ile temiz Windows Debug/Release, Linux GCC/Clang, include hygiene, TSan ve golden suite çalıştır. Başarısız logları arşivle; retry öncesi kök neden kaydı aç. Testleri sessizce kapatma veya golden referansını otomatik yenileme. [CI workflow](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/.github/workflows/ci.yml).

## 3. Kaynak kod bulguları

### A01 — Başarısız GPU submit başarı gibi yorumlanıyor (P1, S+L; GPU)

**Kanıt:** [modules/gpu/src/device.cpp:279–321](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/gpu/src/device.cpp#L279-L321) başarısız vkQueueSubmit2 için önceki last_submitted değerini döndürüyor; [modules/gpu/src/swapchain.cpp:210–232](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/gpu/src/swapchain.cpp#L210-L232) bu değeri submit SONRASI last_submitted ile kıyaslıyor. Önceden başarılı bir submit varsa iki değer de eşit ve sıfırdan farklı; hata dalına girilmiyor. Bu koşul bağımsız C++ testinde doğrulandı. Present'in sinyallenmeyen semaforu beklemesi veya cihaz hatası sonrası yanlış state ilerlemesi olası; Vulkan üzerinde gerçekleşmiş hang iddiası yok.

**Tekrar üretim hedefi:** Bir başarılı submit sonrası fault-injected submit failure. Present çağrısı ve frame slot ilerlemesi olmamalı; acquired image ve semaforların yeniden kullanılabilirliği ayrıca doğrulanmalı.
**Çözüm:** SubmitResult {status, ticket} gibi açık başarı/hata sözleşmesi veya monoton yeni ticket doğrulaması; device lost ayrı durum; internal immediate submit hata yolunu da incele ([modules/gpu/src/device.cpp:136–154](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/gpu/src/device.cpp#L136-L154)). WSI rebuild ve acquire semafor yaşam döngüsü için ADR gerekebilir.
**Regresyon:** mock/injected Vulkan dispatch ile ilk ve sonraki submit failure, invalid CommandList, device lost, present başarısızlığı, resize recovery. GPU robustness + integration/present testleri. Gerçek GPU validation logu gerekli.

### A02 — Çok dosyalı transaction rollback yeni prefab dosyasını geri almıyor (P1, S; Editor_core)

**Kanıt:** [modules/editor_core/src/transaction.cpp:41–66](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L41-L66) önceki dosyayı optional olarak yedekliyor; rollback yalnız önceki içerik varsa write_atomic çağırıyor, yeni dosyayı kaldırmıyor; geri yükleme hatası da yok sayılıyor. [modules/editor_core/include/astral/editor/transaction.hpp:88–103](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/include/astral/editor/transaction.hpp#L88-L103) temiz rollback sözleşmesi sunuyor. [modules/editor_core/include/astral/editor/document.hpp:24–31](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/include/astral/editor/document.hpp#L24-L31) FS arayüzünde remove yok.
**Tekrar üretim hedefi:** Yeni prefab write başarılı, sahne write başarısız; transaction başarısız ama yeni prefab diskte kalır. Var olan prefab yedeğinin restore edilmesi de ayrıca hata enjeksiyonu ile test edilmeli.
**Çözüm:** Journal içinde create/update/delete ve önceki varlık durumu; ters sırada rollback; FS abstraction'a remove/replace, hata raporu ve recovery journal. Rollback başarısızsa rolled_back=true denmemeli. Staging ve hedef dizin aynı filesystem politikasını doğrula.
**Regresyon:** N'inci I/O işlemi hata enjeksiyonu, dosya varlığı/byte eşitliği, rollback hata raporu, yeniden başlatmada recovery.

### A03 — Undo/Redo prefab değişikliklerini kapsamıyor (P1, S; Editor_core)

**Kanıt:** [modules/editor_core/src/transaction.cpp:270–339](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L270-L339) ManualSave'de staged_prefabs_ ayrı listede birikiyor; snapshot yalnız doc.root/summary/coalesce_key; restore_snapshot yalnız sahne root'unu geri alıyor. [modules/editor_core/include/astral/editor/transaction.hpp:120–143](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/include/astral/editor/transaction.hpp#L120-L143) snapshot ve staged list ayrımı. Immediate modda da diskteki prefab geri alınmıyor. Mevcut prefab undo testi yalnız sahne byte'larını ölçüyor: [modules/editor_core/tests/editor_core_test.cpp:164–215](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/tests/editor_core_test.cpp#L164-L215).
**Tekrar üretim hedefi:** Yeni prefab oluştur, Undo, Save: sahne ilk hâline dönse bile staged prefab save sırasında yazılabilir. Prefab overwrite → Undo durumunda önceki prefab içeriği geri dönmeli.
**Çözüm:** Belge + asset diff'ini tek revision/transaction journal altında sakla; dirty state sahne ve asset overlay bütününe göre hesaplanmalı. Redo asset içeriğini de yeniden oluşturmalı; başka belgelerce kullanılan prefab değişikliklerinde conflict/ownership kontrolü.
**Regresyon:** create/update prefab, Undo/Redo/Save, coalesce, redo dalının kesilmesi, iki belge referansı, immediate/manual mod.

### A04 — Autosave yalnız sahneyi kurtarıyor, staged prefab verisini kaybediyor (P1, S; Editor_core)

**Kanıt:** [modules/editor_core/src/transaction.cpp:192–226](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L192-L226) autosave yalnız doc.root; restore_autosave staged_prefabs_ içeriğini geri kurmuyor. [modules/editor_core/src/transaction.cpp:124–137](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L124-L137) open staged listeyi temizliyor.
**Tekrar üretim hedefi:** ManualSave'de yeni prefab ve instance oluştur; autosave, process restart, restore: instance kaydı geri gelir ama prefab içeriği yoktur.
**Çözüm:** Sürümlü recovery bundle: belge revision/base disk hash, staged asset bytes/paths, journal, checksum; atomik yaz ve restore sırasında validate et. Eski sidecar formatı için migration/diagnostic.
**Regresyon:** New/overwrite prefab + restart + restore + save; corrupt sidecar; external change; recovery iptalinde disk değişmemeli.

### A05 — Seçili AI komutları, seçili alt küme olarak yeniden önizlenmiyor (P1, S; Agent)

**Kanıt:** [modules/editor_core/src/agent_task.cpp:288–310](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L288-L310) tüm proposal PreviewResult üzerinden doğrulanır; [modules/editor_core/src/agent_task.cpp:338–377](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L338-L377) approve_selected filtreli komutları session.apply'e veriyor, yeni preview yok. [modules/editor_core/src/command.cpp:243–251](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/command.cpp#L243-L251) InstantiatePrefab yalnız yol/ID doğrulaması yapıyor; [modules/editor_core/src/transaction.cpp:247–270](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L247-L270) transaction aday belgeyi komut bazında değiştiriyor. UI checkbox değişince preview güncellenmiyor: [modules/editor/src/panels/agent_panel.cpp:27–69](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/panels/agent_panel.cpp#L27-L69).
**Tekrar üretim hedefi:** Birbirine bağımlı SavePrefab + InstantiatePrefab önerisinde yalnız instance seç; yeni prefab oluşturma komutu olmadan alt küme preview'dan geçmemeli. Tersi durumda prefab-only diff doğru sunulmalı.
**Çözüm:** Seçim değiştiğinde dependency-aware candidate rebuild; stale revision tekrar kontrolü; yalnız onaylanan adayın hash'i ile commit. Eksik bağımlılık bloklanmalı veya kullanıcıya birlikte seçme önerilmeli. 3D ve field diff aynı alt kümeyi göstermeli.
**Regresyon:** Bağımlı iki komutun tüm seçim kombinasyonları, stale revision, invalid subset, Apply ve preview eşdeğerliği. Mevcut test ([modules/editor_core/tests/editor_core_test.cpp:273–322](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/tests/editor_core_test.cpp#L273-L322)) varsayılan seçimin tamamını uygular; gerçek alt küme testi ekle.

### A06 — ManualSave prefab overlay, Live ve Play yükleyicilerine ulaşmıyor (P1, S; Studio)

**Kanıt:** [modules/editor_core/src/agent.cpp:126–173](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent.cpp#L126-L173) proposal preview virtual_prefabs overlay kullanıyor. [modules/editor_core/src/transaction.cpp:273–285](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L273-L285) ManualSave prefabları disk yerine staged listte tutuyor. [modules/editor/src/workspace.cpp:88–109](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/workspace.cpp#L88-L109) live loader yalnız diskten okuyor, LoadResult hatalarını yok sayıyor; [modules/editor/src/services/play_host.cpp:13–33](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/services/play_host.cpp#L13-L33) play loader da yalnız diskten okuyor. [modules/editor/src/workspace.cpp:330–351](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/workspace.cpp#L330-L351) save asset listesini yeniliyor ama live scene reload çağırmıyor.
**Tekrar üretim hedefi:** ManualSave'de yeni AI prefab önerisini Apply; Ctrl+S öncesi Live/Play görünümü ile Before/After farklılaşabilir veya prefab yüklemesi hata verir. Save sonrası yeniden açmadan görüntü doğru olmalı.
**Çözüm:** Tek read-only DocumentAssetView: staged overlay > disk, preview/live/play/autosave ortak çözümleyici. LoadResult error görünür olsun; mevcut geçerli live world'ü geçersiz adayla silme. Save commit sonrası cache invalidation.
**Regresyon:** new prefab → Preview → Apply → Live → Play → Save → reopen; entity/material/transform eşitliği, missing prefab hata gösterimi.

### A07 — AI Cancel UI thread'i bloke edebiliyor (P1, S; Agent)

**Kanıt:** [modules/editor_core/src/agent_task.cpp:44–65](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L44-L65) blocking popen/fgets/pclose; [modules/editor_core/src/agent_task.cpp:245–256](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L245-L256) cancel_flag yalnız propose döndükten sonra okunuyor; [modules/editor_core/src/agent_task.cpp:165–172](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L165-L172) join_worker doğrudan thread.join; [modules/editor_core/src/agent_task.cpp:313–321](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L313-L321) cancel join yapıyor; [modules/editor/src/panels/agent_panel.cpp:98–107](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/panels/agent_panel.cpp#L98-L107) UI Cancel bunu çağırıyor.
**Tekrar üretim hedefi:** Mock provider kontrollü biçimde tamamlanmıyor; Cancel tıklandığında UI event loop yanıt vermeye devam etmeli. Window close/next submit de test edilmeli.
**Çözüm:** Cooperative stop_token, platform process runner, süre limiti, process group/job object shutdown; cancel isteği non-blocking, poll durum geçişini yönetir. Provider ownership ve exception boundary; teardown bounded olmalı.
**Regresyon:** fake slow provider, cancellation latency budget, timeout, exception, app close; child process/scratch dosyası kalmaması. Gerçek Codex süreci çalıştırılmadı.

### A08 — Offline agent'ın +1 m taşıma komutu gerçekte mutlak konuma atıyor (P2, S; Agent)

**Kanıt:** [modules/editor_core/src/agent.cpp:69–84](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent.cpp#L69-L84) summary Raise selected entity by +1m; position sabit [0,2,0]. Seçili entity'nin mevcut x/y/z'si okunmuyor.
**Tekrar üretim hedefi:** Seçili entity varsayılan dışı konumdayken +1 m iste; x/z aynı, y+1 olmalı.
**Çözüm:** DocumentEntityId üzerinden snapshot transform resolve (prefab override dahil); coordinate space açık; bulunamazsa hata. Summary ve uygulanan değer eşleşmeli.
**Regresyon:** pozitif/negatif koordinat, prefab part, parent/local transform, missing Transform.

### A09 — Başarısız cell load, streaming tarafından Attached kabul ediliyor (P1, S; World)

**Kanıt:** [modules/app/src/map_runtime.cpp:160–197](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/app/src/map_runtime.cpp#L160-L197) pending error yalnız loglanıyor, spawn_cell hatasına rağmen attached_.push_back; [modules/world/src/streaming.cpp:35–60](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/world/src/streaming.cpp#L35-L60) loader.attach void çağrısından sonra durum koşulsuz Attached. Hata alan hücre için otomatik retry yolu yok; yükleme aralığından çıkıp dönme gerekebilir.
**Tekrar üretim hedefi:** Eksik/bozuk .cell dosyası ve daha sonra düzeltilmesi; failed cell, başarı metriğine dahil olmamalı, tekrar yüklenebilmeli.
**Çözüm:** CellLoadResult/AttachResult; explicit Failed/Retry state, sınırlı backoff, diagnostics; partial spawn rollback veya açık partial policy. MapRuntime ve CellStreamer tek gerçek durum sözleşmesi.
**Regresyon:** missing, corrupt, spawn error, on-disk repair, cancel/retry, unload_all, floating origin ve streaming budget.

### A10 — Entity silinince looping sesin ownership'i kayboluyor (P1, S; Audio)

**Kanıt:** [modules/app/src/services/audio_bridge.cpp:15–18](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/app/src/services/audio_bridge.cpp#L15-L18) voice entity üzerindeki AudioVoice component'iyle tutuluyor; [modules/app/src/services/audio_bridge.cpp:70–110](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/app/src/services/audio_bridge.cpp#L70-L110) yalnız AudioSource'u kaldırılmış ama yaşayan entity'ler stop ediliyor. Entity destroy sonrası AudioVoice da silindiğinden query bunu göremez. [modules/audio/src/audio.cpp:128–157](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/audio/src/audio.cpp#L128-L157) looping voice AudioEngine tarafından başlatılıyor; [modules/audio/src/audio.cpp:202–208](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/audio/src/audio.cpp#L202-L208) yalnız artık çalmayan voice'lar serbest bırakılıyor. Loop devam ederse sahne değiştirme/VFX owner silme sonrası orphan voice kalabilir.
**Tekrar üretim hedefi:** Looping AudioSource başlat, entity destroy; kısa fade sonrası voice_count baseline'a dönmeli. Aynı entity index'i yeniden kullanıldığında eski voice durmalı.
**Çözüm:** AudioBridge'de generational Entity → VoiceHandle ownership registry veya ECS lifecycle callback; stale owner cleanup; source removal/replacement, unload ve stop için ortak release; audio_ null init fallback ayrıca kontrol.
**Regresyon:** headless null audio device ile destroy, remove, looping, one-shot, scene unload, VFX stop. Gerçek ses cihazı testi ayrıca.

### A11 — Cooked model index bounds kontrolünde unsigned çarpım taşması (P1, S+L; Assets)

**Kanıt:** [modules/assets/src/cooked_model.cpp:35–58](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L35-L58) indices uint64_t ve indices*4 > remaining kontrolü yapılıyor. Çarpım taşınca kontrol yanlış sonuç verebiliyor; sonra resize. Bağımsız C++20 mantık deneyinde taşma ve yanlış predicate gözlendi. Gerçek loader'da crash/exception oluştuğu bu oturumda gösterilmedi.
**Çözüm:** Checked size conversion; indices > remaining/sizeof(uint32_t) biçiminde bölüm tabanlı guard; type/container/asset budget limitleri. Hata durumunda nullopt + diagnostic; parser exception boundary.
**Regresyon:** writer/reader malformed-input ve sınır değer testleri; allocation gerçekleşmeden reddetme; ASan/UBSan. Test fixture ayrıntısı düzeltme dalında tutulmalı.

### A12 — AMDL count limit'i dosya boyutu ve makul bellek bütçesine bağlı değil (P1, S; Assets)

**Kanıt:** [modules/assets/src/cooked_model.cpp:10–15](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L10-L15), [modules/assets/src/cooked_model.cpp:35–38](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L35-L38), [modules/assets/src/cooked_model.cpp:62–66](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L62-L66), [modules/assets/src/cooked_model.cpp:135–139](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L135-L139), [modules/assets/src/cooked_model.cpp:163–166](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/cooked_model.cpp#L163-L166): max_count 1<<28; primitive/material/image/node vektörleri kayıtların gerçek minimum boyutu doğrulanmadan resize ediliyor. Küçük bozuk bir belge büyük allocation talebi oluşturabilir.
**Çözüm:** Her section için kalan byte + minimum serialized record size + configurable asset memory budget, incremental decode; bad_alloc/length_error güvenli diagnostik; total count ve decompression bütçesi. [modules/assets/src/pack.cpp:132–154](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/pack.cpp#L132-L154) compressed read için aynı bütçe politikası ayrıca.
**Regresyon:** truncated header/section, count limit, oversize compressed asset, aggregate budget, fuzz + sanitizer. Gerçek büyük allocation tetiklenmedi.

### A13 — Belge değiştirme giriş noktası dirty değişiklikleri doğrudan kaybedebiliyor (P1, S; Studio)

**Kanıt:** [modules/editor/src/workspace.cpp:193–235](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor/src/workspace.cpp#L193-L235) open_document açık session'ı dirty kontrolü olmadan değiştiriyor; [modules/editor_core/src/transaction.cpp:124–137](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L124-L137) open mevcut root/history/staged dosyaları değiştirip dirty=false yapıyor. Public workspace giriş noktasında guard eksik; bütün GUI çağıranlarının davranışı ayrı doğrulanmalı.
**Tekrar üretim hedefi:** Dirty scene A açıkken workspace.open_document(B) çağır; Save/Discard/Cancel kararı olmadan A kaybolmamalı.
**Çözüm:** Ortak close/open state machine; Save/Discard/Cancel; save failure veya cancel'da mevcut project/document/Play durumu korunmalı. World mode ve proje değiştirme de aynı guard'dan geçmeli.
**Regresyon:** dirty scene → scene/world, save failure, cancel, missing target, staged prefab, aktif AI proposal; programatik ve GUI çağrıları.

### A14 — External conflict, dosya yok veya okunamıyor olduğunda false dönüyor (P2, S; Editor_core)

**Kanıt:** [modules/editor_core/src/transaction.cpp:159–177](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L159-L177) disk read başarısızsa has_external_conflict false; save buna dayanıyor. Dışarıdan silinmiş sahne sessizce tekrar oluşturulabilir; read error ile dış silinme farklılaştırılmıyor.
**Tekrar üretim hedefi:** Açık belge diskte silinsin veya read hata versin; save öncesi kullanıcıya conflict/error sunulmalı.
**Çözüm:** Disk state enum (unchanged, modified, missing, inaccessible); explicit recreate/overwrite onayı; staged prefab base hash kontrolü de aynı sözleşmeye alınmalı.
**Regresyon:** external edit, delete, rename, permission failure; force overwrite ve Cancel; disk byte/varlık kontrolü.

## 4. Güvenlik / dayanıklılık riskleri (ayrı doğrulama)

| ID | Öncelik | Risk, kaynak ve öneri |
|---|---|---|
| R01 | P1 | **AI policy enforcement:** [modules/editor_core/include/astral/editor/agent_task.hpp:26–32](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/include/astral/editor/agent_task.hpp#L26-L32) allow_read_and_preview / allow_network tanımlı; [modules/editor_core/src/agent_task.cpp:183–230](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L183-L230) submit yalnız external process iznini gate ediyor, [modules/editor_core/src/agent_task.cpp:132–157](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L132-L157) Codex çağrısı explicit network/sandbox/approval policy aktarmıyor. Harici sağlayıcının gerçek izin davranışı doğrulanmadı. E6 öncesi provider capability gate, explicit sandbox/network policy, no-write dry-run, policy değişikliği ve redaction negatif testleri. |
| R02 | P1 | **Asset root containment:** [modules/editor_core/src/command.cpp:65–74](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/command.cpp#L65-L74) lexical validation var fakat canonical path containment/symlink politikası bu akışta yok. [modules/editor_core/src/transaction.cpp:250–260](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/transaction.cpp#L250-L260) ve [modules/editor_core/src/agent.cpp:150–165](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent.cpp#L150-L165) asset_root/path birleştiriyor. Loose runtime asset erişimi de [modules/assets/src/asset_manager.cpp:432–440](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/assets/src/asset_manager.cpp#L432-L440). Trust boundary'leri ayır, root-bound resolver ve symlink/reparse point politikası getir, read/write/cook/pack tutarlılığını test et. Yetkisiz erişim deneyi yapılmadı. |
| R03 | P2 | **Harici provider yaşam döngüsü ve scratch dosyası:** [modules/editor_core/src/agent_task.cpp:132–157](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L132-L157) geçici isim per-provider sayaçla belirleniyor; ayrı instance çakışması, özel izin ve exception-safe cleanup doğrulanmalı. [modules/editor_core/src/agent_task.cpp:245–256](https://github.com/inkbytefo/AstralEngine/blob/f02fc43fea9c1d6fd391cba2b8f2cf3a8a0b133c/modules/editor_core/src/agent_task.cpp#L245-L256) worker exception boundary görünmüyor. Benzersiz güvenli temp file, RAII cleanup, argv tabanlı process runner, exception → Failed. İki provider, invalid path, throwing fake provider ve cancel testleri. |

Bu bölüm kaynakta eksik görünen kontrolleri listeler; exploit doğrulaması veya dış sağlayıcının güvenlik garantisi hakkında sonuç çıkarmaz. Public depoda yalnız savunma odaklı kayıt tutulmuştur.

## 5. Bilinen TODO / dokümantasyon ve doğrulama borcu

[Capability matrix](../../status/capabilities.md) ve [F0 report](f0-verification-report.md) önceki tarihsel kanıtları içerir. GDX tabanına otomatik taşınmaz.

| ID | Öncelik | Takip ve öneri |
|---|---|---|
| D01 | P1 | F0-TRACK-01: geçmiş 7/12 golden-hw failure. Yeni M14 tabanında lavapipe ve RTX referansı yeniden ölç; görüntü farkını açıklamadan referans değiştirme. |
| D02 | P1 | F0-TRACK-02: ASan CI ve Linux TSan kanıtı. Workflow/config ile güncel CI loglarını karşılaştır; ASan/UBSan parser, editor, world ve runtime error yollarını kapsasın. |
| D03 | P2 | F0-TRACK-03: standalone template script type check. Mevcut template CMake script check çağırıyor; kurulu consumer'da testin gerçekten register edilip çalıştığını `ctest -N` ve CTest loguyla doğrula. |
| D04 | P2 | F0-TRACK-04: sürüm ve capability drift. M14 merge'i gerçekleşmişken capability matrix M14 öncesi particle/volume/ui notu içeriyor; kaynak + test + gerçek durumla güncelle. CMake ve roadmap sürüm politikası da hizalansın. |
| D05 | P2 | F0-TRACK-05: headless çalışma ile headless derleme farklı. Vulkan SDK'sız sunucu build N0 kapsamında tasarlanıp doğrulanmalı; şu an bitmiş multiplayer özelliği sayılmaz. |

TODO/FIXME/HACK/XXX için GitHub kod araması yapıldı; indeks **0a9d32f4** (audit tabanının motor parent'ı) sonuçlarını döndürdü. Todo adlı struct, todo adlı vector ve GLSL swizzle xxx gerçek TODO değildir. Bu arama tam ağaç grep yerine geçmez; yeni commit'te sıfır TODO garantisi yok. Kalan engine M15–M23/N0 ve GDX E3–E10, planlı ürün kapsamı olup bug sayılmaz. Statik glTF skin/morph sınırı, editör workshop root, gerçek game PIE ve UI authoring eksikleri GDX/capability roadmap'inde takip edilir.

## 6. Test ve inceleme matrisi

| Alan | Bu oturumda yapılan | Sonraki çalışma |
|---|---|---|
| Editor_core | command, document, transaction, agent, agent_task, public headers; test case/selected test bodies | transaction fault injection, recovery, path, stale revision; A02–A08/A13–A14 |
| Editor | workspace, play_host, agent panel; seçili giriş noktaları | GUI dirty-close, Live/Play parity, real game runtime |
| GPU | submit, immediate submit, swapchain present/rebuild yolları; robustness test listesi | fault-injected Vulkan, validation layers, device lost |
| Assets | pack, cooked model, asset manager, ByteReader, cook tests | fuzz/ASan/UBSan, malformed data ve memory budget |
| World | streaming, map runtime, transform seçili yolları, world lifecycle, streaming test listesi | failed cell retry, entity ownership, parallel/floating origin |
| App | frame loop, physics bridge, terrain bridge, atmosphere, VFX, gameplay smoke seçili yollar | fixed tick replay, terrain tile failures, smoke/particle/volume GPU regressions |
| Audio | audio bridge, engine voice lifecycle | headless/real device loop cleanup |
| Script | host/timer/hot reload seçili yolları | callback exceptions, reload/state, deterministic replay |
| CI | workflow, 5 run metadata, 6 job durumu, 3 log erişim denemesi, historical F0 | V01 ve test matrisinin gerçek yürütülmesi |

Bu, 1.159 girişli depo ağacının envanteri ve risk odaklı kaynak denetimidir. Her dosya/satır, her shader/pass, her örnek ve bütün platformlar taranmadı. RenderGraph/RT shader matematiği, ECS scheduler yarışları, Jolt köprüleri, asset hot reload ve M14 GPU görselliği için ayrı tam inceleme + runtime ölçüm gerekir. Graphify CLI bu ortamda mevcut değildi; kaynaklar GitHub connector ile commit'e sabitlenerek okundu. GitHub arama indeksinin farklı SHA gösterdiği kısım ayrı belirtildi.

## 7. Deney kaydı (izole mantık kontrolü)

Yerel Git clone DNS erişimi nedeniyle başarısız: `Could not resolve host: github.com`. Bu nedenle CMake configure/build, CTest, sanitizer, GPU test, screenshot ve engine smoke test yapılmadı. Yerel g++ ile motor bağımlılığı olmayan C++20 kontrol ifadeleri derlendi (`-Wall -Wextra -Werror`); üç assertion grubu çalıştı:
- A01: başarısız submit ticket = önceki last_submitted; sonraki present hata predicate'i false → gözlendi.
- A11: uint64_t çarpım taşması mevcut bounds predicate'ini geçersiz kılıyor → gözlendi.
- APAK count-only bypass hipotezi: boş tablo ilk ByteReader okumasında fail olduğundan hipotez reddedildi; bulgu sayılmadı.

Bu deneyler mantıksal reproducer'dır; gerçek engine API'si linklenmedi ve GPU/loader çalıştırılmadı.

## 8. Düzeltme stratejisi ve kapanış

Ayrıntılı sıra: [Master Stabilization Backlog](../msr/master-stabilization-backlog.md). Önce V01; ardından GPU submit ve transaction journal/asset overlay, AI review/cancel, parser hardening, streaming/audio. Her bulgu ayrı test-first commit veya küçük ilişkili PR. Sonra Debug/Release/TSan/ASan/UBSan + ilgili golden/hardware ve clean standalone package. Her kayıt için status, fix SHA, test adı, test logu ve platform ekle. Bu audit commit'i yalnız dokümantasyon; hiçbir bulgu kapatılmış değildir.
