# Astral Engine - Dokümantasyon

Astral Engine, modern, modüler C++20 tabanlı bir 3D oyun motorudur. Frostbite'tan ilham alan subsystem mimarisi üzerine kurulu, veri odaklı tasarım prensipleri ile geliştirilmektedir.

## 📚 Dokümantasyon İçeriği

### 🏗️ Mimari ve Tasarım

- [**Mimari Genel Bakış**](architecture/overview.md) - Motorun temel mimari prensipleri ve yapısal tasarımı
- [**Core Modüller**](architecture/core-modules.md) - Engine, Logger, MemoryManager gibi çekirdek sistemler
- [**Subsystem'ler**](architecture/subsystems.md) - Alt sistemlerin nasıl çalıştığı ve entegrasyonu
- [**Modül Bağımlılıkları**](architecture/module-dependencies.md) - Modüller arası veri akışı ve bağımlılıklar
- [**Yapılan İyileştirmeler**](architecture/improvements.md) - Optimizasyonlar ve hata düzeltmeleri

### 🔧 API Referansı

- [**Core Sistemler**](api/core.md) - Engine, Logger, MemoryManager gibi çekirdek API'ler
- [**Renderer Sistemi**](api/renderer.md) - Vulkan renderer ve rendering API'leri
- [**Asset Yönetimi**](api/asset.md) - AssetManager ve varlık yükleme sistemleri
- [**Platform Soyutlama**](api/platform.md) - PlatformSubsystem ve giriş yönetimi
- [**ECS Sistemi**](api/ecs.md) - Entity Component System API'leri

### 📖 Kullanım Kılavuzları

- [**Başlangıç Kılavuzu**](guides/getting-started.md) - Motoru kurulumu ve ilk adımlar
- [**Basit Oyun Oluşturma**](guides/simple-game.md) - Temel bir oyunun nasıl oluşturulacağı
- [**Shader Yazımı**](guides/shaders.md) - Shader dosyaları ve kullanımı
- [**Asset Yönetimi**](guides/assets.md) - Varlıkların nasıl yönetileceği

### 🛠️ Geliştirme

- [**Build Sistemi**](development/build.md) - CMake build sistemi ve derleme komutları
- [**Code Style**](development/code-style.md) - Kodlama standartları ve stil kılavuzu
- [**Hata Ayıklama**](development/debugging.md) - Debug modu ve hata ayıklama teknikleri
- [**Performans Optimizasyonu**](development/performance.md) - Performans optimizasyonu ipuçları

## 🎯 Mevcut Özellikler (v0.2.0)

### ✅ Tamamlanan Sistemler

- **Modüler Subsystem Mimarisi** - Esnek ve genişletilebilir alt sistem yapısı
- **Merkezi Logging Sistemi** - Çok seviyeli loglama ve dosya çıkışı
- **Event-Driven İletişim** - Thread-safe event yönetimi
- **Bellek Yönetim Sistemi** - Özel bellek ayırıcılar ve frame-based bellek
- **Asset Management Sistemi** - Asenkron varlık yükleme ve önbellekleme
- **Vulkan Renderer** - Modern 3D rendering API desteği
- **ECS Temel Yapısı** - Entity Component System mimarisi
- **Kamera Sistemi** - 3D kamera yönetimi ve matris hesaplamaları
- **Math Kütüphanesi** - GLM entegrasyonu ve matematiksel işlemler
- **Hot Reload Desteği** - Runtime asset yeniden yükleme
- **Asset Validation** - Varlık bütünlüğü ve format doğrulama
- **Dependency Tracking** - Asset'ler arası bağımlılık yönetimi
- **Thread Pool** - Asenkron görev yönetimi
- **Post-Processing Efektleri** - Bloom, tonemapping gibi efektler
- **UI Sistemi** - Dear ImGui tabanlı kullanıcı arayüzü

### 🚧 Geliştirme Aşamasında

- **Fizik Sistemi** - Jolt Physics entegrasyonu
- **Audio Sistemi** - Ses yönetimi
- **Scripting Desteği** - Lua veya benzeri scripting dilleri

### 📋 Planlanan Özellikler

- **Scene Management** - Sahne yönetimi ve hiyerarşi
- **Animation System** - Animasyon sistemi
- **AI Framework** - Yapay zeka framework'ü
- **Networking** - Çok oyunculu ağ desteği
- **Asset Pipeline** - Gelişmiş asset işleme hattı
- **Editor Interface** - Görsel editör arayüzü

## 🏗️ Temel Mimari Prensipler

Astral Engine, şu temel prensipler üzerine kuruludur:

### 1. Modülerlik
Her subsystem bağımsız ve değiştirilebilir. Yeni bir özellik eklemek, yeni bir alt sistem oluşturmak kadar kolaydır.

### 2. Soyutlama
Her alt sistem, kendi iç karmaşıklığını gizler ve dışarıya temiz bir arayüz sunar.

### 3. Veri Odaklı Tasarım
Motorun merkezinde, verilerin kendisi ve bu verilerin nasıl işlendiği yer alır. ECS pattern ile performans odaklı tasarım.

### 4. Gevşek Bağlılık
Alt sistemler birbirine doğrudan bağımlı olmamalıdır. İletişim, kontrollü arayüzler üzerinden sağlanır.

### 5. Paralellik
Bağımsız alt sistemler, modern çok çekirdekli işlemcilerden en iyi şekilde faydalanmak için potansiyel olarak ayrı iş parçacıklarında çalıştırılabilir.

## 📁 Proje Yapısı

```
docs/
├── README.md                    # Bu dosya
├── architecture/               # Mimari dokümantasyonu
│   ├── README.md
│   ├── overview.md            # Genel mimari bakış
│   ├── core-modules.md        # Core modüller detayı
│   ├── subsystems.md          # Subsystem mimarisi
│   ├── module-dependencies.md # Bağımlılıklar ve veri akışı
│   └── improvements.md        # Yapılan iyileştirmeler
├── api/                        # API referansı
│   ├── README.md
│   ├── core.md                # Core API'ler
│   ├── renderer.md            # Renderer API
│   ├── asset.md               # Asset yönetimi API
│   ├── platform.md            # Platform soyutlama API
│   └── ecs.md                 # ECS API
├── guides/                     # Kullanım kılavuzları
│   ├── README.md
│   ├── getting-started.md     # Başlangıç kılavuzu
│   ├── simple-game.md         # Basit oyun örneği
│   ├── shaders.md             # Shader yazımı
│   └── assets.md              # Asset yönetimi
└── development/                # Geliştirme dokümantasyonu
    ├── README.md
    ├── build.md               # Build sistemi
    ├── code-style.md          # Kod stili
    ├── debugging.md           # Hata ayıklama
    └── performance.md         # Performans optimizasyonu
```

## 🚀 Hızlı Başlangıç

### Gereksinimler
- CMake 3.24+
- C++20 destekleyen derleyici
- Vulkan SDK

### Kurulum
```bash
git clone <repository-url> AstralEngine
cd AstralEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Çalıştırma
```bash
./bin/AstralEngine
```

## 🤝 Katkıda Bulunma

Dokümantasyona katkıda bulunmak isterseniz:

1. Repository'yi fork edin
2. `docs` branch'inde çalışın (`git checkout -b docs-improvement`)
3. Değişikliklerinizi commit edin (`git commit -m 'Add documentation for X'`)
4. Branch'i push edin (`git push origin docs-improvement`)
5. Pull Request oluşturun

### Dokümantasyon Standartları

- **Türkçe** dilinde yazın
- Markdown formatını kullanın
- Kod örnekleri ekleyin
- Açık ve anlaşılır olun
- Örnekler çalışır durumda olmalı

## 📞 Destek

Sorun yaşarsanız:

1. **Logları kontrol edin**: Konsol çıktısındaki hata mesajlarına bakın
2. **Bağımlılıkları doğrulayın**: Tüm gerekli kütüphanelerin kurulu olduğundan emin olun
3. **CMake cache'ini temizleyin**: `build/` dizinini silip yeniden yapılandırın
4. **Mimari dokümantasyonunu inceleyin**: Detaylı teknik bilgi için `architecture/` klasörüne bakın
5. **Issue oluşturun**: Tam hata çıktısını ve sistem bilgilerini ekleyin

## ✅ Başarı Göstergeleri

Dokümantasyonunuz doğru çalışıyorsa:

- ✅ Tüm bağlantılar çalışıyor
- ✅ Kod örnekleri derleniyor
- ✅ Diyagramlar doğru görüntüleniyor
- ✅ API açıklamaları güncel
- ✅ Kullanım örnekleri çalışıyor
- ✅ Mimarik tasvirleri doğru

---

**Not**: Bu dokümantasyon aktif geliştirme aşamasındadır. Yeni özellikler eklendikçe güncellenecektir.

**Son Güncelleme**: 21 Eylül 2025
**Versiyon**: 0.2.0-alpha
**Dil**: Türkçe (TR)