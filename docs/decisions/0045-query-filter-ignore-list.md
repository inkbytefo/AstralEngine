# ADR-0045 — Fizik sorgularında birden çok yok sayılan gövde

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `physics` (`QueryFilter`), `script` (`ScriptQueryFilter`, `astral.physics.raycast/spherecast`), `app` (`AppScriptPhysics`)
- **İlişkili:** [ADR-0038](0038-physics-api-v2.md), [G1 planı](../archive/legacy/superpowers/plans/2026-11-25-g1-engine-requests.md)

## Bağlam

`QueryFilter` yalnız tek bir `BodyHandle ignore` alıyordu. Bir oyuncunun kendi gövdesini, taşıdığı nesneyi ve aracını aynı anda dışarıda bırakan bir ışın atmak mümkün değildi. Luau tarafında `options.ignore` tek entity'ydi ve sensörler hiç görülemiyordu.

## Seçenekler

- **Sabit boyutlu dizi (ör. 4 gövde):** sahiplik sorunu yok ama keyfi bir sınır koyar.
- **`std::vector<BodyHandle>`:** her sorguda tahsis ve kopya; `RayQuery` toplu sorgularında maliyet.
- **Sahiplenmeyen `std::span<const BodyHandle> ignore_list` (seçilen).**

## Karar

- `QueryFilter` alanları: `layers`, `ignore` (geriye uyum için korunur), `ignore_list` (sahiplenmeyen span), `include_sensors`. Span, sorgu çağrısı boyunca (toplu sorguda `raycast_batch` dönene kadar) geçerli olmalıdır.
- Jolt tarafında `RayBodyFilter` iki kaynağı birleştirip `JPH::BodyID` listesi üzerinden süzer.
- Script katmanı için `ScriptQueryFilter { layers, std::span<const Entity> ignore, bool sensors }` ve `ScriptPhysics::raycast_filtered / spherecast_filtered`. Bunlar varsayılan gövdeli sanal fonksiyonlardır: eski `ScriptPhysics` uygulamaları derlenmeye devam eder (varsayılan yalnız ilk ignore'u kullanır, sensörleri görmez); App uygulaması tam filtreyi uygular.
- Luau: `options.ignore` tek entity ya da en çok 64 entity'lik liste; `options.sensors = true`.

## Sonuçlar

- Mevcut C++ ve Luau çağrıları değişmeden çalışır.
- Overlap sorguları (`overlap_sphere/box`) bu değişikliğe dahil edilmedi; Luau'da `ignore` hâlâ okunup kullanılmıyor.
- Test: `modules/app/tests/physics_angular_test.cpp` (C++ `ignore_list`, sensörler, Luau listesi).
