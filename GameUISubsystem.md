# Astral Engine - GameUISubsystem Mimari Tasarım Belgesi

**Sürüm:** 1.0.0-konsept
**Tarih:** 19 Eylül 2025
**Yazar:** Astral Engine Proje Liderliği

## 1. Giriş ve Felsefe

`GameUISubsystem`, Astral Engine kullanılarak geliştirilen oyunların son kullanıcıya sunulacak arayüzlerini (HUD, menüler, envanter vb.) oluşturmak ve yönetmek için tasarlanmış yüksek performanslı, veri odaklı bir alt sistemdir. Temel felsefesi, `UISubsystem`'in (Dear ImGui) geliştirici odaklı "anlık mod" (immediate mode) yaklaşımının aksine, oyun içi arayüzler için daha uygun olan **"kalıcı mod" (retained mode)** mimarisini benimsemektir.

### Temel Prensipler:

1.  **Performans Odaklılık:** Oyunun ana döngüsü üzerindeki etkiyi en aza indirmek için tasarlanmıştır. Çizim çağrılarını (draw call) birleştirme (batching), doku atlasları (texture atlasing) ve akıllı güncellemeler (smart updates) gibi teknikleri temel alır.
2.  **Veri Odaklı Tasarım (Data-Driven):** Arayüz hiyerarşileri, stiller ve animasyonlar C++ koduna gömülmez. Bunun yerine, UI/UX tasarımcıları tarafından kolayca düzenlenebilen harici dosyalardan (XML/JSON benzeri) yüklenir. Bu, programcı müdahalesi olmadan arayüzde hızlı değişiklikler yapılmasına olanak tanır.
3.  **Sanatçı ve Tasarımcı Dostu İş Akışı:** Mimari, UI/UX tasarımcılarının motorun teknik detaylarına girmeden arayüzleri tasarlayıp uygulayabilmesini hedefler. Gelecekteki görsel bir editörle tam entegrasyon için zemin hazırlar.
4.  **Genişletilebilirlik:** Sistem, yeni UI bileşenleri (widget'lar), layout stratejileri veya animasyon türleri eklemeye olanak tanıyan esnek bir component tabanlı yapı üzerine kuruludur.
5.  **Scripting Entegrasyonu:** Arayüz elemanlarının davranışları (örneğin bir butona tıklandığında ne olacağı) C++ yerine bir scripting dili (örn: Lua) ile tanımlanır. Bu, oyun mantığının arayüzden temiz bir şekilde ayrılmasını ve hızlı prototiplemeyi sağlar.

## 2. Temel Kavramlar ve Terminoloji

*   **Canvas (Tuval):** Bir arayüzün kökünü oluşturan ana konteynerdir. Her bir UI ekranı (ana menü, oyun içi HUD) kendi `Canvas`'ına sahip olur. Render sırasını ve ekran alanını yönetir.
*   **UIElement (Arayüz Elemanı):** Arayüzdeki her bir nesneyi (buton, resim, metin kutusu) temsil eden temel varlıktır. ECS'deki `Entity`'ye benzer şekilde, kendisi veri tutmaz, sadece bir kimliktir.
*   **Component (Bileşen):** Bir `UIElement`'e eklenerek ona özellik ve davranış kazandıran veri bloklarıdır. Örnekler: `UITransformComponent` (pozisyon, boyut), `ImageComponent` (görsel), `TextComponent` (metin), `ButtonComponent` (etkileşim).
*   **Layout (Yerleşim):** `UIElement`'lerin `Canvas` içinde nasıl konumlandırılacağını ve boyutlandırılacağını belirleyen kurallar bütünüdür. Web teknolojilerindeki Flexbox veya Grid gibi sistemlerden ilham alır.
*   **StyleSheet (Stil Sayfası):** Arayüz elemanlarının görsel özelliklerini (renk, font, kenarlık vb.) merkezi olarak tanımlayan dosyalardır. CSS'e benzer bir mantıkla çalışır.
*   **UI Renderer:** `GameUISubsystem`'e ait, UI çizim komutlarını toplayıp optimize ederek ana `RenderSubsystem`'e gönderen özel bir render birimidir.

## 3. Mimari ve Bileşenler

`GameUISubsystem`, birkaç ana bileşenin orkestrasyonu ile çalışır:

### 3.1. `GameUISubsystem` (Ana Sınıf)

*   **Sorumluluk:** Tüm UI sisteminin yaşam döngüsünü yönetir. `ISubsystem` arayüzünü uygular.
*   **İşlevleri:**
    *   Aktif `Canvas`'ları yönetir.
    *   `OnUpdate` döngüsünde layout hesaplamalarını, olay işlemeyi ve animasyonları tetikler.
    *   `UIRenderer`'a çizim için gerekli verileri sağlar.
    *   `AssetSubsystem`'den UI varlıklarını (layout dosyaları, stil dosyaları, fontlar) yükler.

### 3.2. Canvas ve Hiyerarşi Yönetimi

*   Her `Canvas` bir `UIElement` hiyerarşisinin köküdür.
*   `UIElement`'ler, bir ebeveyn-çocuk (parent-child) ilişkisi içinde düzenlenir. Bu, karmaşık ve iç içe arayüzler oluşturmayı kolaylaştırır.
*   Bir `UIElement`, `UITransformComponent` aracılığıyla ebeveynine göreceli bir pozisyona ve boyuta sahip olur.

### 3.3. UI Element ve Component Sistemi

Bu sistem, motorun ana ECS'sinden ilham alır ancak UI'a özeldir ve daha hafiftir.

*   **`UITransformComponent`:** Pozisyon (`position`), boyut (`size`), çapa (`anchor`), pivot (`pivot`) ve döndürme (`rotation`) gibi temel dönüşüm bilgilerini içerir.
*   **`ImageComponent`:** Bir doku (texture) veya doku atlasından bir bölgeyi (`sprite`) referans alır. Renk (`tint`), dolgu modu (`fill mode`) gibi özellikler içerir.
*   **`TextComponent`:** Metin içeriği, font referansı, renk, hizalama ve diğer metin özelliklerini barındırır.
*   **`ButtonComponent`:** Tıklama, üzerine gelme (hover) gibi etkileşim durumlarını yönetir. Normal, hover, pressed gibi durumlar için farklı stilleri veya görselleri referans alabilir. Tıklandığında bir script olayını (`OnClick`) tetikler.
*   **`LayoutComponent`:** Elemanın layout sistemiyle nasıl etkileşime gireceğini belirler (örn: `flex-grow`, `align-self`).

### 3.4. Layout Sistemi (Flexbox Modeli)

Karmaşık ve farklı ekran çözünürlüklerine uyumlu (responsive) arayüzler oluşturmak için modern bir layout sistemi esastır.

*   `UIElement`'ler `FlexContainerComponent` eklenerek birer esnek konteyner haline getirilebilir.
*   Bu konteynerler, çocuk elemanlarını `flex-direction` (yatay/dikey), `justify-content` (hizalama) ve `align-items` (çapraz hizalama) gibi özelliklere göre otomatik olarak konumlandırır.
*   Bu sistem, `OnUpdate` döngüsünün başında, "kirli" (dirty) olarak işaretlenmiş `Canvas`'lar için çalışır ve tüm elemanların nihai pozisyonlarını hesaplar.

### 3.5. Render Sistemi (`UIRenderer`)

Bu, performansın kalbidir.

1.  **Veri Toplama:** `UIRenderer`, `OnUpdate` sonunda tüm görünür `UIElement`'leri ve onların `ImageComponent`, `TextComponent` gibi render edilebilir bileşenlerini gezer.
2.  **Batching (Birleştirme):** Aynı doku atlasını, materyali veya fontu kullanan elemanların vertex verilerini tek bir büyük buffer'da birleştirir. Amaç, GPU'ya gönderilen çizim çağrısı sayısını minimuma indirmektir.
3.  **Komut Listesi Oluşturma:** Optimize edilmiş bu verilerden bir dizi `UIRenderCommand` (örn: `DrawBatch`, `SetClipRect`) oluşturur.
4.  **`RenderSubsystem`'e Teslimat:** Bu komut listesi, ana `RenderSubsystem`'e gönderilir. `RenderSubsystem`, bu UI komutlarını 3D sahne çizildikten sonra, ancak post-processing'den önce işler.

### 3.6. Olay ve Scripting Sistemi

*   Sistem, `PlatformSubsystem`'den gelen ham girdileri (fare konumu, tıklama) alır.
*   **Hit Testing:** Fare konumuna göre hangi `UIElement`'in hedeflendiğini belirler.
*   **Olay Tetikleme:** Bir `ButtonComponent`'e sahip bir eleman tıklandığında, `GameUISubsystem`, `EventManager` aracılığıyla veya doğrudan scripting sistemine "UI.ButtonClicked.MainMenu_NewGame" gibi spesifik bir olay gönderir.
*   Oyun mantığını içeren Lua script'leri bu olayları dinler ve ilgili fonksiyonları çalıştırır (örn: `OnNewGameClicked()`).

## 4. Veri Akışı ve Yaşam Döngüsü

### Bir Frame'in Anatomisi:

1.  **`PreUpdate`:** `PlatformSubsystem` girdileri alır.
2.  **`Update`:** `ECSSubsystem` oyun mantığını günceller.
3.  **`UI` Aşaması (`GameUISubsystem::OnUpdate`):**
    a.  **Olay İşleme:** Girdiler işlenir, hit testing yapılır, hover/pressed durumları güncellenir. Tetiklenmesi gereken script olayları kuyruğa alınır.
    b.  **Animasyon Güncelleme:** Aktif UI animasyonları güncellenir (örn: bir butonun renginin yumuşak geçişi).
    c.  **Layout Hesaplama:** "Kirli" olarak işaretlenmiş `Canvas`'lar için layout sistemi çalışır ve elemanların pozisyon/boyutları hesaplanır.
    d.  **Render Verisi Toplama:** `UIRenderer`, hiyerarşiyi gezer, görünür elemanları toplar ve çizim komutlarını batch'ler halinde organize eder.
4.  **`Render` Aşaması (`RenderSubsystem::OnUpdate`):**
    a.  3D Sahne çizilir.
    b.  **`RenderSubsystem`, `GameUISubsystem`'den render komut listesini alır ve GPU'ya gönderir.**
    c.  Post-processing efektleri uygulanır.

## 5. Varlık (Asset) Formatları

Bu sistemin en önemli gücü, arayüzlerin koddan bağımsız olarak tanımlanmasıdır.

*   **Arayüz Layout Dosyası (`*.aui` - Astral UI):**
    *   XML tabanlı, okunabilir bir format.
    *   `UIElement` hiyerarşisini, bileşenlerini ve özelliklerini tanımlar.
    *   Örnek:
        ```xml
        <Canvas name="MainMenu">
          <UIElement id="Logo" style="logo-style">
            <UITransformComponent anchor="TopCenter" size="256, 128" />
            <ImageComponent texture="Textures/UI/logo.png" />
          </UIElement>
          <UIElement id="NewGameButton" style="main-button">
            <UITransformComponent anchor="Center" size="200, 50" position="0, 50"/>
            <TextComponent text="New Game" font="Fonts/Main.ttf" />
            <ButtonComponent OnClick="MainMenu.OnNewGame" />
          </UIElement>
        </Canvas>
        ```

*   **Stil Sayfası Dosyası (`*.auiss` - Astral UI StyleSheet):**
    *   CSS'den ilham alan, `UIElement`'lerin görsel özelliklerini tanımlayan bir format.
    *   Stillerin yeniden kullanılmasına ve arayüzün genel görünümünün tek bir yerden değiştirilmesine olanak tanır.
    *   Örnek:
        ```css
        .main-button {
          image: "Textures/UI/button_normal.png";
          textColor: #FFFFFF;
          fontSize: 24;
        }

        .main-button:hover {
          image: "Textures/UI/button_hover.png";
          textColor: #FFD700;
        }
        ```

## 6. Performans Optimizasyonları

*   **Draw Call Batching:** `UIRenderer`'ın ana görevi.
*   **Texture Atlasing:** Sık kullanılan UI görselleri, tek bir büyük dokuda birleştirilerek doku değiştirme maliyeti azaltılır. `AssetSubsystem` bu atlasları oluşturabilir.
*   **Dirty Flag Sistemi:** Sadece değişen `Canvas`'lar için layout yeniden hesaplanır.
*   **UI Culling:** Ekran dışında kalan `UIElement`'ler render edilmez.

## 7. Diğer Sistemlerle Entegrasyon

*   **`RenderSubsystem`:** `UIRenderer`'dan aldığı komut listelerini işler.
*   **`AssetSubsystem`:** `*.aui`, `*.auiss`, font ve doku dosyalarını yükler.
*   **`EventManager` & `ScriptingSubsystem`:** UI etkileşimlerini oyun mantığına bağlar.
*   **`PlatformSubsystem`:** Ham girdi verilerini sağlar.

## 8. Geliştirme Yol Haritası

1.  **Faz 1: Temel Kurulum**
    *   `GameUISubsystem` ve temel bileşenlerin (`UITransform`, `Image`, `Text`) oluşturulması.
    *   Basit `*.aui` yükleyicisinin implementasyonu.
    *   `UIRenderer`'da temel batching mekanizmasının kurulması.
    *   FreeType entegrasyonu ile metin renderlama.
2.  **Faz 2: Gelişmiş Layout ve Etkileşim**
    *   Flexbox tabanlı layout sisteminin implementasyonu.
    *   `ButtonComponent` ve scripting olay sisteminin entegrasyonu.
    *   Stil sayfası (`*.auiss`) sisteminin geliştirilmesi.
3.  **Faz 3: Animasyon ve Efektler**
    *   Zaman tabanlı özellik animasyonları için bir sistem eklenmesi (renk geçişleri, boyut değişimleri vb.).
    *   UI'a özel basit shader efektleri için altyapı.
4.  **Faz 4: Editör Entegrasyonu**
    *   `UISubsystem` (ImGui) kullanılarak `GameUISubsystem` için görsel bir editör oluşturulması. Bu editör, tasarımcıların `*.aui` dosyalarını görsel olarak düzenlemesine olanak tanıyacaktır.