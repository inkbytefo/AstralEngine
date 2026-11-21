# ADR-0013: Değişiklik takibi, reflection ve JSON serileştirme

**Durum:** Kabul · 2026-11-21 · Bağlam: [ADR-0012 §2](0012-render-api-contract.md)

**Karar:**
1. **Değişiklik = const olmayan erişim.** Satır başına `added`/`changed` tick'i; `each<T>`, `get<T>` ve değer
   değiştiren `add` işaretler. Neden `Mut<T>` sarmalayıcısı değil: API'yi her yerde değiştirirdi; `const` zaten
   C++'ta okuma niyetini söyler. Bedeli: yazmadan mutable alan erişim de "değişti" sayılır — belgelendi.
2. **Sistem başına referans** (Bevy modeli): App her sistemden önce referansı o sistemin son çalışma tick'ine
   ayarlar, sonra tick'i ilerletir. Kütüphane kullanımında `clear_trackers()`.
3. **Filtreler builder'da** (`query<...>().without<T>()...`): parametre paketine filtre tipleri karıştırmaktan
   okunaklı ve derlemesi hafif. `World::each` builder'ın kısa yolu.
4. **Olaylar iki güncelleme yaşar**, okuyucu kendi imlecini tutar: sınırsız kuyruk yok, sıra bağımlılığı yok.
5. **Reflection makroyla** (`ASTRAL_REFLECT`), kod üreteci yok. Kimlik makroya yazılan ad — derleyiciden
   bağımsız ve kalıcı (`component_name_hash` artık yalnız iç kullanım).
6. **JSON kendi kodumuz** (core): bağımlılık yok, public API'de yabancı tip yok; sahne yazımı için yeterli.
   Dağıtılan oyun ikili cook verisi yükleyecek (M6), JSON hızı kritik yol değil.
7. **Yükleme hoşgörülü**: bilinmeyen alan yok sayılır, eksik alan varsayılan kalır, hatalar listelenir — eski
   kayıtlar ve elle düzenlenmiş dosyalar kırılmaz.

**Sonuçlar:** Sahne/prefab (M5c), editör ve ağ aynı reflection'ı kullanacak. Runtime handle'lar serileşmez;
asset referansları M5b'de AssetId ile gelir.
