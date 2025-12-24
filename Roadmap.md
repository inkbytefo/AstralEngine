# 🗺️ Astral Engine Roadmap

Bu belge, Astral Engine projesinin geliştirme yol haritasını, tamamlanan kilometre taşlarını ve gelecek hedeflerini içerir.

## 🚀 Faz 1: Temel Çekirdek ve Mimari (Tamamlandı)
- ✅ **Modüler Subsystem Mimarisi**: Engine, Platform, Asset, Render, UI ayrımı.
- ✅ **Deterministik Yaşam Döngüsü**: Kayıt sırasına göre Init, tersine göre Shutdown (LIFO).
- ✅ **Performanslı Update Döngüsü**: Stage-based pointer caching ile sıfır-lookup çekirdek döngüsü.
- ✅ **Logging Sistemi**: Renkli ve seviyeli loglama (spdlog benzeri).
- ✅ **Event System**: Subsystemler arası gevşek bağlı iletişim.

## 🛠️ Faz 2: Editör ve ECS Geçişi (Devam Ediyor)
- ✅ **ECS Entegrasyonu (EnTT)**: Sahne ve Entity sınıfları modernize edildi.
- 🔄 **Advanced Properties Panel**: (Yükleniyor...) Entity özelliklerini anlık düzenleme.
- 🔄 **Scene Serialization**: Sahneyi diskten yükleme/kaydetme (.scene).
- ✅ **Editor UI (ImGui)**: Unreal style docking ve viewport entegrasyonu.
- ✅ **Vulkan RHI**: Backend soyutlaması ve shader yönetimi.

## 📅 Faz 3: İleri Render ve Fizik (Planlanan)
- [ ] **PBR Rendering**: Fiziksel tabanlı materyal sistemi (Metalness/Roughness/AO).
- [ ] **Shadow Mapping**: Gerçek zamanlı dinamik gölgeler.
- [ ] **Post-Processing**: Bloom, HDR, Tone Mapping.
- [ ] **Jolt Physics**: Rigidbody ve Collision bileşenleri.

---
*Son Güncelleme: 24.12.2024*
