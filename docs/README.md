# Astral Engine - Dokümantasyon

Astral Engine, modern, modüler C++20 tabanlı bir oyun motorudur. Frostbite'tan ilham alan subsystem mimarisi üzerine kurulu, veri odaklı tasarım prensipleri ile geliştirilmektedir.

## 📚 Dokümantasyon İçeriği

### 🏗️ Mimari ve Tasarım

- [**Mimari Genel Bakış**](architecture/README.md) - Motorun temel mimari prensipleri ve yapısal tasarımı
- [**Subsystem Mimarisi**](architecture/subsystem.md) - Alt sistemlerin nasıl çalıştığı ve entegrasyonu
- [**Event Sistemi**](architecture/event-system.md) - Event-driven iletişim mekanizması
- [**ECS (Entity Component System)**](architecture/ecs.md) - Entity Component System mimarisi ve bileşenler

### 🔧 API Referansı

- [**Core Sistemler**](api/core.md) - Engine, Logger, MemoryManager gibi çekirdek API'ler
- [**Renderer Sistemi**](api/renderer.md) - Vulkan renderer ve rendering API'leri
- [**Asset Yönetimi**](api/asset.md) - AssetManager ve varlık yükleme sistemleri
- [**Platform Soyutlama**](api/platform.md) - PlatformSubsystem ve giriş yönetimi

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

## 🎯 Mevcut Özellikler (v0.1.0-alpha)

### ✅ Tamamlanan Sistemler

- **Modüler Subsystem Mimarisi** - Esnek ve genişletilebilir alt sistem yapısı
- **Merkezi Logging Sistemi** - Çok seviyeli loglama ve dosya çıktısı
- **Event-Driven İletişim** - Thread-safe event yönetimi
- **Bellek Yönetim Sistemi** - Özel bellek ayırıcılar ve frame-based bellek
- **Asset Management Sistemi** - Asenkron varlık yükleme ve önbellekleme
- **Vulkan Renderer** - Modern 3D rendering API desteği
- **ECS Temel Yapısı** - Entity Component System mimarisi
- **Kamera Sistemi** - 3D kamera yönetimi ve matris hesaplamaları
- **Math Kütüphanesi** - GLM entegrasyonu ve matematiksel işlemler

### 🚧 Geliştirme Aşamasında

- **SDL3 Entegrasyonu** - Platform soyutlama katmanı
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
│   ├── subsystem.md
│   ├── event-system.md
│   └── ecs.md
├── api/                        # API referansı
│   ├── core.md
│   ├── renderer.md
│   ├── asset.md
│   └── platform.md
├── guides/                     # Kullanım kılavuzları
│   ├── getting-started.md
│   ├── simple-game.md
│   ├── shaders.md
│   └── assets.md
└── development/                # Geliştirme dokümantasyonu
    ├── build.md
    ├── code-style.md
    ├── debugging.md
    └── performance.md
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

## 📞 İletişim

- **GitHub Issues**: Bug raporları ve öneriler için
- **Discord**: Geliştirici topluluğu için

---

**Not**: Bu dokümantasyon aktif geliştirme aşamasındadır. Yeni özellikler eklenmeye devam edecektir.