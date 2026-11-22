# ADR-0032 — Ölçeklenebilir çalışma zamanı: erişim bildiren sistemler, paralel zamanlayıcı, paralel transform ve G/Ç ayrımı (M13.6)

- **Durum:** Kabul (uygulandı: M13.6)
- **Tarih:** 2026-11-23
- **Bağlam:** [roadmap.md → M13.6](../roadmap.md), [ADR-0004](0004-helping-wait-own-group.md) (`JobSystem`), [ADR-0005](0005-own-ecs.md) (ECS), [ADR-0013](0013-ecs-change-detection-reflection.md) (change detection), [ADR-0017](0017-gameplay-scripting.md) (Luau)

## Sorun

M13.5 ile `Renderer`, `App` ve diğer modüller temiz ve tek sorumluluklu parçalara ayrıldı; ancak çalışma zamanı (runtime) hâlâ birkaç noktada tek iş parçacığına (main thread) bağlıydı:
1. **Sistemlerin sıralı koşması:** Bir fazdaki (`PreUpdate`, `Update`, `FixedUpdate`, `PostUpdate`) C++ sistemleri birbirinden tamamen bağımsız component'lere dokunsa bile `App::run_phase` içinde tek tek sırayla koşuyordu.
2. **Tek iş parçacıklı `update_transforms`:** Kök ve çocuk entity'lerin `WorldTransform` matrisleri her kare tek iş parçacığında baştan hesaplanıyordu.
3. **Ana iş parçacığında G/Ç ve shader derleme:** `MapRuntime::begin_load` hücre dosyasını (`.cell`) diskten ana iş parçacığında okuyup yalnız JSON çözmeyi `JobSystem`'e veriyordu; `MaterialLibrary::update` ise sıcak yeniden yüklemede (`.ashader` değiştiğinde) glslang derlemesini doğrudan ana iş parçacığında yapıyordu.
4. **Script sorgu ve ölçüm maliyeti:** Luau script'lerinde `astral.query({"A", "B"})` her çağrıda tüm eşleşen entity'ler için iç içe Lua tabloları tahsis ediyordu ve profiler'da tüm script modülleri tek bir `"scripts"` etiketi altında toplanıyordu.

## Kararlar

### 1. Ölçek Benchmark'ları (`bench/`, P0)
- `bench/` altında tek bir başsız çalıştırılabilir (`astral-bench`) içinde 5 alt benchmark tanımlandı:
  - `ecs` (100k entity, bağımsız ve çakışan sistemler, seri vs. paralel faz zamanlayıcı)
  - `transforms` (derin ve geniş hiyerarşilerde seri vs. paralel `update_transforms`)
  - `script` (10k scriptli entity üzerinde `astral.system`, `astral.query` ve yeni `astral.each`)
  - `streaming` (hücre okuma + çözme akışı)
  - `shader_compile` (`.ashader` ayrıştırma + SPIR-V derleme süresi)
- CTest'te hızlı bir duman testi (`astral_benchmarks_smoke`) her derlemede benchmark'ların kırılmadığını doğrular.

### 2. Erişim Bildiren Sistemler ve Deterministik Paralel Zamanlayıcı (P1)
- `SystemAccess`: Bir sistemin okuduğu (`reads`) ve yazdığı (`writes`) `ComponentId` kümelerini ya da `exclusive` (dışlayıcı — tüm dünyaya erişir) bayrağını tutar.
- `System` sınıfına `reads<Cs...>()`, `writes<Cs...>()` bildirim yardımcıları ve `virtual void configure(SystemAccess&)` kancası eklendi. Hiçbir erişim bildirmeyen sistem **varsayılan olarak dışlayıcı (`exclusive = true`)** kabul edilir; böylece mevcut tüm oyun sistemleri %100 geriye uyumlu ve güvenli kalır.
- **Çakışma Kuralı:** İki sistem $A$ ve $B$ için:
  - Biri `exclusive` ise çakışır.
  - $(A.\text{writes} \cap (B.\text{reads} \cup B.\text{writes})) \neq \emptyset$ veya $(B.\text{writes} \cap A.\text{reads}) \neq \emptyset$ ise çakışır.
  - Yalnızca ortak okuma ($A.\text{reads} \cap B.\text{reads}$) çakışma **sayılmaz**.
