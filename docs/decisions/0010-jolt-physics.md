# ADR-0010: Fizik Jolt üzerine, ECS'ten bağımsız modül

**Durum:** Kabul · 2026-11-21

**Karar:** `Astral::Physics` Jolt Physics v5 kullanır; public API kendi tiplerimizle (handle, ShapeDesc,
BodyDesc, RayHit, ContactEvent). Modül ECS'i bilmez; App, `RigidBody`/`CharacterController`
component'leri ile köprü kurar ve fiziği sabit zaman adımıyla çalıştırır.

**Neden:** Jolt çok çekirdekli, deterministik seçeneği olan, AAA oyunlarda kullanılan bir motor;
karakter kontrolcüsü hazır. ECS'ten bağımsızlık fiziği headless test edilebilir ve değiştirilebilir kılar.

**Sonuçlar:** Jolt SSE4.2 ile (AVX kapalı) derlenir: eski işlemcilerde de çalışır. RTTI/exception
motorla tutarlı açık. Jolt'un süreç genelindeki tip kaydı referans sayımlı. JobSystem entegrasyonu ve
interpolasyon sonraki adım.
