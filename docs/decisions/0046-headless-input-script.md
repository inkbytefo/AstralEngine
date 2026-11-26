# ADR-0046 — Başsız çalışmada betikli girdi (`AppDesc::input_script`)

- **Durum:** Kabul edildi
- **Tarih:** 2026-11-25
- **Kapsam:** `app` (`AppDesc`, `App` ana döngüsü), `platform` (`Input`)

## Bağlam

`Context::input` `const Input&` ve pencere yokken hiçbir şey tuş bildirmiyor. Oyunların "restart tuşuna basılınca" gibi dallarını başsız testte çalıştırmanın yolu yoktu.

## Seçenekler

- **`App::test_input()` ile dışarıdan döngüye müdahale:** test kodunun her kareyi elle yönetmesini gerektirir, `run()` ile çalışmaz.
- **`AppDesc` içinde veri olarak betik (seçilen):** deterministik, `run()` ve `run_frames()` ile çalışır, global durum yok.

## Karar

- `InputScriptEvent { std::uint64_t frame; Key key; bool down; }` ve `AppDesc::input_script` (`std::span<const InputScriptEvent>`). App olayları kopyalar ve kareye göre sıralar; her karenin başında o kareye ait olanları ham girdiye uygular.
- Pencere yoksa (headless, offscreen) App her kare kendisi `Input::begin_frame()` çağırır; böylece `pressed/released` kenar olarak çalışır.

## Sonuçlar

- Yalnız klavye tuşları desteklenir; fare, gamepad ve eksenler eklenmedi.
- `AppDesc` bu alanla 25 veri üyesi sınırına ulaştı (bkz. [ADR-0030](0030-refactor-rules-and-size-budget.md)); yeni ayarlar için gruplama gerekir.
- Test: `modules/app/tests/input_script_test.cpp`.
