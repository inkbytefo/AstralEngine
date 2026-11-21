# ADR-0009: Render graph üzerinde hibrit deferred renderer

**Durum:** Kabul · 2026-11-21

**Bağlam:** Eski v2'de mesh + SDF hibrit deferred renderer vardı ama pass'ler, bariyerler ve kaynaklar
elle bağlanmıştı; yeni teknik eklemek her yere dokunmayı gerektiriyordu.

**Karar:**
- Tüm kare bir **render graph**'tan geçer: pass'ler erişimlerini bildirir, graph bariyerleri, render
  pass'leri, geçici texture havuzunu ve kullanılmayan pass ayıklamayı yapar.
- Renderer **deferred**: mesh'ler G-buffer'a, SDF'ler compute raymarch ile ayrı bir G-buffer'a yazılır;
  aydınlatma derinliğe göre birleştirir. Böylece mesh ve SDF aynı ışıkları, gölgeleri ve materyal
  modelini paylaşır, ve birbirini doğru örter.
- HDR ara hedefler formatsız storage image; aydınlatma compute'ta.

**Sonuçlar:** Yeni efekt = yeni pass (OCP). SDF maliyeti şekil sayısıyla doğrusal; brick bake ileride
aynı arayüzün arkasına eklenecek.
