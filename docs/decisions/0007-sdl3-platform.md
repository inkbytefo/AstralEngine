# ADR-0007: Platform katmanı SDL3

**Durum:** Kabul · 2026-11-21

**Karar:** Pencere/input/(ileride ses, gamepad) için SDL3. GLFW değil.

**Neden:** Gamepad (eşleme veritabanı, hotplug, titreşim, gyro), ses, IME, high-DPI, haptik ve
ileride mobil/konsol desteği tek kütüphanede. GLFW sadece pencere + temel input.

**Sonuçlar:** Daha büyük bağımlılık (kaynaktan derleniyor, statik). SDL yalnızca
`modules/platform/src/` içinde; değiştirmek oyun kodunu etkilemez.
