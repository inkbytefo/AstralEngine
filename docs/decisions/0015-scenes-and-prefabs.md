# ADR-0015 — Sahneler ve prefab'lar

- **Durum:** Kabul
- **Tarih:** 2026-11-21
- **Bağlam:** ADR-0012 §3 (sahne ve dünya modeli), ADR-0013 (reflection ve JSON), ADR-0014 (asset'ler)

## Sorun

`save_world` dünyayı düz bir anlık görüntü olarak yazar: kimlikler kayıt sırasına göre verilir, tek bir
entity silinince sonraki bütün kimlikler kayar. Bir prefab örneği kaydedilince, prefab'la bağı kopmuş
düz entity'lere dönüşür. Sahne dosyası diff'lenebilir, elle ve yapay zekâyla yazılabilir olmalı. Prefab
düzenlemesi dünyadaki bütün örneklere yansımalı, örneğin kendi değişiklikleri de korunmalı.

## Karar

### 1. Biçim: `astral.scene` v1, sahne ve prefab için aynı

- Her kaydın kalıcı bir `id`'si var. Kimlik çalışma anında `SceneId` component'inde tutulur; tekrar
  kaydetmede aynı kalır, yeni entity'ler en büyük kimliğin devamını alır. Bir kaydı silmek diğer
  satırlara dokunmaz.
- Prefab, tek kökü (Parent'ı olmayan tek kaydı) olan bir sahnedir. Ayrı bir biçim yoktur.
- Örnek kaydı yalnızca farkları taşır: `prefab`, `components` (kök için alan bazlı yamalar), `removed`,
  `parts` (prefab içi yol → yama), `deleted`.
- Entity referansları kayıt kimliğiyle (`4`) ya da örnek içindeki yolla (`"2/5"`, iç içe `"2/4/3"`)
  yazılır. Bu sayede bir prefab'ın içindeki bir parçaya parent'lanmak da mümkün.
- Enum'lar isimle yazılır (`"SmoothUnion"`). Float'lar en kısa geri dönüşen ondalıkla (`0.6`), kısa
  sayı dizileri tek satırda yazılır. Amaç temiz diff.

### 2. Override = birleştirme yaması, kayıtta fark olarak hesaplanır

- Yükleme: prefab şablonu yerleştirilir, yamalar üstüne birleştirilir. Nesneler alan alan birleşir,
  diğer her şey (diziler dahil) bütün olarak değişir. `null` silme anlamına gelmez, bir değerdir (null
  entity, boş optional).
- Kaydetme: örneğin o anki durumu şablonla JSON düzeyinde karşılaştırılır, yalnızca farklar yazılır.
  Oyun ya da editör bir parçayı değiştirince kayıt bunu kendiliğinden override olarak yazar; ayrı bir
  "override işaretle" adımı yoktur (Unity'deki prefab modeli).
- Karşılaştırmadan önce iki taraf da kanonikleştirilir: tipe okunup geri yazılır (`TypeRegistry::Entry::transcode`).
  Eksik alanlar varsayılanla dolar, sayılar alan tipine yuvarlanır. Böylece sahte farklar oluşmaz.

### 3. Referansların yeniden yazımı tip bilgisiyle yapılır

JSON tipsizdir: bir sayının entity referansı mı yoksa sıradan bir değer mi olduğunu JSON tek başına
söyleyemez. Prefab iç içe yerleştirilince referansların öne ek alması gerekir. Bunun için yeni bir
şema yazılmadı; reflection kullanılıyor. Component tipine okunurken her referans geçici bir yer
tutucu entity'ye çevriliyor, yazarken eşlenmiş referans geri konuyor. Aynı mekanizma kanonikleştirmeyi
de yapıyor.

### 4. Çalışma anı modeli

- Örnek kökünde `PrefabInstance{path}` bulunur. Parçalarda `PrefabPart{instance, path}` bulunur;
  parçalar örnekle birlikte yok edilir ve kendi kayıtları olmaz.
- `PrefabLibrary` prefab'ı şablona (`PrefabTemplate`) açar: iç içe prefab'lar genişletilir, yamaları
  uygulanır. Şablon, kullandığı belgelerin sürümleriyle birlikte önbelleğe alınır. `refresh()`,
  belgesi değişen şablonları düşürür.
- `sync_prefabs` her kare çalışır. Kodla oluşturulan örnekleri (`world.create(Transform{}, PrefabInstance{...})`)
  açar. Şablonu değişen örneği önce eski şablona göre fark alarak, yeni şablon + farklar ile yeniden
  açar: hot-reload override'ları korur. Örnek dışındaki entity'lere giden referanslar ham tokenla
  (`"#indeks.nesil"`) taşınır.
- Kökün kendi component'leri prefab'ınkine üstün gelir. `world.create(Transform{pos}, PrefabInstance{...})`
  doğal olarak örneği yerleştirir.
- World modülü dosya sisteminden habersizdir: belgeleri `DocumentSource` geri çağrısıyla alır. App bunu
  `AssetManager::load_document` ile besler (senkron okuma, hot-reload, son sağlam sürümü koruma).

### 5. Varyant ve iç içe yerleştirme

- Varyant: prefab'ın kökü başka bir prefab'ın örneği olabilir. Varyantın parçaları temel prefab'ın
  yollarını aynen kullanır, böylece temeldeki override'lar varyantta da geçerli kalır. Varyantta
  çakışan bir kayıt kimliği hatadır.
- Kendini içeren prefab (a → b → a) hatadır; zincir hata mesajında gösterilir.

### 6. Sahneye yazılabilen mesh: `PrimitiveMesh`

`MeshRenderer` çalışma anı handle'ı tutar, bu yüzden dosyaya yazılamaz. `PrimitiveMesh{shape, MaterialDesc}`
yazılabilir bir karşılık: App bundan `MeshRenderer` üretir, aynı tanıma sahip materyalleri paylaştırır,
component değişince günceller. Dosyadan gelen modeller için `ModelRef` (M5b) kullanılmaya devam ediyor.

## Sonuçlar

- `save_world` / `load_world` düz anlık görüntü olarak kalıyor (kayıt oyunu, araçlar). Sahne ve prefab'lar
  `save_scene` / `load_scene` kullanıyor.
- `SaveContext` / `LoadContext` artık referansları geri çağrıyla çözüyor (önceden indeks tablosuydu).
  Bu kırıcı bir değişiklik, ancak yalnızca motorun kendi kodu bu bağlamları doğrudan kullanıyordu.
- Enum'ların isimle yazılması JSON çıktısını değiştirdi; okuma tarafı sayıyı da kabul etmeye devam ediyor.
- App'e offscreen render (`AppDesc::offscreen`, `App::read_frame`) eklendi. Sahne görüntüleri ve ileride
  görüntü testleri gerçek App yolundan geçiyor.

## Alternatifler

- **Yüklemede düzleştirme (bağ yok, Bevy'nin ilk sahne modeli):** basit, ama prefab düzenlemesi örneklere
  yayılmıyor ve kayıt şişiyor.
- **Açık override listesi (her alan için "override edildi" bayrağı):** editör gerektiriyor ve koddan
  yapılan değişiklikler kayboluyor. Fark hesaplama aynı sonucu bayraksız veriyor.
- **İsim yolları (Godot'taki `NodePath`):** okunaklı, ama yeniden adlandırma referansları kırıyor. Kalıcı
  kimlikler + bulma yardımcıları (`find_part`, `find_by_name`) tercih edildi.
- **Ayrı prefab biçimi:** gereksiz; tek kök kuralı yeterli.
