# A2.1 — İstemci sahneleri ve kameralar

## Yaşam döngüsü

`Application::Run()` pencere/GPU sahipliğini ve ana döngüyü korur. İstemci üç hook kullanır:

1. `CreateInitialScene()`: başlangıç sahnesini döndürür. Varsayılan davranış gerçekten boş sahnedir. Null dönüş açık bir başlatma hatasıdır.
2. `OnInitialize()`: aktif sahne hazırken `PushSystem<T>()` ile istemci sistemlerini kaydeder. Grafik modunda render servisleri de hazırdır; headless modunda GPU/window getter'ları null olabilir. Kaydedilen sistemlerin `OnInit()` çağrıları bu hook'tan sonra yapılır.
3. `OnUpdate(FrameContext&, uint32_t)`: gameplay aşamasında, fizik/world-transform/render extraction öncesinde çalışır. İstemci işleri ayrıca `ISubsystem` olarak kaydedilebilir; sistemin `SystemStage` değeri çalışma aşamasını belirler.

Hook'lar base constructor içinde çağrılmaz; türetilmiş nesne tamamen oluşturulduktan sonra `Run()` tarafından çağrılır. İstemci tarafından sistemlere verilen referansların sistem ömrü boyunca geçerli olması gerekir. Sahne/entity sahipliği için motorun Scene/Entity API'si kullanılmalıdır.

## Minimal özel istemci

```cpp
#include "AstralEngine.h"

class MyGame final : public Astral::Application {
protected:
    std::shared_ptr<Astral::Scene> CreateInitialScene() override {
        auto scene = std::make_shared<Astral::Scene>("My Level");

        auto sphere = scene->CreateEntity();
        sphere.AddComponent<Astral::TransformComponent>();
        sphere.AddComponent<Astral::SDFComponent>();

        auto camera = scene->CreateEntity();
        camera.AddComponent<Astral::TransformComponent>(glm::vec3(0, 0, 5));
        camera.AddComponent<Astral::CameraComponent>();
        if (!Astral::SetActiveCamera(scene->GetRegistry(), camera.GetHandle())) {
            throw std::runtime_error("Camera selection failed");
        }
        return scene;
    }
};

namespace Astral {
Application* CreateApplication() { return new MyGame(); }
}
#include "Astral/EntryPoint.hpp"
```

Yeni oyun hedefi yalnızca `AstralEngine` kütüphanesine bağlanır. Özel sahne/kamera için motor `.cpp` dosyaları değiştirilmez.

## Kamera sözleşmesi

- `CameraComponent`: perspektif dikey FOV (radyan), yakın/uzak sınır ve `primary` bayrağı içerir.
- Pozisyon ve yön `TransformComponent` içindedir. Yerel ileri eksen `-Z`, yukarı eksen `+Y` olur. Roll ve parent transform'u desteklenir; kamera baz vektörleri normalize edilerek ölçek etkisi ayrıştırılır.
- `SetActiveCamera(registry, handle)` önce hedefin Camera/Transform bileşenlerini kontrol eder, sonra diğer kameraları pasifleştirir. Geçersiz handle mevcut seçimi değiştirmez. `NullEntityHandle` seçimi temizler.
- Kamera bileşeni eklemek otomatik seçim yapmaz. Birden fazla `primary` içeren yüklenmiş veride en küçük geçerli handle deterministik olarak seçilir.
- Geçersiz FOV, clip aralığı, aspect veya dejenere transform için extraction kamera döndürmez.
- Silinen/pasif/geçersiz kamera yerine gizli bir varsayılan kamera seçilmez. Renderer opak siyah görüntü üretir ve temporal history'yi geçersiz kılar.
- `RenderCamera`, ECS depolamasına referans tutmayan karelik bir veri kopyasıdır. Güncel world transform ve viewport aspect ile çıkarılır. Renderer bu veriyi raymarching ve deferred ışık hesaplamasında kullanır.
- Kamera/sahne değişimi veya projection değişimi history reset üretir. Normal kamera hareketinde önceki view-projection korunur; gelişmiş motion-vector TAA bu işin kapsamı değildir.
- Kamera bileşeni açık 16 baytlık little-endian alan düzeniyle sahneye yazılır. Kopyada korunur; tek nesne duplicate işleminde yeni kameranın `primary` değeri sıfırlanır.

İlk uygulamada yakın/uzak sınırlar raymarch mesafe aralığıdır. Orthographic/asimetrik projection ve sinematik kamera efektleri bu değişikliğin kapsamına dahil değildir.

## Sandbox ve editör

`Projects/Sandbox/src/DemoScene.hpp` eski dört nesneli sahneyi, 32 SDF nesneli stress sahnesini, kamera açısını ve frame-index tabanlı animasyonları içerir. Motor bu dosyaya bağımlı değildir. GPU regresyon hedefi aynı istemci fixture'ını kullanır.

Eşzamanlı referans oyun geliştirmesiyle Sandbox'ın varsayılanı puzzle modudur. Eski demo `--demo` veya benchmark seçenekleriyle açılır. Demo benchmark'ında `fixedTimeStep` ve `fixedDeltaTime` 0,016 olarak seçilerek tarihsel hareketler korunur; bu istemciye ait bir test politikasıdır.

`EmptyGameTemplate` geometri veya kamera eklemez. Editör, proje başlangıç sahnesini kullanır; kamera içermeyen ilk sahne için kendi kamera entity'sini sağlar. Viewport gizmo matrisleri sahnenin aktif kamerasından çıkarılır.

## Doğrulama

CPU kamera testleri `EngineTests --scene` ile çalışır. Geçici sahne dosyası testin çalışma dizininde oluşturulur; testleri izole bir dizinde çalıştırın.

GPU testi:

```powershell
cmake --build build-release --target CameraGpuTests
.\build-release\CameraGpuTests.exe artifacts/a21/after artifacts/a21/before
```

İkinci dizin isteğe bağlıdır ve **değişiklik öncesinde** kaydedilmiş, aynı GPU/shader ayarlarıyla üretilmiş RGBA dosyalarını içermelidir. Dosyalar 320×180, RGBA8, beşinci frame çıktısıdır. Eski sürüm kaydı yoksa ikinci argümanı atlamak kamera işlev testlerini çalıştırır; bu durumda tarihsel görsel regresyon doğrulandı denemez.

Testler iki render yolunda şunları doğrular:

- İstenen beş frame'in tamamlanması ve istemci sistemi initialization sırası.
- Varsayılan sahnede sıfır entity ve siyah çıktı.
- Önce görüntü ürettikten sonra aktif kamera kaldırılınca siyah çıktı.
- Kamera pozisyonu ve FOV değişince çıktı değişmesi.
- Referans varsa kanal ortalama mutlak farkı ≤0,5 ve farkı 8'i aşan kanal oranı ≤%1.

5 Eylül 2026 yerel önce/sonra karşılaştırmasında monolitik MAE **0,003125**, maksimum fark **4**; deferred MAE **0,0265451**, maksimum fark **1** ölçüldü (kanal aralığı 0–255). 8'i aşan kanal farkı yoktu. Bunlar küçük standart sahne regresyonudur; büyük sahne performansı veya genel görsel kalite sertifikası değildir.

GPU smoke testi de artık kendi sphere/kamera sahnesini açıkça oluşturur; boş base application nedeniyle compute yolunun sessizce atlanması önlenir.