- **Deterministik Dalga (Wave / Frontier) Zamanlayıcısı (`SystemScheduler`):**
  - Kayıt sırasını koruyarak sistemleri ardışık paralel dalgalara (batch/wave) böler: bir dalgadaki hiçbir sistem kendisiyle veya aradaki bekleyen bir sistemle çakışmaz.
  - Dalga boyutu 1 ise sistem doğrudan ana iş parçacığında koşar (ekstra job maliyeti yok); $> 1$ ise dalgadaki sistemler `JobSystem` üzerinde paralel koşar.
  - **Deterministik Change Detection:** Paralel bir dalgada her sisteme kayıt sırasına göre benzersiz bir yazma tick'i (`base_tick + i`) ve kendi `last_run_` taban çizgisi verilir; `World` üzerinde thread-local `(read_baseline, write_tick)` bağlamı sayesinde aynı dalgadaki iki paralel sistem birbirinin `baseline_` veya `tick_` değerini ezmez.
  - `AppDesc::parallel_systems` (varsayılan `true`) ve `ScheduleSettings` ile çalışma anında seri/paralel geçiş yapılabilir; seri ve paralel koşu aynı dünya durumunu üretir.

### 3. Paralel Transform Yayılımı (`update_transforms(World&, JobSystem*)`, P2)
- `update_transforms(world)` imzası `update_transforms(World& world, JobSystem* jobs = nullptr)` olarak genişletildi (`FrameLoop` `context_.jobs` adresini geçirir).
- Kök entity'ler (`Parent` bileşeni olmayanlar) `world.query<const Transform, WorldTransform>().without<Parent>()` ile doğrudan archetype sütunları üzerinde (yeterli sayıda entity varsa `parallel_each` ile) hesaplanır.
- Çocuk entity'ler derinlik seviyelerine (tree levels) ayrılır; her derinlik seviyesi bir önceki seviye bittikten sonra kendi içinde paralel (`jobs->parallel_for`) hesaplanır. Döngü veya ölü ebeveyn koruması aynen korunur.

### 4. Ana İş Parçacığından Çıkan Hücre G/Ç ve Asenkron Shader Derleme (P3)
- **`MapRuntime::begin_load`:** Cook edilmiş `.cell` dosyasının `assets_.read(...)` ile diskten/paketten okunması da `jobs_.submit` içine taşındı (`AssetManager::read` ve `Pack::read` zaten thread-safe'tir). Ana iş parçacığı `begin_load` sırasında hiçbir dosya G/Ç'si yapmaz.
- **`MaterialLibrary::update`:** Değişen `.ashader` dosyalarının `parse_ashader` + `compile_surface_shader` / `compile_screen_shader` (glslang SPIR-V derlemesi) aşamaları `JobSystem` varsa arka plan görevinde çalıştırılır; ana iş parçacığı yalnız hazır SPIR-V'den hafif GPU pipeline/materyal güncellemesini yapar.

### 5. Toplu Script Sorgusu (`astral.each`) ve Modül Başına Profiler Kapsamı (P4)
- **`astral.each({"A", "B"}, function(entity, a, b) ... end)`:** Tablo tahsisi yapmadan eşleşen entity'leri doğrudan Luau geri çağrısına (callback) akıtır ve aynı `ComponentBox` userdata vekillerini yerinde yeniden kullanır.
- **Modül Başına Profiler Kapsamı:** `ScriptEnv::profiler` eklendi; `ScriptHost::run` her script modülünü kendi dosya adıyla (`ASTRAL_PROFILE_SCOPE(impl.env.profiler, m.name.c_str())`) ölçer. Böylece F1 → Performance panelinde hangi `.luau` dosyasının ne kadar süre harcadığı doğrudan görünür.

## Sonuçlar

- Erişim bildiren bağımsız sistemler çok çekirdekli CPU'da paralel ölçeklenir; bildirmeyen sistemler hiçbir davranış değişikliği olmadan güvenli sıralı modda çalışır.
- Büyük sahnelerde kök ve hiyerarşik transform güncellemesi `JobSystem` üzerinden paralelleşir.
- Açık dünyada hücre yükleme sırasında ana iş parçacığında dosya okuma bloklaması kalmaz.
- Luau script'leri `astral.each` ile sıfır tablo tahsisiyle binlerce entity tarayabilir ve profiler'da modül bazında izlenir.
