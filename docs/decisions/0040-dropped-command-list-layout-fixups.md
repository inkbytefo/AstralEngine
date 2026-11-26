# ADR-0040 — Bırakılan CommandList'lerin layout düzeltmesi

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `gpu` modülü, MSR G03 / G02
- **İlişkili:** [Master Stabilization Roadmap](../archive/msr/master-stabilization-roadmap.md), [GPU/RHI denetimi](../archive/audits/gpu-rhi-deep-audit-2026-11-25.md)

## Bağlam

Texture durumu kayıt anında izlenir: `CommandList::transition` texture'ın global durumunu hemen günceller ve sonraki listeler barrier'larının `oldLayout`'unu bu durumdan seçer. Submit edilmeden bırakılan (drop) bir liste, değiştirdiği durumları geri alır. Aynı anda birden fazla liste kaydedilebildiği için şu sıra hatalıydı:

1. A, X'i `ColorTarget`'a geçirir.
2. B, X'i `ShaderRead`'e geçirir; barrier `ColorTarget → ShaderRead` olur.
3. A bırakılır ve X'i `Undefined`'a geri alır.
4. B submit edilir; GPU'da X hiç `ColorTarget` olmadığı halde barrier oradan başlar.

## Seçenekler

- **Tek aktif kayıt:** ikinci `begin_commands` reddedilir. Basit, ama mevcut public sözleşmeyi (birden fazla açık liste) ve testleri bozar.
- **Submit anında durum çözümü:** her liste yalnız yerel durum tutar, barrier'lar submit'te üretilir. Doğru ama kayıt modelini baştan değiştirir.
- **Bırakmada fix-up barrier (seçilen):** bırakılan listenin geçişini, ondan sonra aynı texture'a dokunmuş açık bir liste varsa, bir sonraki submit'in önüne eklenen ayrı bir komut tamponunda çalıştır.

## Karar

- Her `CommandList` benzersiz bir `serial_` alır. Texture kaydı, onu en son geçiren listenin serial'ını (`last_recorder`) tutar. Liste her texture için ilk (`before`) ve son (`after`) durumunu saklar.
- Liste bırakıldığında, her texture için:
  - texture'ı en son bu liste geçirdiyse durum `before`'a geri alınır;
  - başka bir liste sonradan geçirdiyse durum olduğu gibi kalır ve `before → after` geçişi `Device::Impl::layout_fixups` kuyruğuna eklenir.
- Bir sonraki başarılı submit, kuyruktaki geçişleri slot'un `fixup` komut tamponuna kaydeder. Bu tamponu ana tampondan önce aynı `vkQueueSubmit2` içinde gönderir ve ardından kuyruğu temizler. Submit başarısız olursa kuyruk korunur.
- Listelerin kayıt sırasıyla submit edilmesi sözleşmesi değişmez. Önce açılan bir listenin dokunduğu texture'a sonradan geçiş kaydeden bir liste, öncekinden önce submit edilirse sonuç tanımsızdır.

## Sonuçlar

- Public API değişmedi; `CommandList` yalnız private alan kazandı.
- Submit başına en fazla bir ek komut tamponu oluşur, o da yalnız bırakılan bir liste sonraki bir listeye temel olduysa.
- Kaynak ömrü tarafında (G02), kayıt sırasında `destroy` edilen bir kaynak, o an açık her liste submit ya da drop edilene kadar yaşar. Bu iki mekanizma birlikte "birden fazla açık liste" modelini korur.
- Test: `modules/gpu/tests/msr_regression_test.cpp` içindeki G03 senaryoları, Vulkan validation hatası olmadan geçmelidir.

## Not (2026-11-26)

Uygulamada kuyruk `Device::Impl::layout_fixups` adıyla değil, kayıt durumunun içinde `recording.fixups` olarak yaşar (`modules/gpu/src/vk_device.hpp:108`); `record_layout_fixups` onu bir sonraki gönderime yazar (`modules/gpu/src/vk_device.hpp:167-168`). Karar değişmedi.
