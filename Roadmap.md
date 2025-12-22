# 🗺️ Astral Engine Roadmap

Bu belge, Astral Engine projesinin geliştirme yol haritasını, tamamlanan kilometre taşlarını ve gelecek hedeflerini içerir.

## 🚀 Faz 1: Temel Çekirdek ve Mimari (Tamamlandı)
- ✅ **Modüler Subsystem Mimarisi**: Engine, Platform, Asset, Render, UI ayrımı.
- ✅ **Logging Sistemi**: Renkli ve seviyeli loglama (spdlog benzeri).
- ✅ **Asset Management**: `AssetManager` ile model, texture ve materyal yükleme altyapısı.
- ✅ **Event System**: Subsystemler arası gevşek bağlı iletişim.

## 🛠️ Faz 2: Editör ve ECS Geçişi (Tamamlandı/Devam Ediyor)
- ✅ **ECS Entegrasyonu (EnTT)**: 
  - Custom ECS yerine EnTT kütüphanesi entegre edildi.
  - `Scene` ve `Entity` sınıfları oluşturuldu.
  - Component yapısı (Transform, Mesh, Material, Light, Camera) modernize edildi.
- ✅ **Editor UI (ImGui)**:
  - Docking arayüzü (Unreal Engine benzeri).
  - **Viewport**: 3D sahne render görüntüsü.
  - **World Outliner**: Sahne hiyerarşisi ve entity listesi.
  - **Details Panel**: Seçili entity özelliklerini görüntüleme (hazırlık aşamasında).
  - **Asset Browser**: (Temel hazırlık).
- ✅ **Vulkan RHI (Render Hardware Interface)**:
  - Vulkan backend soyutlaması.
  - Dinamik render pipeline oluşturma.
  - Shader yönetimi.

## 📅 Faz 3: İleri Render ve Fizik (Planlanan)
### Q1 2025
- [ ] **PBR Rendering**: Fiziksel tabanlı materyal sistemi (Metalness/Roughness workflow).
- [ ] **Shadow Mapping**: Directional ve Point light gölgelendirmeleri.
- [ ] **Post-Processing Stack**: Bloom, Tone Mapping, Color Grading.
- [ ] **Jolt Physics Entegrasyonu**: 
  - Rigidbody simülasyonu.
  - Collider componentleri (Box, Sphere, Capsule).

## 🧠 Faz 4: Oynanış ve Scripting
- [ ] **C# Scripting (Mono)**: Unity benzeri script yapısı.
- [ ] **Prefab Sistemi**: Entity şablonları oluşturma ve instance alma.
- [ ] **Input Mapping**: Gelişmiş aksiyon tabanlı girdi sistemi.

## 🎨 Faz 5: Araçlar ve Polish
- [ ] **Material Editor**: Node-based materyal düzenleyici.
- [ ] **Animation Controller**: State machine tabanlı animasyon sistemi.
- [ ] **Profiler**: CPU/GPU performans analiz araçları.

---
*Son Güncelleme: 22.12.2024*
